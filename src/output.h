#pragma once

#include <functional>
#include <vector>
#include <map>
#include "uv_helper.h"
#include "compress.h"
#include "cso.h"
#include "sector.h"

namespace maxcso {

typedef std::function<void (int64_t pos, int64_t total, int64_t written)> OutputCallback;
typedef std::function<void (bool status, const char *reason)> OutputFinishCallback;

class Output {
public:
	Output(uv_loop_t *loop, const Task &task);
	~Output();

	void SetFile(uv_file file, int64_t srcSize, uint32_t blockSize, CSOFormat fmt);
	void Enqueue(int64_t pos, uint8_t *buffer);
	bool QueueFull();

	void OnProgress(OutputCallback callback);
	void OnFinish(OutputFinishCallback callback);

private:
	void CheckFinish();
	void Flush();
	int32_t Align(int64_t &pos);
	void HandleReadySector(Sector *sector);
	bool ShouldCompress(int64_t pos, uint8_t *buffer);
	inline int64_t SrcSizeAligned();

	enum State {
		STATE_INIT = 0x00,
		STATE_HAS_FILE = 0x01,
		STATE_INDEX_READY = 0x02,
		STATE_INDEX_WRITTEN = 0x04,
		STATE_DATA_WRITTEN = 0x08,
	};

	UVHelper uv_;
	uv_loop_t *loop_;
	uint32_t flags_;
	uint32_t state_;
	CSOFormat fmt_;
	double origMaxCostPercent_;
	double lz4MaxCostPercent_;

	uv_file file_;
	uv_fs_t req_;
	uv_fs_t flush_;

	int64_t srcSize_;
	int64_t srcPos_;
	int64_t dstPos_;

	uint32_t *index_;
	uint8_t indexShift_;
	uint32_t indexAlign_;
	uint32_t blockSize_;
	uint8_t blockShift_;

	OutputCallback progress_;
	OutputFinishCallback finish_;

	std::vector<Sector *> freeSectors_;
	std::map<int64_t, Sector *> pendingSectors_;
	std::unordered_map<uint32_t, Sector *> partialSectors_;
};

};
