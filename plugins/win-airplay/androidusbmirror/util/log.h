#ifndef LOG_H
#define LOG_H

enum sc_log_level {
	SC_LOG_LEVEL_DEBUG,
	SC_LOG_LEVEL_INFO,
	SC_LOG_LEVEL_WARN,
	SC_LOG_LEVEL_ERROR,
};

#define LOGV(...)
#define LOGD(...)
#define LOGI(...)
#define LOGW(...)
#define LOGE(...)
#define LOGC(...)

#endif
