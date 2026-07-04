#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <source_location>
#include <cstdint>

namespace flowhook
{
    enum class ErrorCode
    {
        // Task life cycls
        TASK_NOT_FOUND,
        TASK_ALREADY_RUNNING,
        TASK_NOT_RUNNING,
        TASK_FULL,
        TASK_NOT_ACTIVE,
        TASK_ALREADY_ACTIVE,
        TASK_ALREADY_EXISTS,

        // Config lookup errors
        PATH_NOT_FOUND,
        PATH_ALREADY_EXISTS,
        CONFIG_FILE_EMPTY,
        CONFIG_PARSE_FAILED,

        EVENT_NOT_FOUND,
        EVENT_NOT_SUPPORTED,
        COMMAND_NOT_FOUND,
        DUPLICATE_ENTRY,
        COMMAND_ALREADY_EXISTS,
        COMMAND_EMPTY,

        CALLBACK_NOT_FOUND,
        CALLBACK_ALREADY_EXISTS,

        // filewatcher errors
        FILEWATCHER_ALREADY_RUNNING,
        FILEWATCHER_NOT_RUNNING,
        FILEWATCHER_EMPTY,

        // session logger errors
        SESSION_LOGGER_ALREADY_RUNNING,
        SESSION_LOGGER_NOT_RUNNING,
        SESSION_LOGGER_UNINITIALIZED,

        // System/ OS Errors
        SYS_POLL_FAILED,
        SYS_PIPE_FAILED,
        SYS_IO_FAILED,
        SYS_THREAD_FAILED,
        SYS_ALLOC_FAILED,

        EMPTY_VALUE,
        VALUE_NOT_FOUND,
        INVALID_DEPTH,
        INVALID_TIMEOUT,

        UNKNOWN
    };

    struct StackFrame
    {
        std::string function;
        std::string file;
        uint32_t line;
    };

    struct FWError
    {
        ErrorCode code;
        std::string message;

        std::vector<StackFrame> stackTrace;

        FWError(ErrorCode code, std::string message) : code(code), message(message) {}

        void pushFrame(const std::source_location &loc)
        {
            stackTrace.push_back(StackFrame{
                loc.function_name(),
                loc.file_name(),
                loc.line()
            });
        }

        void printStackTrace() const
        {
            std::cerr << "  Trace (most recent last):\n";
            for (const auto &f : stackTrace)
                std::cerr << "    → " << f.function << " (" << f.file << ":" << f.line << ")\n";
        }

        static FWError make(ErrorCode code, std::string message, std::source_location loc = std::source_location::current())
        {
            FWError e(code, message);
            e.pushFrame(loc);
            return e;
        }
    };
}
