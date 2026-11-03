#pragma once

#include <vector>
#include <string>
#include <chrono>

#include "error/result.h"
#include "types.h"
#include "filewatcher.h"
#include "session_logger.h"

namespace flowhook
{
    class FileWatcher;
    class TaskRunner
    {
    private:
        FileWatcher *fw = nullptr;
        SessionLogger *sl = nullptr;
        Task task;
        std::vector<WatchCallback> callbacks;

        std::chrono::steady_clock::time_point last_executed;
        std::chrono::milliseconds cooldown_ms{500};

        bool flushed;
        bool is_init = false;
        int execution_id = 0;
        std::vector<std::string> resolved_files;

        TaskRunner() = default;
        Result<void> init(const std::string &task_name, const std::string &working_directory);
        Result<void> init_degraded(const std::string &task_name, const std::string &working_directory);
        Result<void> add_path_internal(const std::string &path, int MAX_DEPTH, int CURRENT_DEPTH);
        bool check_path_existence(const std::string &path);
    public:
        static Result<TaskRunner*> create(const std::string &task_name, const std::string &working_directory);
        static Result<TaskRunner*> create_degraded(const std::string &task_name, const std::string &working_directory);
        bool is_degraded() const;
        ~TaskRunner();

        std::string get_task_name() const { return task.name; }
        std::string get_task_id() const { return task.working_dir; }
        Task get_task() const { return task; }
        std::vector<std::string> get_resolved_files() const { return resolved_files; }
        int get_build_timeout() const { return task.build_timeout; }

        // adopts the uuid recorded in the persisted config; identity always
        // flows from config, never from the freshly stamped runner value
        void adopt_uuid(const std::string &uuid) { task.uuid = uuid; }


        bool isIgnored(const std::string &path);

        bool is_active() const { return task.isActive; }
        void activate() { task.isActive = true; }
        void deactivate() { task.isActive = false; }
        bool is_running() const { return task.isRunning; }
        Result<void> set_depth(int num);
        Result<void> set_timeout(int timeout);

        Result<void> change_task_name(const std::string &task_name);
        Result<void> change_working_directory(const std::string &working_directory);
        Result<void> add_command(const std::string &command);
        Result<void> delete_command(const std::string &command);

        Result<void> add_on_success(const std::string &command);
        Result<void> delete_on_success(const std::string &command);

        Result<void> add_on_failure(const std::string &command);
        Result<void> delete_on_failure(const std::string &command);

        Result<void> add_path(const std::string &path);
        Result<void> delete_path(const std::string &path);

        Result<void> add_ignored_path(const std::string &path);
        Result<void> add_ignored_pattern(const std::string &pattern);
        Result<void> remove_ignored_path(const std::string &path);
        Result<void> remove_ignored_pattern(const std::string &pattern);

        Result<void> add_callback(const WatchCallback &callback);
        Result<void> delete_callback(const WatchCallback &callback);

        Result<std::vector<std::string>> get_watch_list();

        Result<void> execute(const WatchEvent &e);
        Result<void> start();
        Result<void> stop();
    };
}
