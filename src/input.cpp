#include "input.h"
#include "buffer_pool.h"
#include "cso.h"
#include "dax.h"
#include "lz4.h"
#define ZLIB_CONST
#include "zlib.h"

namespace maxcso {

Input::Input(uv_loop_t *loop)
	: loop_(loop), type_(UNKNOWN), paused_(false), resumeShouldRead_(false), size_(-1), cache_(nullptr),
	csoIndex_(nullptr), daxSize_(nullptr), daxIsNC_(nullptr) {
}

Input::~Input() {
	delete [] cache_;
	cache_ = nullptr;
	delete [] csoIndex_;
	csoIndex_ = nullptr;
	delete [] daxSize_;
	daxSize_ = nullptr;
	delete [] daxIsNC_;
	daxIsNC_ = nullptr;
}

void Input::OnFinish(InputFinishCallback finish) {
	finish_ = finish;
}

void Input::OnBegin(InputBeginCallback begin) {
	begin_ = begin;
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

		const bool isZSO = !memcmp(headerBuf, ZSO_MAGIC, 4);
		if (isZSO || !memcmp(headerBuf, CSO_MAGIC, 4)) {
			const CSOHeader *const header = reinterpret_cast<CSOHeader *>(headerBuf);
			if (isZSO) {
				type_ = ZSO;
			} else {
				type_ = header->version == 2 ? CSO2 : CSO1;
			}
			if (header->version > 2) {
				finish_(false, "CSO header indicates unsupported version");
			} else if (header->sector_size < SECTOR_SIZE || header->sector_size > pool.bufferSize) {
				finish_(false, "CSO header indicates unsupported sector size");
			} else if ((header->uncompressed_size & SECTOR_MASK) != 0) {
				finish_(false, "CSO uncompressed size not aligned to sector size");
			} else {
				size_ = header->uncompressed_size;
				csoIndexShift_ = header->index_shift;
				csoBlockSize_ = header->sector_size;

				csoBlockShift_ = 0;
				for (uint32_t i = header->sector_size; i > 1; i >>= 1) {
					++csoBlockShift_;
				}

				const uint32_t sectors = static_cast<uint32_t>(SizeAligned() >> csoBlockShift_);
				csoIndex_ = new uint32_t[sectors + 1];
				const unsigned int bytes = (sectors + 1) * sizeof(uint32_t);
				const uv_buf_t buf = uv_buf_init(reinterpret_cast<char *>(csoIndex_), bytes);
				SetupCache(header->sector_size);

				uv_.fs_read(loop_, &req_, file_, &buf, 1, sizeof(CSOHeader), [this, bytes](uv_fs_t *req) {
					if (req->result != bytes) {
						// Index wasn't all there, this file is corrupt.
						finish_(false, "Unable to read entire index");
						uv_fs_req_cleanup(req);
						return;
					}
					uv_fs_req_cleanup(req);

					begin_(size_);
					ReadSector();
				});
			}
		} else if (!memcmp(headerBuf, DAX_MAGIC, 4)) {
			type_ = DAX;
			const DAXHeader *const header = reinterpret_cast<DAXHeader *>(headerBuf);
			if (header->version > 1) {
				finish_(false, "DAX header indicates unsupported version");
			} else if ((header->uncompressed_size & SECTOR_MASK) != 0) {
				finish_(false, "DAX uncompressed size not aligned to sector size");
			} else {
				size_ = header->uncompressed_size;

				const uint32_t frames = static_cast<uint32_t>((size_ + DAX_FRAME_SIZE - 1) >> DAX_FRAME_SHIFT);
				daxIndex_ = new uint32_t[frames];
				daxSize_ = new uint16_t[frames];
				daxIsNC_ = new bool[frames];
				memset(daxIsNC_, 0, sizeof(bool) * frames);

				uv_buf_t bufs[3];
				int nbufs = 2;
				uint32_t bytes = frames * (sizeof(uint32_t) + sizeof(uint16_t));
				bufs[0] = uv_buf_init(reinterpret_cast<char *>(daxIndex_), frames * sizeof(uint32_t));
				bufs[1] = uv_buf_init(reinterpret_cast<char *>(daxSize_), frames * sizeof(uint16_t));

				int nareas = header->version >= 1 ? header->nc_areas : 0;
				DAXNCArea *areas = nareas > 0 ? new DAXNCArea[nareas] : nullptr;
				if (areas != nullptr) {
					bufs[nbufs++] = uv_buf_init(reinterpret_cast<char *>(areas), nareas * sizeof(DAXNCArea));
					bytes += nareas * sizeof(DAXNCArea);
				}
				SetupCache(DAX_FRAME_SIZE);

				uv_.fs_read(loop_, &req_, file_, bufs, nbufs, sizeof(DAXHeader), [this, bytes, areas, nareas](uv_fs_t *req) {
					if (req->result != bytes) {
						// Index wasn't all there, this file is corrupt.
						finish_(false, "Unable to read entire index");
						uv_fs_req_cleanup(req);
						return;
					}
					uv_fs_req_cleanup(req);

					// Map the areas to an index and free.
					for (int i = 0; i < nareas; ++i) {
						for (uint32_t frame = 0; frame < areas[i].count; ++frame) {
							daxIsNC_[areas[i].start + frame] = true;
						}
					}
					delete [] areas;

					begin_(size_);
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

					SetupCache(SECTOR_SIZE);
					begin_(size_);
					ReadSector();
				}
			});
		}
		pool.Release(headerBuf);
	});
}

void Input::SetupCache(uint32_t minSize) {
	const uint32_t STANDARD_SIZE = 32768;
	while (minSize < STANDARD_SIZE) {
		minSize <<= 1;
	}

	cachePos_ = size_;
	cacheSize_ = minSize;
	cache_ = new uint8_t[cacheSize_];
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

	// Position of data in file.
	int64_t pos = pos_;
	// Offset into position where our data is.
	uint32_t offset = 0;
	unsigned int len = SECTOR_SIZE;
	bool compressedDeflate = false;
	bool compressedLZ4 = false;
	switch (type_) {
	case ISO:
		break;
	case CSO1:
	case CSO2:
	case ZSO:
		{
			const uint32_t block = static_cast<uint32_t>(pos_ >> csoBlockShift_);
			const uint32_t index = csoIndex_[block];
			const uint32_t nextIndex = csoIndex_[block + 1];

			pos = static_cast<uint64_t>(index & 0x7FFFFFFF) << csoIndexShift_;
			const int64_t nextPos = static_cast<uint64_t>(nextIndex & 0x7FFFFFFF) << csoIndexShift_;
			len = static_cast<unsigned int>(nextPos - pos);
			offset = pos_ & (csoBlockSize_ - 1);

			switch (type_) {
			case CSO1:
				compressedDeflate = (index & CSO_INDEX_UNCOMPRESSED) == 0;
				break;
			case CSO2:
				// In v2, only smaller than csoBlockSize_ is compressed.  Flags means how.
				if (index & CSO2_INDEX_LZ4) {
					compressedLZ4 = len < csoBlockSize_;
				} else {
					compressedDeflate = len < csoBlockSize_;
				}
				break;
			case ZSO:
				compressedLZ4 = (index & CSO_INDEX_UNCOMPRESSED) == 0;
				break;
			}

			if (!compressedDeflate && !compressedLZ4 && offset != 0) {
				pos += offset;
				offset = 0;
			}
		}
		break;
	case DAX:
		{
			const uint32_t frame = static_cast<uint32_t>(pos_ >> DAX_FRAME_SHIFT);
			pos = daxIndex_[frame];
			len = daxSize_[frame];
			compressedDeflate = !daxIsNC_[frame];
			offset = pos_ & DAX_FRAME_MASK;

			if (!compressedDeflate && offset != 0) {
				pos += offset;
				offset = 0;
			}
		}
		break;
	}

	// This ends up being owned by the compressor.
	if (pos >= cachePos_ && pos + len <= cachePos_ + cacheSize_) {
		// Already read in, let's just reuse.
		if (compressedDeflate || compressedLZ4) {
			EnqueueDecompressSector(cache_ + pos - cachePos_, len, offset, compressedLZ4);
		} else {
			uint8_t *readBuf = pool.Alloc();
			memcpy(readBuf, cache_ + pos - cachePos_, len);
			callback_(pos_, readBuf);

			pos_ += SECTOR_SIZE;
			ReadSector();
		}
	} else {
		const uv_buf_t buf = uv_buf_init(reinterpret_cast<char *>(cache_), cacheSize_);
		cachePos_ = pos;
		uv_.fs_read(loop_, &req_, file_, &buf, 1, pos, [this, len, offset, compressedDeflate, compressedLZ4](uv_fs_t *req) {
			if (req->result < static_cast<ssize_t>(len)) {
				finish_(false, "Unable to read entire sector");
				uv_fs_req_cleanup(req);
				return;
			}
			uv_fs_req_cleanup(req);

			if (compressedDeflate || compressedLZ4) {
				EnqueueDecompressSector(cache_, len, offset, compressedLZ4);
			} else {
				uint8_t *readBuf = pool.Alloc();
				memcpy(readBuf, cache_, len);
				callback_(pos_, readBuf);

				pos_ += SECTOR_SIZE;
				ReadSector();
			}
		});
	}
}

void Input::EnqueueDecompressSector(uint8_t *src, uint32_t len, uint32_t offset, bool isLZ4) {
	// We swap this with the compressed buf, and free the readBuf.
	uint8_t *const actualBuf = pool.Alloc();
	decompressError_.clear();
	uv_.queue_work(loop_, &work_, [this, actualBuf, src, len, isLZ4](uv_work_t *req) {
		bool result;
		if (isLZ4) {
			result = DecompressSectorLZ4(actualBuf, src, csoBlockSize_, decompressError_);
		} else {
			result = DecompressSectorDeflate(actualBuf, src, len, type_, decompressError_);
		}
		if (!result) {
			if (decompressError_.empty()) {
				decompressError_ = "Unknown error";
			}
		}
	}, [this, actualBuf, offset](uv_work_t *req, int status) {
		if (offset != 0) {
			memmove(actualBuf, actualBuf + offset, SECTOR_SIZE);
		}

		if (!decompressError_.empty()) {
			finish_(false, decompressError_.c_str());
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

bool Input::DecompressSectorDeflate(uint8_t *dst, const uint8_t *src, unsigned int len, FileType type, std::string &err) {
	z_stream z;
	memset(&z, 0, sizeof(z));
	// TODO: inflateReset2?
	if (inflateInit2(&z, type == DAX ? 15 : -15) != Z_OK) {
		err = z.msg ? z.msg : "Unable to initialize inflate";
		return false;
	}

	z.avail_in = len;
	z.next_out = dst;
	z.avail_out = pool.bufferSize;
	z.next_in = src;

	const int status = inflate(&z, Z_FINISH);
	if (status != Z_STREAM_END) {
		err = z.msg ? z.msg : "Inflate failed";
		inflateEnd(&z);
		return false;
	}

	if (z.total_out < SECTOR_SIZE) {
		err = "Expected to decompress into at least a full sector";
		inflateEnd(&z);
		return false;
	}

	inflateEnd(&z);
	return true;
}

bool Input::DecompressSectorLZ4(uint8_t *dst, const uint8_t *src, int dstSize, std::string &err) {
	// Must use fast, because we don't know the size of the input data.  It could include padding.
	if (LZ4_decompress_fast(reinterpret_cast<const char *>(src), reinterpret_cast<char *>(dst), dstSize) < 0) {
		err = "LZ4 decompression failed.";
		return false;
	}
	return true;
}

inline int64_t Input::SizeAligned() {
	return size_ + csoBlockSize_ - 1;
}

};