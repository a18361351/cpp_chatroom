#ifndef LOG_MANAGER_HEADER
#define LOG_MANAGER_HEADER

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG

#include <spdlog/spdlog.h>

class LoggerLevelSetter {
    public:
    LoggerLevelSetter() {
        spdlog::set_level(spdlog::level::debug);
    }
};
static LoggerLevelSetter l;

#endif 