#include <iostream>
#include <vector>
#include <string>
#include <iostream>
#include <chrono>
#include <sstream>
#include <ctime>
#include <filesystem>

#include "include/session_logger.h"
#include "error/error.h"
#include "include/macros.hpp"

namespace fs = std::filesystem;
// Error
using namespace std;
namespace flowhook
{

    Result<SessionLogger*> SessionLogger::create(const string &path)
    {
        FW_LOG("[DEBUG] Creating session logger ...");
        auto logger = new SessionLogger();
        TEST_OVERLOADED(logger->init(path), SessionLogger*);
        FW_LOG("[DEBUG] Session logger created successfully. ✓");
        return Result<SessionLogger*>::Ok(logger);
    }

    Result<void> SessionLogger::init(const string &path)
    {
        // check if file path is empty
        if (path.empty())
        {
            return Result<void>::Err(FWError::make(
                ErrorCode::EMPTY_VALUE, "Error: file path is empty " + path + ". ✗"));
        }
        file_path = path;
        FW_LOG("[DEBUG] Log file path set to: " + file_path);
        return Result<void>::Ok();
    }

    SessionLogger::~SessionLogger()
    {
        FW_LOG("[DEBUG] Session logger destroyed.");
        stop();
    }

    Result<void> SessionLogger::start()
    {
        if (is_running)
        {
            return Result<void>::Ok(); // idempotent
        }

        FW_LOG("[DEBUG] Starting a new log session ...");
        file.open(file_path, ios::out | ios::app);
        if (!file.is_open())
        {
            return Result<void>::Err(FWError::make(
                ErrorCode::SYS_IO_FAILED, "Error: opening log file " + file_path + ". ✗"));
        }
        session["task_name"] = fs::path(file_path).filename().string();
        session["working_directory"] = fs::path(file_path).parent_path().string();

        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&now_time), "%Y-%m-%dT%H:%M:%S");

        session["session-start-timestamp"] = ss.str();
        session["session_log"] = json::array();
        flushed = false;
        is_running = true;

        FW_LOG("[DEBUG] Log session started successfully.");
        return Result<void>::Ok();
    }

    Result<void> SessionLogger::log_execution(const ExecutionResult &execution_result)
    {
        if(!is_running)
        {
            return Result<void>::Err(FWError::make(
                ErrorCode::SESSION_LOGGER_NOT_RUNNING, "Error: session logger not initialized. ✗"));
        }
        if (session.empty())
        {
            return Result<void>::Err(FWError::make(
                ErrorCode::SESSION_LOGGER_NOT_RUNNING, "Error: session logger not initialized. ✗"));
        }
        WatchEvent e = execution_result._event;
        string terminal_msg = execution_result.log;
        vector<string> commands = execution_result.build_commands;
        int success_code = execution_result.id;

        json event;
        event["event_type"] = "modify";
        event["file_path"] = e.path;
        event["file_type"] = e.filetype;
        event["event_mask"] = e.event_mask;

        try
        {
            event["build-command"] = json::array();
            for (auto &cmd : commands)
            {
                event["build-command"].push_back(cmd);
            }
            event["success_code"] = success_code;
            event["terminal_msg"] = terminal_msg;

            auto now = std::chrono::system_clock::now();
            std::time_t now_time = std::chrono::system_clock::to_time_t(now);
            std::stringstream ss;

            // thread-safe time
            std::tm tm_buf;
            localtime_r(&now_time, &tm_buf); // POSIX, thread-safe
            ss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S");

            event["timestamp"] = ss.str();

            session["session_log"].push_back(event);

            FW_LOG("[DEBUG] An execution result logged successfully. ✓");
        }
        catch (const std::bad_alloc &e)
        {
            return Result<void>::Err(FWError::make(
                ErrorCode::SYS_ALLOC_FAILED, "Error: bad alloc error caught. ✗"));
        }
        return Result<void>::Ok();
}

    Result<void> SessionLogger::stop()
    {
        if (!is_running)
        {
            return Result<void>::Ok(); // idempotent
        }

        if (!file.is_open())
        {
            return Result<void>::Err(FWError::make(ErrorCode::SYS_IO_FAILED, "Error: couldn't open session logger file. ✗"));
        }

        if (!flushed)
        {
            auto now = std::chrono::system_clock::now();
            std::time_t now_time = std::chrono::system_clock::to_time_t(now);
            std::stringstream ss;
            ss << std::put_time(std::localtime(&now_time), "%Y-%m-%dT%H:%M:%S");

            session["session-end-timestamp"] = ss.str();
            try
            {
                file << session.dump(4) << endl;
            }
            catch (const std::bad_alloc &e)
            {
                return Result<void>::Err(FWError::make(
                    ErrorCode::SYS_ALLOC_FAILED, "Error: allocating memory for session log failed. ✗"));
            }
            if (file.fail())
            {
                return Result<void>::Err(FWError::make(
                    ErrorCode::SYS_IO_FAILED, "Error: writing to session log failed. ✗"));
            }
            file.close();
            if (file.fail())
            {
                return Result<void>::Err(FWError::make(
                    ErrorCode::SYS_IO_FAILED, "Error: closing session log failed. ✗"));
            }
            is_running = false;
            flushed = true;
        }
        FW_LOG("[DEBUG] Log session stopped successfully. ✓");
        return Result<void>::Ok();
    }
}
