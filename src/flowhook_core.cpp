#include <filesystem>
#include <string>
#include <vector>

#include "include/error/error.h"
#include "include/error/result.h"
#include "include/flowhook_core.h"
#include "include/macros.hpp"
#include "include/task_runner.h"

using namespace std;
namespace fs = std::filesystem;

namespace flowhook {

Result<FlowHookCore *> FlowHookCore::create() {
  FlowHookCore *core = new FlowHookCore();
  TEST_OVERLOADED(core->init(), FlowHookCore *);
  return Result<FlowHookCore *>::Ok(core);
}

Result<FlowHookCore *> FlowHookCore::lazy_create() {
    FlowHookCore *core = new FlowHookCore();
    TEST_OVERLOADED(core->lazy_init(), FlowHookCore *);
    return Result<FlowHookCore *>::Ok(core);
}

FlowHookCore::~FlowHookCore() {
  config_manager->flush();
  for (auto it = task_runners.begin(); it != task_runners.end(); it++) {
    delete (*it);
  }
}

Result<void> FlowHookCore::set_default_ignored() {
  default_ignored_patterns = {"*.o",   "*.a",   "*.so", "*.out", "*.exe",
                              "*.swp", "*.swo", "*~",   ".#*",   "*.class",
                              "*.pyc", "*.log", "*.git"};
  default_ignored_paths = {".git"};
  return Result<void>::Ok();
}

Result<void> FlowHookCore::lazy_init() {
    config_manager = TRY(ConfigManager::create(), void);
    FW_VERBOSE("[FLOWHOOK] a lazy Flowhook core instance initialized.");
    return Result<void>::Ok();
}

Result<void> FlowHookCore::init() {
  config_manager = TRY(ConfigManager::create(), void);
  FW_LOG("[DEBUG] initializing a FlowHook core instance ....");

  vector<Task> tasks = TRY(config_manager->get_tasks(), void);
  set_default_ignored();

  FW_LOG("[DEBUG] loading tasks from config file ....");
  for (auto task : tasks) {
    TaskRunner *tr = TRY(TaskRunner::create(task.name, task.id), void);
    for (auto i : task.ignored_paths)
      TEST(tr->add_ignored_path(i));
    for (auto ip : task.ignored_patterns)
      TEST(tr->add_ignored_pattern(ip));
    if (task.isActive) {
      tr->activate();
    } else {
      tr->deactivate();
    }
    tr->set_depth(task.watching_depth);
    tr->set_timeout(task.build_timeout);

    for (auto c : task.commands)
      TEST(tr->add_command(c));
    for (auto p : task.file_paths)
    {
        auto r = tr->add_path(p);
        if(r.isErr())
        {
            if(r.getErrCode() == ErrorCode::PATH_NOT_FOUND)
            {
                tr->delete_path(p);
            }
            else
            {
                return Result<void>::Err(r.unwrapErr());
            }
        }
    }
    for (auto p : task.dir_paths)
    {
        auto r = tr->add_path(p);
        if(r.isErr())
        {
            if(r.getErrCode() == ErrorCode::PATH_NOT_FOUND)
            {
                tr->delete_path(p);
            }
            else
            {
                return Result<void>::Err(r.unwrapErr());
            }
        }
    }
    for (auto s : task.on_success)
      TEST(tr->add_on_success(s));
    for (auto f : task.on_failure)
      TEST(tr->add_on_failure(f));

    task_runners.push_back(tr);
  }
  FW_LOG("[DEBUG] loading tasks from config file completed. ✓");
  FW_VERBOSE("[FLOWHOOK] Flowhook core initialized.");
  return Result<void>::Ok();
}

Result<void> FlowHookCore::create_task(const std::string &task_name,
                                       const std::string &task_id) {
  FW_LOG("[DEBUG] Creating task...");
  if (task_name.empty()) {
    return Result<void>::Err(FWError::make(
        ErrorCode::EMPTY_VALUE, "Error: task name cannot be empty ✗"));
  }
  for (auto it = task_runners.begin(); it != task_runners.end(); it++) {
    if ((*it) == nullptr)
      continue;
    string id = (*it)->get_task_id();
    if (id == task_id) {
      return Result<void>::Err(FWError::make(ErrorCode::DUPLICATE_ENTRY,
                                             "Error: task already exists ✗"));
    }
  }

  if (!fs::exists(task_id) || !fs::is_directory(task_id)) {
    return Result<void>::Err(FWError::make(
        ErrorCode::PATH_NOT_FOUND, "Error: working directory not found ✗"));
  }

  if (task_runners.size() >= 200) {
    return Result<void>::Err(
        FWError::make(ErrorCode::TASK_FULL, "Error: task limit reached ✗"));
  }
  auto result = TRY(TaskRunner::create(task_name, task_id), void);

  for (auto i : default_ignored_paths)
    result->add_ignored_path(i);
  for (auto ip : default_ignored_patterns)
    result->add_ignored_pattern(ip);

  task_runners.push_back(result);
  TEST(config_manager->log_task(result->get_task()));

  FW_LOG("[DEBUG] logging task to config file completed. ✓");
  FW_VERBOSE("[FLOWHOOK] Task created: name='" + task_name + "' path='" +
             task_id + "' ✓");
  return Result<void>::Ok();
}

Result<void> FlowHookCore::delete_task(const std::string &task_id) {
  FW_LOG("[DEBUG] Deleting task...");
  for (auto it = task_runners.begin(); it != task_runners.end(); it++) {
    string name = (*it)->get_task_id();
    if (name == task_id) {
      Task _task_to_be_deleted = (*it)->get_task();
      TEST(config_manager->delete_task(_task_to_be_deleted));
      task_runners.erase(it);
      FW_VERBOSE("[FLOWHOOK] Task deleted: " + task_id + "' ✓");
      return Result<void>::Ok();
    }
  }

  return Result<void>::Err(FWError::make(
      ErrorCode::TASK_NOT_FOUND, "Error: task not found " + task_id + " ✗"));
}

bool FlowHookCore::is_task(const std::string &task_id) {
  for (auto it = task_runners.begin(); it != task_runners.end(); it++) {
    if ((*it)->get_task_id() == task_id) {
      return true;
    }
  }
  return false;
}

Result<void> FlowHookCore::set_depth(const std::string &task_id, int depth) {
  for (auto it = task_runners.begin(); it != task_runners.end(); it++) {
    if ((*it)->get_task_id() == task_id) {
      TEST((*it)->set_depth(depth));
      TEST(config_manager->update_task((*it)->get_task()));
      return Result<void>::Ok();
    }
  }
  return Result<void>::Err(FWError::make(
      ErrorCode::TASK_NOT_FOUND, "Error: task not found " + task_id + " ✗"));
}

Result<void> FlowHookCore::set_timeout(const std::string &task_id, int timeout) {
  for (auto it = task_runners.begin(); it != task_runners.end(); it++) {
    if ((*it)->get_task_id() == task_id) {
      TEST((*it)->set_timeout(timeout));
      TEST(config_manager->update_task((*it)->get_task()));
      return Result<void>::Ok();
    }
  }
  return Result<void>::Err(FWError::make(
      ErrorCode::TASK_NOT_FOUND, "Error: task not found " + task_id + " ✗"));
}


std::vector<std::string>
FlowHookCore::get_resolved_files(const std::string task_id) {
  for (auto it = task_runners.begin(); it != task_runners.end(); it++) {
    if ((*it)->get_task_id() == task_id) {
      return (*it)->get_resolved_files();
    }
  }
  return std::vector<std::string>();
}

Result<int> FlowHookCore::get_task_depth(const std::string &task_id) {
  for (auto it = task_runners.begin(); it != task_runners.end(); it++) {
    if ((*it)->get_task_id() == task_id) {
      auto t = (*it)->get_task();
      return Result<int>::Ok(t.watching_depth);
    }
  }
  return Result<int>::Err(FWError::make(
      ErrorCode::TASK_NOT_FOUND, "Error: task not found " + task_id + " ✗"));
}

Result<int> FlowHookCore::get_task_timeout(const std::string &task_id) {
  for (auto it = task_runners.begin(); it != task_runners.end(); it++) {
    if ((*it)->get_task_id() == task_id) {
      int t = (*it)->get_build_timeout();
      return Result<int>::Ok(t);
    }
  }
  return Result<int>::Err(FWError::make(
      ErrorCode::TASK_NOT_FOUND, "Error: task not found " + task_id + " ✗"));
}


Result<bool> FlowHookCore::is_task_active(const std::string &task_id) {
  for (auto it = task_runners.begin(); it != task_runners.end(); it++) {
    if ((*it)->get_task_id() == task_id) {
      return Result<bool>::Ok((*it)->is_active());
    }
  }
  return Result<bool>::Err(FWError::make(
      ErrorCode::TASK_NOT_FOUND, "Error: task not found " + task_id + " ✗"));
}

Result<void> FlowHookCore::activate_task(const std::string &task_id) {
  for (auto it = task_runners.begin(); it != task_runners.end(); it++) {
    string id = (*it)->get_task_id();
    if (id == task_id) {
      (*it)->activate();
      TEST(config_manager->update_task((*it)->get_task()));
      FW_VERBOSE("[FLOWHOOK] Task activated: " + task_id + " ✓");
      return Result<void>::Ok();
    }
  }

  return Result<void>::Err(FWError::make(
      ErrorCode::TASK_NOT_FOUND, "Error: task not found " + task_id + " ✗"));
}

Result<void> FlowHookCore::deactivate_task(const std::string &task_id) {
  for (auto it = task_runners.begin(); it != task_runners.end(); it++) {
    string id = (*it)->get_task_id();
    if (id == task_id) {
      (*it)->deactivate();
      TEST(config_manager->update_task((*it)->get_task()));
      FW_VERBOSE("[FLOWHOOk] Task deactivated: " + task_id + " ✓");
      return Result<void>::Ok();
    }
  }

  return Result<void>::Err(FWError::make(
      ErrorCode::TASK_NOT_FOUND, "Error: task not found " + task_id + " ✗"));
}

Result<void> FlowHookCore::set_task_path(const std::string &task_id,
                                         const std::string &path) {
  if (!fs::exists(path)) {
    return Result<void>::Err(FWError::make(
        ErrorCode::PATH_NOT_FOUND, "Error: path not found " + path + " ✗"));
  }
  for (auto it = task_runners.begin(); it != task_runners.end(); it++) {
    string id = (*it)->get_task_id();
    if (id == task_id) {
      TEST((*it)->add_path(path));
      TEST(config_manager->update_task((*it)->get_task()));
      FW_LOG("[DEBUG] Task path set: " + task_id + " ✓");
      return Result<void>::Ok();
    }
  }

  return Result<void>::Err(FWError::make(
      ErrorCode::TASK_NOT_FOUND, "Error: task not found " + task_id + " ✗"));
}

Result<void> FlowHookCore::delete_task_path(const std::string &task_id,
                                            const std::string &path) {
  for (auto it = task_runners.begin(); it != task_runners.end(); it++) {
    string id = (*it)->get_task_id();
    if (id == task_id) {
      TEST((*it)->delete_path(path));
      TEST(config_manager->update_task((*it)->get_task()));
      FW_LOG("[DEBUG] Task path deleted: " + task_id + " ✓");
      return Result<void>::Ok();
    }
  }

  return Result<void>::Err(FWError::make(
      ErrorCode::TASK_NOT_FOUND, "Error: task not found " + task_id + " ✗"));
}

Result<void> FlowHookCore::set_ignored_path(const std::string &task_id,
                                            const std::string &path) {
  for (auto it = task_runners.begin(); it != task_runners.end(); it++) {
    string id = (*it)->get_task_id();
    if (id == task_id) {
      TEST((*it)->add_ignored_path(path));
      TEST(config_manager->update_task((*it)->get_task()));
      FW_LOG("[DEBUG] Task ignore path set: " + task_id + " ✓");
      return Result<void>::Ok();
    }
  }

  return Result<void>::Err(FWError::make(
      ErrorCode::TASK_NOT_FOUND, "Error: task not found " + task_id + " ✗"));
}

Result<void> FlowHookCore::set_ignored_pattern(const std::string &task_id,
                                               const std::string &pattern) {
  for (auto it = task_runners.begin(); it != task_runners.end(); it++) {
    string id = (*it)->get_task_id();
    if (id == task_id) {
      TEST((*it)->add_ignored_pattern(pattern));
      TEST(config_manager->update_task((*it)->get_task()));
      FW_LOG("[DEBUG] Task ignore pattern set: " + task_id + " ✓");
      return Result<void>::Ok();
    }
  }

  return Result<void>::Err(FWError::make(
      ErrorCode::TASK_NOT_FOUND, "Error: task not found " + task_id + " ✗ "));
}

Result<void> FlowHookCore::remove_ignored_path(const std::string &task_id,
                                               const std::string &path) {
  for (auto it = task_runners.begin(); it != task_runners.end(); it++) {
    string id = (*it)->get_task_id();
    if (id == task_id) {
      TEST((*it)->remove_ignored_path(path));
      TEST(config_manager->update_task((*it)->get_task()));
      FW_LOG("[DEBUG] Ignored path removed: " + task_id + " ✓");
      return Result<void>::Ok();
    }
  }

  return Result<void>::Err(FWError::make(
      ErrorCode::TASK_NOT_FOUND, "Error: task not found " + task_id + " ✗"));
}

Result<void> FlowHookCore::remove_ignored_pattern(const std::string &task_id,
                                                  const std::string &pattern) {
  for (auto it = task_runners.begin(); it != task_runners.end(); it++) {
    string id = (*it)->get_task_id();
    if (id == task_id) {
      TEST((*it)->remove_ignored_pattern(pattern));
      TEST(config_manager->update_task((*it)->get_task()));
      FW_LOG("[DEBUG] Ignored pattern removed: " + task_id + " ✓");
      return Result<void>::Ok();
    }
  }

  return Result<void>::Err(FWError::make(
      ErrorCode::TASK_NOT_FOUND, "Error: task not found " + task_id + " ✗ "));
}

Result<void> FlowHookCore::set_task_command(const std::string &task_id,
                                            const std::string &command) {
  for (auto it = task_runners.begin(); it != task_runners.end(); it++) {
    string id = (*it)->get_task_id();
    if (id == task_id) {
      TEST((*it)->add_command(command));
      TEST(config_manager->update_task((*it)->get_task()));
      FW_LOG("[DEBUG] Task command set: " + task_id + " ✓");
      return Result<void>::Ok();
    }
  }
  return Result<void>::Err(FWError::make(
      ErrorCode::TASK_NOT_FOUND, "Error: task not found " + task_id + " ✗ "));
}

Result<void> FlowHookCore::delete_task_command(const std::string &task_id,
                                               const std::string &command) {
  for (auto it = task_runners.begin(); it != task_runners.end(); it++) {
    string id = (*it)->get_task_id();
    if (id == task_id) {
      TEST((*it)->delete_command(command));
      TEST(config_manager->update_task((*it)->get_task()));
      FW_LOG("[DEBUG] Task command deleted: " + task_id + " ✓");
      return Result<void>::Ok();
    }
  }
  return Result<void>::Err(FWError::make(
      ErrorCode::TASK_NOT_FOUND, "Error: task not found " + task_id + " ✗ "));
}

Result<void> FlowHookCore::set_task_on_success(const std::string &task_id,
                                               const std::string &command) {
  if (task_id.empty() || command.empty()) {
    return Result<void>::Err(FWError::make(
        ErrorCode::EMPTY_VALUE,
        "Error: task id and command cannot be empty " + task_id + " ✗ "));
  }

  for (auto it = task_runners.begin(); it != task_runners.end(); it++) {
    string id = (*it)->get_task_id();
    if (id == task_id) {
      TEST((*it)->add_on_success(command));
      TEST(config_manager->update_task((*it)->get_task()));
      FW_LOG("[DEBUG] Task on success set: " + task_id + " ✓");
      return Result<void>::Ok();
    }
  }
  return Result<void>::Err(FWError::make(
      ErrorCode::TASK_NOT_FOUND, "Error: task not found " + task_id + " ✗ "));
}

Result<void> FlowHookCore::delete_task_on_success(const std::string &task_id,
                                                  const std::string &command) {
  for (auto it = task_runners.begin(); it != task_runners.end(); it++) {
    string id = (*it)->get_task_id();
    if (id == task_id) {
      TEST((*it)->delete_on_success(command));
      TEST(config_manager->update_task((*it)->get_task()));
      FW_LOG("[DEBUG] Task on success deleted: " + task_id + " ✓");
      return Result<void>::Ok();
    }
  }

  return Result<void>::Err(FWError::make(
      ErrorCode::TASK_NOT_FOUND, "Error: task not found " + task_id + " ✗ "));
}

Result<void> FlowHookCore::set_task_on_failure(const std::string &task_id,
                                               const std::string &command) {
  if (task_id.empty() || command.empty()) {
    return Result<void>::Err(FWError::make(
        ErrorCode::EMPTY_VALUE,
        "Error: task id and command cannot be empty " + task_id + " ✗ "));
  }
  for (auto it = task_runners.begin(); it != task_runners.end(); it++) {
    string id = (*it)->get_task_id();
    if (id == task_id) {
      TEST((*it)->add_on_failure(command));
      TEST(config_manager->update_task((*it)->get_task()));
      FW_LOG("[DEBUG] Task on failure set: " + task_id + " ✓");
      return Result<void>::Ok();
    }
  }
  return Result<void>::Err(FWError::make(
      ErrorCode::TASK_NOT_FOUND, "Error: task not found " + task_id + " ✗ "));
}

Result<void> FlowHookCore::delete_task_on_failure(const std::string &task_id,
                                                  const std::string &command) {
  for (auto it = task_runners.begin(); it != task_runners.end(); it++) {
    string id = (*it)->get_task_id();
    if (id == task_id) {
      TEST((*it)->delete_on_failure(command));
      TEST(config_manager->update_task((*it)->get_task()));
      FW_LOG("[DEBUG] Task on failure deleted: " + task_id + " ✓");
      return Result<void>::Ok();
    }
  }

  return Result<void>::Err(FWError::make(
      ErrorCode::TASK_NOT_FOUND, "Error: task not found " + task_id + " ✗ "));
}

Result<void> FlowHookCore::start_task(const std::string &task_id) {
  FW_LOG("[DEBUG] Starting task: " + task_id + " ...");
  FW_LOG("[DEBUG] Looping through tasks to find the correct one ...");
  for (auto it = task_runners.begin(); it != task_runners.end(); it++) {
    string id = (*it)->get_task_id();
    if (id == task_id) {
      FW_LOG("[DEBUG] Task found: " + task_id + " ✓");
      if ((*it)->is_running()) {
        return Result<void>::Err(
            FWError::make(ErrorCode::TASK_ALREADY_RUNNING,
                          "Error: task already running " + task_id + " ✗"));
      }
      FW_LOG("[DEBUG] Starting task_runner...");
      TEST((*it)->start());
      FW_VERBOSE("[FLOWHOOK] Task started: " + task_id + " ✓");
      return Result<void>::Ok();
    }
  }
  return Result<void>::Err(FWError::make(
      ErrorCode::TASK_NOT_FOUND, "Error: task not found " + task_id + " ✗"));
}

Result<void> FlowHookCore::stop_task(const std::string &task_id) {
  for (auto it = task_runners.begin(); it != task_runners.end(); it++) {
    string id = (*it)->get_task_id();
    if (id == task_id) {
      FW_LOG("[DEBUG] Stopping task: " + task_id + " ✗");
      if (!(*it)->is_running()) {
        return Result<void>::Err(
            FWError::make(ErrorCode::TASK_NOT_RUNNING,
                          "Error: task not running " + task_id + " ✗"));
      }
      TEST((*it)->stop());
      FW_VERBOSE("[FLOWHOOK] Task stopped: " + task_id + " ✓");
      return Result<void>::Ok();
    }
  }
  return Result<void>::Err(FWError::make(
      ErrorCode::TASK_NOT_FOUND, "Error: task not found " + task_id + " ✗"));
}

Result<void> FlowHookCore::start_all() {
  if (task_runners.empty()) {
    return Result<void>::Err(
        FWError::make(ErrorCode::TASK_NOT_FOUND, "Error: no tasks to start ✗"));
  }

  for (auto it = task_runners.begin(); it != task_runners.end(); it++) {
    if ((*it)->is_running()) {
      return Result<void>::Err(FWError::make(ErrorCode::TASK_ALREADY_RUNNING,
                                             "Error: task already running ✗"));
    }
    FW_LOG("[DEBUG] Starting task: " + (*it)->get_task_id() + " ...");
    TEST((*it)->start());
  }
  FW_VERBOSE("[FLOWHOOK] All tasks started ✓");
  return Result<void>::Ok();
}

Result<void> FlowHookCore::stop_all() {
  if (task_runners.empty()) {
    return Result<void>::Err(
        FWError::make(ErrorCode::TASK_NOT_FOUND, "Error: no tasks to stop ✗"));
  }

  for (auto it = task_runners.begin(); it != task_runners.end(); it++) {
    FW_LOG("[DEBUG] Stopping task " + (*it)->get_task_id() + " ...");
    TEST((*it)->stop());
  }
  FW_VERBOSE("[FLOWHOOK] All tasks stopped ✓");
  return Result<void>::Ok();
}

Result<void> FlowHookCore::start_active() {
  if (task_runners.empty()) {
    return Result<void>::Err(
        FWError::make(ErrorCode::TASK_NOT_FOUND, "Error: no tasks to start ✗"));
  }

  bool found_active = false;
  for (auto it = task_runners.begin(); it != task_runners.end(); it++) {
    if ((*it)->is_active()) {
      found_active = true;
      FW_LOG("[DEBUG] Starting task " + (*it)->get_task_id() + " ...");
      TEST((*it)->start());
    }
  }
  if (!found_active) {
    return Result<void>::Err(FWError::make(
        ErrorCode::TASK_NOT_FOUND, "Error: no active tasks to start ✗"));
  }
  FW_VERBOSE("[FLOWHOOK] All active tasks started ✓");
  return Result<void>::Ok();
}

std::vector<Task> FlowHookCore::get_tasks() const {
  vector<Task> tasks;
  for (auto it = task_runners.begin(); it != task_runners.end(); it++) {
    tasks.push_back((*it)->get_task());
  }
  return tasks;
}

Result<vector<string>>
FlowHookCore::get_watch_list(const std::string &task_id) {
  vector<string> watched_paths;
  for (auto it = task_runners.begin(); it != task_runners.end(); it++) {
    if ((*it)->get_task_id() == task_id)
      watched_paths = TRY((*it)->get_watch_list(), vector<string>);
  }
  return Result<vector<string>>::Ok(watched_paths);

  // activate
}
} // namespace flowhook
