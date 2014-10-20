#include "output.h"
#include "buffer_pool.h"
#include "cso.h"

namespace maxcso {

Output::Output(uv_loop_t *loop) : loop_(loop), srcSize_(-1), index_(nullptr), writing_(false) {
}

Output::~Output() {
	delete [] index_;
	index_ = nullptr;
}

void Output::SetFile(uv_file file, int64_t srcSize) {
	file_ = file;
	srcSize_ = srcSize;
	srcPos_ = 0;

	const uint32_t sectors = static_cast<uint32_t>(srcSize >> SECTOR_SHIFT);
	// Start after the header and index, which we'll fill in later.
	index_ = new uint32_t[sectors + 1];
	// Start after the end of the index data and header.
	dstPos_ = sizeof(CSOHeader) + (sectors + 1) * sizeof(uint32_t);

	// TODO: We might be able to optimize shift better by running through the data.
	// That would require either a second pass or keeping the entire result in RAM.
	// For now, just take worst case (all blocks stored uncompressed.)
	int64_t worstSize = dstPos_ + srcSize;
	shift_ = 0;
	for (int i = 62; i >= 31; --i) {
		int64_t max = 1LL << i;
		if (worstSize >= max) {
			// This means we need i + 1 bits to store the position.
			// We have to shift enough off to fit into 31.
			shift_ = i + 1 - 31;
			break;
		}
	}

	// If the shift is above 11, the padding could make it need more space.
	// But that would be > 4 TB anyway, so let's not worry about it.
	align_ = 1 << shift_;
	Align();
}

void Output::Align() {
	uint32_t off = static_cast<uint32_t>(dstPos_ % align_);
	if (off != 0) {
		dstPos_ += align_ - off;
	}
}

static bool TESTING;

void Output::Enqueue(int64_t pos, uint8_t *sector) {
	// TODO: Serialize writes, queue, release sector
	// TODO: Don't compress certain blocks?  Like header, dirs?

	// For now, just for testing.
	int32_t s = static_cast<int32_t>(pos >> SECTOR_SHIFT);
	index_[s] = static_cast<int32_t>(dstPos_ >> shift_) | CSO_INDEX_UNCOMPRESSED;
	const uv_buf_t buf = uv_buf_init(reinterpret_cast<char *>(sector), SECTOR_SIZE);

	// TODO: Should really write multiple at once.  Wrong way...
	writing_ = true;
	uv_.fs_write(loop_, &req_, file_, &buf, 1, dstPos_, [this, sector](uv_fs_t *req) {
		writing_ = false;
		pool.Release(sector);
		if (req->result != SECTOR_SIZE) {
			finish_(false, "Unable to write data (disk space?)");
			uv_fs_req_cleanup(req);
			return;
		}
		uv_fs_req_cleanup(req);

		dstPos_ += SECTOR_SIZE;
		srcPos_ += SECTOR_SIZE;
		Align();

		// TODO: Better (including index?)
		float prog = static_cast<float>(srcPos_) / static_cast<float>(srcSize_);
		progress_(prog);
	});
}

bool Output::QueueFull() {
	// TODO: This is wrong, but just to make it not crash for now.
	return writing_;
}

void Output::OnProgress(OutputCallback callback) {
	progress_ = callback;
}

void Output::OnFinish(OutputFinishCallback callback) {
	finish_ = callback;
}

void Output::Flush() {
	CSOHeader *header = new CSOHeader;
	memcpy(header->magic, CSO_MAGIC, sizeof(header->magic));
	header->header_size = sizeof(CSOHeader);
	header->uncompressed_size = srcSize_;
	header->sector_size = SECTOR_SIZE;
	header->version = 1;
	header->index_shift = shift_;
	header->unused[0] = 0;
	header->unused[1] = 0;

	const uint32_t sectors = static_cast<uint32_t>(srcSize_ >> SECTOR_SHIFT);

	// TODO: Need to drain first before broadcasting finish.
	// TODO: Then need to update the final index.

	uv_buf_t bufs[2];
	bufs[0] = uv_buf_init(reinterpret_cast<char *>(header), sizeof(CSOHeader));
	bufs[1] = uv_buf_init(reinterpret_cast<char *>(index_), (sectors + 1) * sizeof(uint32_t));
	const size_t totalBytes = sizeof(CSOHeader) + (sectors + 1) * sizeof(uint32_t);
	uv_.fs_write(loop_, &flush_, file_, bufs, 2, 0, [this, header, totalBytes](uv_fs_t *req) {
		if (req->result != totalBytes) {
			printf("GOT: %lld not %lld\n", (uint64_t)req->result, (uint64_t)totalBytes);
			finish_(false, "Unable to write header data");
		} else {
			finish_(true, nullptr);
		}
		uv_fs_req_cleanup(req);
		delete header;
	});
}

};
