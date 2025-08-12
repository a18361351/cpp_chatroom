#ifndef LOG_MANAGER_HEADER
#define LOG_MANAGER_HEADER

#include <spdlog/spdlog.h>

class LoggerLevelSetter {
    public:
    LoggerLevelSetter() {
        spdlog::set_level(spdlog::level::debug);
    }
};
static LoggerLevelSetter l;

#endif 