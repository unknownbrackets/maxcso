#include "uv_helper.h"

namespace maxcso {

UVHelper g_helper;

int UVHelper::ticket_;
int UVHelper::mutexInit_ = 0;
uv_mutex_t UVHelper::mutex_;
std::unordered_map<int, fs_func_cb> UVHelper::fs_funcs_;
std::unordered_map<int, std::pair<work_func_cb, after_work_func_cb> > UVHelper::work_funcs_;

};
