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

	int fs_open(uv_loop_t *loop, uv_fs_t *req, const char *path, int flags, int mode, fs_func_cb cb) {
		req->data = Freeze(cb);
		return uv_fs_open(loop, req, path, flags, mode, &Dispatch);
	}

	int fs_close(uv_loop_t *loop, uv_fs_t *req, uv_file file, fs_func_cb cb) {
		req->data = Freeze(cb);
		return uv_fs_close(loop, req, file, &Dispatch);
	}

	int fs_read(uv_loop_t *loop, uv_fs_t *req, uv_file file, const uv_buf_t bufs[], unsigned int nbufs, int64_t offset, fs_func_cb cb) {
		req->data = Freeze(cb);
		return uv_fs_read(loop, req, file, bufs, nbufs, offset, &Dispatch);
	}

	int fs_write(uv_loop_t *loop, uv_fs_t *req, uv_file file, const uv_buf_t bufs[], unsigned int nbufs, int64_t offset, fs_func_cb cb) {
		req->data = Freeze(cb);
		return uv_fs_write(loop, req, file, bufs, nbufs, offset, &Dispatch);
	}

	int fs_fstat(uv_loop_t *loop, uv_fs_t *req, uv_file file, fs_func_cb cb) {
		req->data = Freeze(cb);
		return uv_fs_fstat(loop, req, file, &Dispatch);
	}

	int queue_work(uv_loop_t *loop, uv_work_t *req, work_func_cb cb, after_work_func_cb after) {
		req->data = Freeze(cb, after);
		return uv_queue_work(loop, req, &Dispatch, &Dispatch);
	}

private:
	static void Dispatch(uv_fs_t *req) {
		fs_func_cb f = ThawFS(req->data);
		f(req);
	}

	static void *Freeze(fs_func_cb cb) {
		int ticket = ++ticket_;
		fs_funcs_[ticket] = cb;
		return (void *)ticket;
	}
	static fs_func_cb ThawFS(void *data) {
		int ticket = (int)data;
		fs_func_cb f = fs_funcs_[ticket];
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

	static void *Freeze(work_func_cb cb, after_work_func_cb after) {
		int ticket = ++ticket_;
		work_funcs_[ticket] = std::make_pair(cb, after);
		return (void *)ticket;
	}
	static work_func_cb ThawWork(void *data) {
		int ticket = (int)data;
		work_func_cb f = work_funcs_[ticket].first;
		return f;
	}
	static after_work_func_cb ThawAfterWork(void *data) {
		int ticket = (int)data;
		after_work_func_cb f = work_funcs_[ticket].second;
		work_funcs_.erase(ticket);
		return f;
	}

	static int ticket_;
	static std::unordered_map<int, fs_func_cb> fs_funcs_;
	static std::unordered_map<int, std::pair<work_func_cb, after_work_func_cb> > work_funcs_;
};

};
