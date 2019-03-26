#ifndef LOG_COMMON_H
#define LOG_COMMON_H

#include "logging.h" //google-glog

#define LOGD LOG(INFO)
#define LOGI LOG(INFO)
#define LOGW LOG(WARNING)
#define LOGE LOG(ERROR)
#define LOGF LOG(FATAL)

#endif
