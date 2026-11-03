#pragma once
#include <string>
#include <vector>


#include "task_runner.h"
#include "config_manager.h"
#include "error/result.h"
#include "types.h"

namespace flowhook {
    class FlowHookCore {
        private:
            ConfigManager* config_manager;
            std::vector<TaskRunner*> task_runners;

            std::vector<std::string> default_ignored_patterns;
            std::vector<std::string> default_ignored_paths;


            FlowHookCore() = default;

            bool isValidDir(std::string &path);
            Result<void> init();
            Result<void> lazy_init();
            Result<Task> find_task(const std::string &task_id);
        public:
            static Result<FlowHookCore*> create();
            static Result<FlowHookCore*> lazy_create();
            ~FlowHookCore();

            Result<void> set_default_ignored();
            Result<void> set_depth(const std::string &task_id, int depth);
            Result<void> set_timeout(const std::string &task_id, int timeout);
            Result<void> create_task(const std::string &task_name, const std::string &task_id);
            Result<void> delete_task(const std::string &task_id);
            bool is_task(const std::string &task_id);

            std::vector<std::string> get_resolved_files(const std::string task_id);
            Result<bool> is_task_active(const std::string &task_id);
            Result<int> get_task_depth(const std::string &task_id);
            Result<int> get_task_timeout(const std::string &task_id);


            Result<void> start_task(const std::string &task_id);
            Result<void> stop_task(const std::string &task_id);


            Result<void> set_task_path(const std::string &task_id, const std::string &path);
            Result<void> delete_task_path(const std::string &task_id, const std::string &path);

            Result<void> set_task_command(const std::string &task_id, const std::string &command);
            Result<void> delete_task_command(const std::string &task_id, const std::string &command);

            Result<void> set_task_on_success(const std::string &task_id, const std::string &command);
            Result<void> delete_task_on_success(const std::string &task_id, const std::string &command);
            Result<void> set_task_on_failure(const std::string &task_id, const std::string &command);
            Result<void> delete_task_on_failure(const std::string &task_id, const std::string &command);

            Result<void> set_ignored_path(const std::string &task_id, const std::string &path);
            Result<void> set_ignored_pattern(const std::string &task_id, const std::string &pattern);

            Result<void> remove_ignored_path(const std::string &task_id, const std::string &path);
            Result<void> remove_ignored_pattern(const std::string &task_id, const std::string &pattern);

            Result<void> start_all();
            Result<void> stop_all();

            Result<void> activate_task(const std::string &task_id);
            Result<void> deactivate_task(const std::string &task_id);

            Result<void> start_active();
            Result<void> stop_active();

            std::vector<Task> get_tasks() const;
            Result<std::vector<std::string>> get_watch_list(const std::string &task_id);
    };
}
