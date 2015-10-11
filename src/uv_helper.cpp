#include "uv_helper.h"

namespace maxcso {

UVHelper g_helper;

intptr_t UVHelper::nextTicket_ = 0;
int UVHelper::mutexInit_ = 0;
uv_mutex_t UVHelper::mutex_;
std::unordered_map<intptr_t, fs_func_cb> UVHelper::fs_funcs_;
std::unordered_map<intptr_t, std::pair<work_func_cb, after_work_func_cb> > UVHelper::work_funcs_;

};
