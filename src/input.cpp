#include <zlib.h>
#include "input.h"
#include "buffer_pool.h"

namespace maxcso {

static const char *CSO_MAGIC = "CISO";

static const uint32_t SECTOR_SIZE = 0x800;
static const uint32_t SECTOR_MASK = 0x7FF;
static const uint8_t SECTOR_SHIFT = 11;

// TODO: Endian-ify?
struct CSOHeader {
	char magic[4];
	uint32_t header_size;
	uint64_t uncompressed_size;
	uint32_t sector_size;
	uint8_t version;
	uint8_t index_shift;
	uint8_t unused[2];
};

Input::Input(uv_loop_t *loop)
	: loop_(loop), type_(UNKNOWN), paused_(false), resumeShouldRead_(false), size_(-1), csoIndex_(nullptr) {
}

Input::~Input() {
	delete [] csoIndex_;
	csoIndex_ = nullptr;
}

void Input::OnFinish(InputFinishCallback finish) {
	finish_ = finish;
}

void Input::Pipe(uv_file file, InputCallback callback) {
	file_ = file;
	callback_ = callback;
	pos_ = 0;

	// First, we need to check what format it is in.
	DetectFormat();
}

void Input::DetectFormat() {
	// CSO files will start with "CISO" magic, so let's try to read a header and see what we get.
	uint8_t *const headerBuf = pool.Alloc();
	const uv_buf_t buf = uv_buf_init(reinterpret_cast<char *>(headerBuf), 24);
	uv_.fs_read(loop_, &req_, file_, &buf, 1, 0, [this, headerBuf](uv_fs_t *req) {
		if (req->result != 24) {
			// ISOs are always sector aligned, and CSOs always have headers.
			finish_(false, "Not able to read first 24 bytes");
			uv_fs_req_cleanup(req);
			pool.Release(headerBuf);
			return;
		}
		uv_fs_req_cleanup(req);

		if (!memcmp(headerBuf, CSO_MAGIC, 4)) {
			type_ = CISO;
			const CSOHeader *const header = reinterpret_cast<CSOHeader *>(headerBuf);
			if (header->version > 1) {
				finish_(false, "CISO header indicates unsupported version");
			} else if (header->sector_size != SECTOR_SIZE) {
				finish_(false, "CISO header indicates unsupported sector size");
			} else if ((header->uncompressed_size & SECTOR_MASK) != 0) {
				finish_(false, "CISO uncompressed size not aligned to sector size");
			} else {
				size_ = header->uncompressed_size;
				csoShift_ = header->index_shift;

				const uint32_t sectors = static_cast<uint32_t>(size_ >> SECTOR_SHIFT);
				csoIndex_ = new uint32_t[sectors + 1];
				const unsigned int bytes = (sectors + 1) * sizeof(uint32_t);
				const uv_buf_t buf = uv_buf_init(reinterpret_cast<char *>(csoIndex_), bytes);

				uv_.fs_read(loop_, &req_, file_, &buf, 1, 24, [this, bytes](uv_fs_t *req) {
					if (req->result != bytes) {
						// Index wasn't all there, this file is corrupt.
						finish_(false, "Unable to read entire index");
						uv_fs_req_cleanup(req);
						return;
					}
					uv_fs_req_cleanup(req);

					ReadSector();
				});
			}
		} else {
			type_ = ISO;

			uv_.fs_fstat(loop_, &req_, file_, [this](uv_fs_t *req) {
				// An ISO can't be an uneven size, must align to sectors.
				if (req->result < 0 || (req->statbuf.st_size & SECTOR_MASK) != 0) {
					finish_(false, "ISO file not aligned to sector size");
					uv_fs_req_cleanup(req);
				} else {
					size_ = req->statbuf.st_size;
					uv_fs_req_cleanup(req);

					ReadSector();
				}
			});
		}
		pool.Release(headerBuf);
	});
}

void Input::ReadSector() {
	// At the end of the file, all done.
	if (pos_ >= size_) {
		finish_(true, nullptr);
		return;
	}

	if (paused_) {
		// When we resume, it'll need to call ReadSector() to resume.
		resumeShouldRead_ = true;
		return;
	}

	int64_t pos = pos_;
	unsigned int len = SECTOR_SIZE;
	bool compressed = false;
	switch (type_) {
	case ISO:
		break;
	case CISO:
		{
			const uint32_t sector = static_cast<uint32_t>(pos_ >> SECTOR_SHIFT);
			const uint32_t index = csoIndex_[sector];
			const uint32_t nextIndex = csoIndex_[sector + 1];
			compressed = (index & 0x80000000) == 0;

			pos = static_cast<uint64_t>(index & 0x7FFFFFFF) << csoShift_;
			const int64_t nextPos = static_cast<uint64_t>(nextIndex & 0x7FFFFFFF) << csoShift_;
			len = static_cast<unsigned int>(nextPos - pos);

			if (!compressed && len != SECTOR_SIZE) {
				finish_(false, "Uncompressed sector is not the expected size");
				return;
			}
		}
		break;
	}

	// This ends up being owned by the compressor.
	uint8_t *const readBuf = pool.Alloc();
	const uv_buf_t buf = uv_buf_init(reinterpret_cast<char *>(readBuf), len);
	uv_.fs_read(loop_, &req_, file_, &buf, 1, pos, [this, readBuf, len, compressed](uv_fs_t *req) {
		if (req->result != len) {
			finish_(false, "Unable to read entire sector");
			uv_fs_req_cleanup(req);
			return;
		}
		uv_fs_req_cleanup(req);

		if (compressed) {
			// We swap this with the compressed buf, and free the readBuf.
			uint8_t *const actualBuf = pool.Alloc();
			csoError_.clear();
			uv_.queue_work(loop_, &work_, [this, actualBuf, readBuf, len](uv_work_t *req) {
				if (!DecompressSector(actualBuf, readBuf, len, csoError_)) {
					if (csoError_.empty()) {
						csoError_ = "Unknown error";
					}
				}
			}, [this, actualBuf, readBuf](uv_work_t *req, int status) {
				pool.Release(readBuf);

				if (!csoError_.empty()) {
					finish_(false, csoError_.c_str());
					pool.Release(actualBuf);
				} else if (status == -1) {
					finish_(false, "Decompression work failed");
					pool.Release(actualBuf);
				} else {
					callback_(pos_, actualBuf);

					pos_ += SECTOR_SIZE;
					ReadSector();
				}
			});
		} else {
			callback_(pos_, readBuf);

			pos_ += SECTOR_SIZE;
			ReadSector();
		}
	});
}

void Input::Pause() {
	paused_ = true;
}

void Input::Resume() {
	paused_ = false;
	if (resumeShouldRead_) {
		resumeShouldRead_ = false;
		ReadSector();
	}
}

bool Input::DecompressSector(uint8_t *dst, const uint8_t *src, unsigned int len, std::string &err) {
	z_stream z;
	z.zalloc = Z_NULL;
	z.zfree = Z_NULL;
	z.opaque = Z_NULL;
	z.msg = Z_NULL;
	if (inflateInit2(&z, -15) != Z_OK) {
		err = z.msg ? z.msg : "Unable to initialize inflate";
		return false;
	}

	z.avail_in = len;
	z.next_out = dst;
	z.avail_out = BufferPool::BUFFER_SIZE;
	z.next_in = src;

	const int status = inflate(&z, Z_FULL_FLUSH);
	if (status != Z_STREAM_END) {
		err = z.msg ? z.msg : "Inflate failed";
		inflateEnd(&z);
		return false;
	}

	if (z.avail_out != BufferPool::BUFFER_SIZE - SECTOR_SIZE) {
		err = "Expected to decompress into a full sector";
		inflateEnd(&z);
		return false;
	}

	inflateEnd(&z);
	return true;
}

};