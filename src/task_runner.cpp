#include <chrono>
#include <filesystem>
#include <fnmatch.h>
#include <stdio.h>
#include <string>
#include <sys/inotify.h>
#include <vector>

#include "include/error/error.h"
#include "include/error/result.h"
#include "include/macros.hpp"
#include "include/identity.h"
#include "types.h"
#include "include/task_runner.h"

namespace fs = std::filesystem;
using namespace std;

namespace flowhook {
Result<TaskRunner *> TaskRunner::create(const string &task_name,
                                        const string &working_directory) {
  TaskRunner *t = new TaskRunner();
  TEST_OVERLOADED(t->init(task_name, working_directory), TaskRunner *);
  return Result<TaskRunner *>::Ok(t);
}

// Builds a runner for a task whose working directory is currently missing.
// State (commands, paths, flags) is kept so the task stays representable and
// visible, but no watchers or log files are acquired until the directory
// exists again.
Result<TaskRunner *> TaskRunner::create_degraded(
    const string &task_name, const string &working_directory) {
  TaskRunner *t = new TaskRunner();
  TEST_OVERLOADED(t->init_degraded(task_name, working_directory), TaskRunner *);
  return Result<TaskRunner *>::Ok(t);
}

Result<void> TaskRunner::init(const string &task_name,
                              const string &working_directory) {
  FW_LOG("[DEBUG] Initializing task runner for " + task_name + " in " +
         working_directory + " ...✗");
  fw = TRY(FileWatcher::create(), void);
  task.name = task_name;
  task.working_dir = working_directory;
  task.uuid = generate_uuid();
  flushed = false;
  task.isRunning = false;

  if (!fs::exists(working_directory)) {
    return Result<void>::Err(FWError::make(
        ErrorCode::PATH_NOT_FOUND, "Error: working directory does not exist " +
                                       working_directory + ". ✗"));
  }

  string file_name = task_name + "-flowhook.log";
  fs::path _file_path = fs::path(working_directory) / file_name;
  sl = TRY(SessionLogger::create(_file_path.string()), void);

  FW_LOG("[FLOWHOOK] Adding path " << _file_path << " to session logger...");

  last_executed =
      std::chrono::steady_clock::now() - std::chrono::milliseconds(500);
  FW_LOG("[DEBUG] Task runner initialized. ✓");
  return Result<void>::Ok();
}

Result<void> TaskRunner::init_degraded(const string &task_name,
                                       const string &working_directory) {
  task.name = task_name;
  task.working_dir = working_directory;
  task.uuid = generate_uuid();
  flushed = false;
  task.isRunning = false;
  // fw and sl stay null: this runner only carries task state
  FW_VERBOSE("[FLOWHOOK] Task '" + task_name +
             "' degraded: its working directory " + working_directory +
             " does not exist. ✓");
  return Result<void>::Ok();
}

bool TaskRunner::is_degraded() const { return fw == nullptr; }

TaskRunner::~TaskRunner() {

  if (fw)
    fw->stop();
  delete fw;
  if (sl)
    sl->stop();
  delete sl;
  task.isRunning = false;
  FW_LOG("[DEBUG] TaskRunner destoryed.");
}

Result<void> TaskRunner::set_depth(int num) {
  if (num > 10) {
    return Result<void>::Err(FWError::make(
        ErrorCode::INVALID_DEPTH, "Error: invalid depth set - depth too much "
                                  "✗, use a depth between 0 and 10"));
  } else if (num < -1) {
    return Result<void>::Err(FWError::make(
        ErrorCode::INVALID_DEPTH, "Error: invalid depth set - depth set too "
                                  "low ✗, use a depth between 0 and 10"));
  }

  task.watching_depth = num;
  FW_LOG("[DEBUG] Depth set to " + std::to_string(num));
  return Result<void>::Ok();
}

Result<void> TaskRunner::set_timeout(int timeout) {
    if(timeout <= 0) {
        return Result<void>::Err(FWError::make(
            ErrorCode::INVALID_TIMEOUT, "Error: invalid timeout set - timeout must be greater than 0"));
    }
  task.build_timeout = timeout;
  FW_LOG("[DEBUG] Timeout set to " + std::to_string(timeout));
  return Result<void>::Ok();
}

Result<void> TaskRunner::change_task_name(const string &task_name) {
  // check if task name is empty
  if (task_name.empty()) {
    return Result<void>::Err(
        FWError::make(ErrorCode::EMPTY_VALUE, "Error: task name is empty ✗"));
  }
  // check if task name already exists
  if (task.name == task_name) {
    return Result<void>::Err(FWError::make(
        ErrorCode::TASK_ALREADY_EXISTS, "Error: task name already exists ✗"));
  }
  task.name = task_name;
  FW_LOG("[DEBUG] Changing task name successful. ✓");
  return Result<void>::Ok();
}

Result<void>
TaskRunner::change_working_directory(const string &working_directory) {
  // check if working directory is empty
  if (working_directory.empty()) {
    return Result<void>::Err(FWError::make(
        ErrorCode::EMPTY_VALUE, "Error: working directory is empty ✗"));
  }
  // check if working directory is a valid path
  if (!fs::exists(working_directory)) {
    return Result<void>::Err(FWError::make(
        ErrorCode::PATH_NOT_FOUND, "Error: working directory not found ✗"));
  }
  // check if working directory already exists
  if (task.working_dir == working_directory) {
    return Result<void>::Err(
        FWError::make(ErrorCode::PATH_ALREADY_EXISTS,
                      "Error: working directory already exists ✗"));
  }
  task.working_dir = working_directory;
  FW_LOG("[DEBUG] Working directory changed successfully. ✓");
  return Result<void>::Ok();
}

bool TaskRunner::isIgnored(const string &path) {
  string filename = fs::path(path).filename().string();
  for (auto &p : task.ignored_paths) {
    if (p == path || p == filename)
      return true;
  }

  for (auto &p : task.ignored_patterns) {
    if (fnmatch(p.c_str(), filename.c_str(), 0) == 0)
      return true;
    if (fnmatch(p.c_str(), path.c_str(), 0) == 0)
      return true;
  }
  return false;
}

Result<void> TaskRunner::add_ignored_path(const string &path) {
  if (path.empty()) {
    return Result<void>::Err(
        FWError::make(ErrorCode::EMPTY_VALUE, "Error: command is empty"));
  }
  for (auto &p : task.ignored_paths) {
    if (p == path) {
      return Result<void>::Err(FWError::make(ErrorCode::PATH_ALREADY_EXISTS,
                                             "Error: path already exists"));
    }
  }
  task.ignored_paths.push_back(path);
  FW_LOG("[DEBUG] Adding ignored path " + path + " to Task completed. ✓");
  return Result<void>::Ok();
}

Result<void> TaskRunner::add_ignored_pattern(const string &pattern) {
  if (pattern.empty()) {
    return Result<void>::Err(
        FWError::make(ErrorCode::EMPTY_VALUE, "Error: command is empty"));
  }
  for (auto &p : task.ignored_patterns) {
    if (p == pattern) {
      return Result<void>::Err(FWError::make(ErrorCode::PATH_ALREADY_EXISTS,
                                             "Error: path already exists"));
    }
  }
  task.ignored_patterns.push_back(pattern);
  FW_LOG("[DEBUG] Adding ignored pattern " + pattern + " to Task completed. ✓");
  return Result<void>::Ok();
}

Result<void> TaskRunner::remove_ignored_path(const string &path) {
  for (auto it = task.ignored_paths.begin(); it != task.ignored_paths.end();
       it++) {
    if (*it == path) {
      task.ignored_paths.erase(it);
      return Result<void>::Ok();
    }
  }

  return Result<void>::Err(FWError::make(ErrorCode::VALUE_NOT_FOUND,
                                         "Error: ignored path not found"));
}

Result<void> TaskRunner::remove_ignored_pattern(const string &pattern) {
  for (auto it = task.ignored_patterns.begin();
       it != task.ignored_patterns.end(); it++) {
    if (*it == pattern) {
      task.ignored_patterns.erase(it);
      return Result<void>::Ok();
    }
  }

  return Result<void>::Err(FWError::make(ErrorCode::VALUE_NOT_FOUND,
                                         "Error: ignored pattern not found"));
}

Result<void> TaskRunner::add_command(const string &command) {
  // check if command is empty
  if (command.empty()) {
    return Result<void>::Err(
        FWError::make(ErrorCode::EMPTY_VALUE, "Error: command is empty"));
  }

  // check if command already exists
  for (auto &cmd : task.commands) {
    if (cmd == command) {
      return Result<void>::Err(
          FWError::make(ErrorCode::COMMAND_ALREADY_EXISTS,
                        "Error: command already exists ✗"));
    }
  }

  task.commands.push_back(command);
  return Result<void>::Ok();
}

Result<void> TaskRunner::delete_command(const string &command) {
  for (auto it = task.commands.begin(); it != task.commands.end(); it++) {
    if (*it == command) {
      task.commands.erase(it);
      return Result<void>::Ok();
    }
  }

  return Result<void>::Err(
      FWError::make(ErrorCode::COMMAND_NOT_FOUND, "Error: command not found"));
}

bool TaskRunner::check_path_existence(const string &path) {
  for (auto &p : task.file_paths) {
    if (p == path) {
      return true;
    }
  }
  for (auto &p : task.dir_paths) {
    if (p == path) {
      return true;
    }
  }
  return false;
}

Result<void> TaskRunner::add_path(const string &path) {
  if (check_path_existence(path))
    return Result<void>::Ok(); // idempotent
  if (!fs::exists(path)) {
    return Result<void>::Err(
        FWError::make(ErrorCode::PATH_NOT_FOUND,
                      "Error: provided path " + path + " does not exist! ✗ \n // use the `flowhook remove --path <path>` command to remove it from the task"));
  }
  if (isIgnored(path)) {
    FW_LOG("[DEBUG] Path " + path + " matches ignored paths and patterns.");
    return Result<void>::Ok();
  }

  TEST(add_path_internal(path, task.watching_depth, 0));
  if (fs::is_directory(path)) {
    task.dir_paths.push_back(path);
    FW_VERBOSE("[FLOWHOOK] Adding " + path +
               "'s children to task runner completed. ✓");
  } else
    task.file_paths.push_back(path);
  FW_VERBOSE("[FLOWHOOK] Adding path " + path + " to task runner completed. ✓");
  return Result<void>::Ok();
}

Result<void> TaskRunner::add_path_internal(const string &path, int MAX_DEPTH,
                                           int CURRENT_DEPTH) {
  if (check_path_existence(path))
    return Result<void>::Ok(); // idempotent
  // if path is directory add each files inside it iteratively
  if (isIgnored(path)) {
    FW_LOG("[DEBUG] Path " + path + " matches ignored paths and patterns.");
    FW_LOG("[DEBUG] Adding Path " + path + " to filewatcher failed. ✗");
    return Result<void>::Ok();
  }

  if (fs::is_directory(path)) {
    FW_LOG("[DEBUG] Path " + path +
           " is a directory adding child files recursively...");
    for (auto &entry : fs::directory_iterator(path)) {
      if (entry.path().filename() == MARKER_FILE_NAME) {
        continue; // the identity marker is FlowHook's own bookkeeping file
      }
      if (isIgnored(entry.path().string())) {
        FW_LOG("[DEBUG] Path " + entry.path().string() +
               " matches ignored patterns.");
        FW_LOG("[DEBUG] Adding Path " + entry.path().string() +
               " to task failed. ✗");
        continue;
      }

      if (entry.is_regular_file()) {
        FW_LOG("[DEBUG] Adding path " + entry.path().string() +
               " to filewatcher...");
        FW_LOG("[DEBUG] Checking if resolved file path " +
               entry.path().string() +
               " matches ignored paths and patterns ...");

        resolved_files.push_back(entry.path().string());
        fw->add_path(entry.path().string());

        FW_LOG("[DEBUG] Adding path " + entry.path().string() +
               " to task completed. ✓");
      } else if (entry.is_directory()) {
        if (MAX_DEPTH > CURRENT_DEPTH) {
          resolved_files.push_back(entry.path().string());
          TEST(add_path_internal(entry.path().string(), MAX_DEPTH,
                                 CURRENT_DEPTH + 1));
        } else {
          FW_LOG("[DEBUG] Path " + entry.path().string() +
                 " is a directory. But MAX_DEPTH=" + to_string(MAX_DEPTH) +
                 " have been reached. Child files won't be watched.");
        }
      }
    }
  } else if (!fs::is_directory(path)) {
    FW_LOG("[DEBUG] Path " + path + " is a file. Adding to task...");
    resolved_files.push_back(path);
    fw->add_path(path);
  }
  return Result<void>::Ok();
}

Result<void> TaskRunner::delete_path(const string &path) {
  if (!fs::exists(path)) {
    return Result<void>::Err(
        FWError::make(ErrorCode::PATH_NOT_FOUND,
                      "Error: provided path " + path + " does not exist! ✗"));
  }

  if (!fs::is_directory(path)) {
    for (auto it = task.file_paths.begin(); it != task.file_paths.end(); it++) {
      if (*it == path) {
        task.file_paths.erase(it);
        return Result<void>::Ok();
      }
    }
  } else {
    for (auto it = task.dir_paths.begin(); it != task.dir_paths.end(); it++) {
      if (*it == path) {
        task.dir_paths.erase(it);
        return Result<void>::Ok();
      }
    }
  }
  if (fw)
    TEST(fw->remove_path(path));
  return Result<void>::Err(
      FWError::make(ErrorCode::PATH_NOT_FOUND, "Error: path not found"));
}

Result<void> TaskRunner::add_on_success(const string &command) {
  // check if command is empty
  if (command.empty()) {
    return Result<void>::Err(
        FWError::make(ErrorCode::EMPTY_VALUE, "Error: command is empty"));
  }

  // check if command already exists
  for (auto it = task.on_success.begin(); it != task.on_success.end(); it++) {
    if (*it == command) {
      return Result<void>::Err(FWError::make(ErrorCode::COMMAND_ALREADY_EXISTS,
                                             "Error: command already exists"));
    }
  }
  task.on_success.push_back(command);
  return Result<void>::Ok();
}

Result<void> TaskRunner::delete_on_success(const string &command) {
  for (auto it = task.on_success.begin(); it != task.on_success.end(); it++) {
    if (*it == command) {
      task.on_success.erase(it);
      return Result<void>::Ok();
    }
  }

  return Result<void>::Err(
      FWError::make(ErrorCode::COMMAND_NOT_FOUND, "Error: command not found"));
}

Result<void> TaskRunner::add_on_failure(const string &command) {
  // check if command is empty
  if (command.empty()) {
    return Result<void>::Err(
        FWError::make(ErrorCode::EMPTY_VALUE, "Error: command is empty"));
  }
  // check if command already exists
  for (auto it = task.on_failure.begin(); it != task.on_failure.end(); it++) {
    if (*it == command) {
      return Result<void>::Err(FWError::make(ErrorCode::COMMAND_ALREADY_EXISTS,
                                             "Error: command already exists"));
    }
  }
  task.on_failure.push_back(command);
  return Result<void>::Ok();
}

Result<void> TaskRunner::delete_on_failure(const string &command) {
  for (auto it = task.on_failure.begin(); it != task.on_failure.end(); it++) {
    if (*it == command) {
      task.on_failure.erase(it);
      return Result<void>::Ok();
    }
  }

  return Result<void>::Err(
      FWError::make(ErrorCode::COMMAND_NOT_FOUND, "Error: command not found"));
}

Result<void> TaskRunner::execute(const WatchEvent &e) {
  auto now = chrono::steady_clock::now();
  if (now - last_executed < cooldown_ms)
    return Result<void>::Ok(); // silently skip

  FW_LOG("[DEBUG] Starting command execution pipeline ...");
  // check if event is null
  if (e.isNull()) {
    return Result<void>::Err(
        FWError::make(ErrorCode::EVENT_NOT_FOUND, "Error: event is null ✗"));
  }

  // check if task commands are empty
  if (task.commands.empty()) {
    return Result<void>::Err(FWError::make(ErrorCode::COMMAND_EMPTY,
                                           "Error: no commands to execute ✗"));
  }

  for (auto &cmd : task.commands) {
    FW_LOG("[DEBUG] Executing command through pipes ....");
    int timeout = task.build_timeout;
    string secure_execution_chain =
        "cd " + task.working_dir + " && " + (timeout > 0 ? "timeout " + std::to_string(timeout) + "s " : "") + cmd + " 2>&1";
    FILE *fp = popen(secure_execution_chain.c_str(), "r");
    if (fp == NULL) {
      return Result<void>::Err(
          FWError::make(ErrorCode::SYS_PIPE_FAILED, "Error: popen failure ✗"));
    }

    char buffer[128];
    string log_output;

    size_t total_bytes = 0;
    const size_t MAX_LOG_SIZE = 64 * 1024;
    while (fgets(buffer, 128, fp) != NULL) {
      size_t chunk_size = strlen(buffer);
      total_bytes += chunk_size;
      LOG_BUILD_OUTPUT(buffer);
      if (total_bytes < MAX_LOG_SIZE) {
        log_output += buffer;
        continue;
      } else {
        log_output +=
            "\n[FLOWHOOK WARNING: Log Truncated. Exceeded 64KB safety limit]";
        break;
      }
    }
    if (ferror(fp)) {
      return Result<void>::Err(FWError::make(
          ErrorCode::SYS_IO_FAILED, "Error: reading from pipe failed ✗"));
    }

    int status = pclose(fp);
    int true_exit_code = -1;
    if (WIFEXITED(status)) {
      true_exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
      true_exit_code = -WTERMSIG(status);
    } else {
      true_exit_code = status;
    }

    if (true_exit_code != 0) {
      for (auto &cmd_i : task.on_failure) {
        FW_LOG("[DEBUG] Executing failure commands...");
        int timeout = task.build_timeout;
        string exec_chain =
            "cd " + task.working_dir + " && " + (timeout > 0 ? "timeout " + std::to_string(timeout) + "s " : "") + cmd_i;
        system(exec_chain.c_str());
      }
    } else {
      for (auto &cmd_i : task.on_success) {
        FW_LOG("[DEBUG] Executing success commands...");
        int timeout = task.build_timeout;
        string exec_chain =
            "cd " + task.working_dir + " && " + (timeout > 0 ? "timeout " + std::to_string(timeout) + "s " : "") + cmd_i;
        system(exec_chain.c_str());
      }
    }

    ExecutionResult result = {execution_id, true_exit_code, e, log_output,
                              task.commands};
    TEST(sl->log_execution(result));
    execution_id++;
  }
  last_executed = chrono::steady_clock::now();
  FW_LOG("[DEBUG] Command Execution pipeline finalized. ✓");
  return Result<void>::Ok();
}

Result<void> TaskRunner::add_callback(const WatchCallback &callback) {
  // check if callback is empty
  if (callback.isNull()) {
    return Result<void>::Err(
        FWError::make(ErrorCode::EMPTY_VALUE, "Error: callback is empty ✓"));
  }
  // check if callback already exists
  for (auto &cb : callbacks) {
    if (cb == callback) {
      return Result<void>::Err(
          FWError::make(ErrorCode::CALLBACK_ALREADY_EXISTS,
                        "Error: callback already exists ✓"));
    }
  }
  //
  callbacks.push_back(callback);
  return Result<void>::Ok();
}

Result<void> TaskRunner::delete_callback(const WatchCallback &callback) {
  for (auto it = callbacks.begin(); it != callbacks.end(); it++) {
    if (*it == callback) {
      callbacks.erase(it);
      return Result<void>::Ok();
    }
  }

  return Result<void>::Err(FWError::make(ErrorCode::CALLBACK_NOT_FOUND,
                                         "Error: callback not found"));
}

Result<void> TaskRunner::start() {
  FW_LOG("[DEBUG] Starting TaskRunner...");
  if (is_degraded()) {
    return Result<void>::Err(
        FWError::make(ErrorCode::PATH_NOT_FOUND,
                      "Error: task is '" + task.name + "' degraded: its working directory " +
                          task.working_dir + " does not exist ✗ \n // cd into that task's renamed directory and run flowhook check to re-anchor it."));
  }
  if (task.isRunning) {
    return Result<void>::Ok(); // idempotent
  }
  task.isRunning = true;

  WatchCallback callback = {this, &TaskRunner::execute};
  TEST(add_callback(callback));
  FW_LOG("[DEBUG]  Linking callbacks with events...");
  for (auto &cb : callbacks) {
    TEST(fw->link_event(IN_CLOSE_WRITE, cb));
    TEST(fw->link_event(IN_MODIFY, cb));
    TEST(fw->link_event(IN_MOVED_FROM, cb));
  }

  TEST(sl->start());

  TEST(fw->start(100));
  return Result<void>::Ok();
}

Result<vector<string>> TaskRunner::get_watch_list() {
  if (is_degraded())
    return Result<vector<string>>::Ok(vector<string>());
  return fw->get_watch_list();
}

Result<void> TaskRunner::stop() {
  if (!task.isRunning) {
    return Result<void>::Ok();
  }

  int count = 0;

  // unlink all linked events
  for (auto &cb : callbacks) {
    TEST(fw->unlink_event(IN_CLOSE_WRITE, cb));
    TEST(fw->unlink_event(IN_MODIFY, cb));
    TEST(fw->unlink_event(IN_MOVED_FROM, cb));
    count++;
  }

  // remove the callback added at start
  WatchCallback callback = {this, &TaskRunner::execute};
  TEST(delete_callback(callback));

  TEST(fw->stop());
  TEST(sl->stop());
  task.isRunning = false;
  return Result<void>::Ok();
}
} // namespace flowhook

// WRITTEN BY Mnasie
