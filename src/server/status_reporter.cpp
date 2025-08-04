#ifndef SERVER_STATUS_REPORTER_HEADER
#define SERVER_STATUS_REPORTER_HEADER

// status_reporter.cpp: 后台服务器向状态服务器报告自身负载状态的类

#include <sys/types.h>

namespace chatroom {
    class StatusReporter {
        public:

        bool ReportLoad(uint current_load) {
            return false;
        }

        bool EmergencyReportLoad(uint current_load) {
            return false;
        }
    };
}

#endif