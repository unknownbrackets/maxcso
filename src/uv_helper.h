#pragma once

#include <functional>
#include <unordered_map>
#include "uv.h"

#ifdef _MSC_VER
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "iphlpapi.lib")
#endif

namespace maxcso {

typedef std::function<void (uv_fs_t *req)> fs_func_cb;
typedef std::function<void (uv_work_t *req)> work_func_cb;
typedef std::function<void (uv_work_t *req, int status)> after_work_func_cb;

class UVHelper {
public:
	UVHelper() {
	}

	inline int fs_open(uv_loop_t *loop, uv_fs_t *req, const char *path, int flags, int mode, fs_func_cb &&cb) {
		req->data = Freeze(std::move(cb));
		return uv_fs_open(loop, req, path, flags, mode, &Dispatch);
	}

	inline int fs_close(uv_loop_t *loop, uv_fs_t *req, uv_file file, fs_func_cb &&cb) {
		req->data = Freeze(std::move(cb));
		return uv_fs_close(loop, req, file, &Dispatch);
	}

	inline int fs_read(uv_loop_t *loop, uv_fs_t *req, uv_file file, const uv_buf_t bufs[], unsigned int nbufs, int64_t offset, fs_func_cb &&cb) {
		req->data = Freeze(std::move(cb));
		return uv_fs_read(loop, req, file, bufs, nbufs, offset, &Dispatch);
	}

	inline int fs_write(uv_loop_t *loop, uv_fs_t *req, uv_file file, const uv_buf_t bufs[], unsigned int nbufs, int64_t offset, fs_func_cb &&cb) {
		req->data = Freeze(std::move(cb));
		return uv_fs_write(loop, req, file, bufs, nbufs, offset, &Dispatch);
	}

	inline int fs_fstat(uv_loop_t *loop, uv_fs_t *req, uv_file file, fs_func_cb &&cb) {
		req->data = Freeze(std::move(cb));
		return uv_fs_fstat(loop, req, file, &Dispatch);
	}

	inline int queue_work(uv_loop_t *loop, uv_work_t *req, work_func_cb &&cb, after_work_func_cb &&after) {
		req->data = Freeze(std::move(cb), std::move(after));
		return uv_queue_work(loop, req, &Dispatch, &Dispatch);
	}

private:
	static void Dispatch(uv_fs_t *req) {
		fs_func_cb f = ThawFS(req->data);
		f(req);
	}

	inline static void *Freeze(fs_func_cb &&cb) {
		int ticket = ++ticket_;
		fs_funcs_.emplace(ticket, std::move(cb));
		return (void *)ticket;
	}
	inline static fs_func_cb ThawFS(void *data) {
		int ticket = (int)data;
		fs_func_cb f = std::move(fs_funcs_.at(ticket));
		fs_funcs_.erase(ticket);
		return f;
	}

	static void Dispatch(uv_work_t *req) {
		work_func_cb f = ThawWork(req->data);
		f(req);
	}
	static void Dispatch(uv_work_t *req, int status) {
		after_work_func_cb f = ThawAfterWork(req->data);
		f(req, status);
	}

	inline static void *Freeze(work_func_cb &&cb, after_work_func_cb &&after) {
		int ticket = ++ticket_;
		work_funcs_.emplace(ticket, std::move(std::make_pair(std::move(cb), std::move(after))));
		return (void *)ticket;
	}
	inline static work_func_cb ThawWork(void *data) {
		int ticket = (int)data;
		work_func_cb f = std::move(work_funcs_.at(ticket).first);
		return f;
	}
	inline static after_work_func_cb ThawAfterWork(void *data) {
		int ticket = (int)data;
		after_work_func_cb f = std::move(work_funcs_.at(ticket).second);
		work_funcs_.erase(ticket);
		return f;
	}

	static int ticket_;
	static std::unordered_map<int, fs_func_cb> fs_funcs_;
	static std::unordered_map<int, std::pair<work_func_cb, after_work_func_cb> > work_funcs_;
};

};
