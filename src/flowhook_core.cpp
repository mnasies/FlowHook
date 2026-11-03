#include <filesystem>
#include <string>
#include <vector>

#include "include/error/error.h"
#include "include/error/result.h"
#include "include/flowhook_core.h"
#include "include/identity.h"
#include "include/macros.hpp"
#include "include/task_runner.h"

#include <fnmatch.h>

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
  // .flowhook carries the project identity marker; watch activity on it is
  // always FlowHook's own bookkeeping
  default_ignored_paths = {".git", ".flowhook"};
  return Result<void>::Ok();
}

Result<void> FlowHookCore::lazy_init() {
    config_manager = TRY(ConfigManager::create(), void);
    FW_VERBOSE("[FLOWHOOK] a lazy Flowhook core instance initialized.");
    return Result<void>::Ok();
}

// Looks up a task by id in the live task runners first, then falls back to
// the persisted config. This lets read-only methods work in lazy mode, where
// no task runners are created.
Result<Task> FlowHookCore::find_task(const std::string &task_id) {
  for (auto it = task_runners.begin(); it != task_runners.end(); it++) {
    if ((*it)->get_task_id() == task_id)
      return Result<Task>::Ok((*it)->get_task());
  }

  if (config_manager != nullptr) {
    auto _ts = TRY(config_manager->get_tasks(), Task);
    for (auto &t : _ts)
      if (t.id == task_id)
        return Result<Task>::Ok(t);

    // Re-anchor fallback: if the caller's path isn't known to config, but
    // carries a valid .flowhook marker whose uuid matches an existing task
    // whose old directory is missing on disk, this is a renamed directory.
    // Update the config in place and return the re-anchored task.
    auto marker = read_marker(task_id);
    if (marker.isOk()) {
      Identity iden = marker.unwrap();
      if (!iden.uuid.empty()) {
        for (auto &t : _ts) {
          if (t.uuid == iden.uuid && !t.uuid.empty() && !fs::exists(t.id)) {
            FW_VERBOSE("[FLOWHOOK] Re-anchoring task '" + t.name + "': " +
                       t.id + " -> " + task_id);
            std::string old_id = t.id;
            Task reanchored = t;
            reanchored.id = task_id;
            for (auto &dp : reanchored.dir_paths) {
              if (dp.rfind(old_id, 0) == 0) {
                dp = task_id + dp.substr(old_id.size());
              }
            }
            for (auto &fp : reanchored.file_paths) {
              if (fp.rfind(old_id, 0) == 0) {
                fp = task_id + fp.substr(old_id.size());
              }
            }
            auto u = config_manager->update_task(reanchored);
            if (u.isOk()) {
              for (auto it = task_runners.begin(); it != task_runners.end(); it++) {
                if ((*it)->get_task().uuid == iden.uuid) {
                  (*it)->change_working_directory(task_id);
                }
              }
              return Result<Task>::Ok(reanchored);
            }
          }
        }
      }
    }
  }

  return Result<Task>::Err(FWError::make(
      ErrorCode::TASK_NOT_FOUND, "Error: task not found " + task_id + " ✗"));
}

// Mirrors TaskRunner::isIgnored for tasks that have no live runner.
bool FlowHookCore::is_lazy_ignored(const Task &task, const std::string &path) {
  string filename = fs::path(path).filename().string();
  if (filename == MARKER_FILE_NAME)
    return true;
  for (auto &p : task.ignored_paths)
    if (p == path || p == filename)
      return true;
  for (auto &p : task.ignored_patterns)
    if (fnmatch(p.c_str(), filename.c_str(), 0) == 0 ||
        fnmatch(p.c_str(), path.c_str(), 0) == 0)
      return true;
  return false;
}

// Mirrors TaskRunner::add_path_internal's traversal (resolved file list only)
// without touching any watcher. Skips paths that no longer exist so renamed
// or deleted paths are silently filtered out.
void FlowHookCore::resolve_files_lazy(const Task &task, const string &path,
                                      int current_depth,
                                      vector<string> &out) {
  if (!fs::exists(path) || is_lazy_ignored(task, path))
    return;
  if (fs::is_directory(path)) {
    for (auto &entry : fs::directory_iterator(path)) {
      if (entry.path().filename() == MARKER_FILE_NAME)
        continue; // the identity marker is FlowHook's own bookkeeping file
      if (is_lazy_ignored(task, entry.path().string()))
        continue;
      if (entry.is_regular_file()) {
        out.push_back(entry.path().string());
      } else if (entry.is_directory() &&
                 current_depth < task.watching_depth) {
        out.push_back(entry.path().string());
        resolve_files_lazy(task, entry.path().string(), current_depth + 1,
                           out);
      }
    }
  } else {
    out.push_back(path);
  }
}

Result<void> FlowHookCore::init() {
  config_manager = TRY(ConfigManager::create(), void);
  FW_LOG("[DEBUG] initializing a FlowHook core instance ....");

  vector<Task> tasks = TRY(config_manager->get_tasks(), void);
  set_default_ignored();

  FW_LOG("[DEBUG] loading tasks from config file ....");
  for (auto task : tasks) {
    // A task whose working directory has vanished (renamed, deleted) is
    // represented in degraded state: no watchers, no log files, but the
    // task stays visible and re-anchorable. One stale dir must not abort
    // the whole init.
    if (!fs::exists(task.id)) {
      TaskRunner *tr = TRY(TaskRunner::create_degraded(task.name, task.id), void);
      if (!task.uuid.empty())
        tr->adopt_uuid(task.uuid);
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
      for (auto s : task.on_success)
        TEST(tr->add_on_success(s));
      for (auto f : task.on_failure)
        TEST(tr->add_on_failure(f));
      task_runners.push_back(tr);
      FW_VERBOSE("[FLOWHOOK] Task '" + task.name + "' at " + task.id +
                 " has no working directory: loaded in degraded state.\n" +
                 "           cd into the renamed directory and run flowhook "
                 "check to re-anchor it.");
      continue;
    }

    TaskRunner *tr = TRY(TaskRunner::create(task.name, task.id), void);
    // config owns identity: the freshly stamped runner uuid is discarded
    // in favor of the persisted one
    if (!task.uuid.empty())
      tr->adopt_uuid(task.uuid);

    // identity reconciliation (report only): a marker whose uuid disagrees
    // with the config entry means this directory may belong to another task
    // and adopting it silently would be unsafe
    auto marker = read_marker(task.id);
    if (marker.isOk()) {
      std::string marker_uuid = marker.unwrap().uuid;
      std::string config_uuid = tr->get_task().uuid;
      if (marker_uuid != config_uuid) {
        FW_VERBOSE(
            "[FLOWHOOK] Identity conflict: task '" + task.name + "' at " +
            task.id + " claims uuid " + config_uuid + " but its marker says " +
            marker_uuid + "\n" +
            "           this directory may belong to another task — resolve "
            "it explicitly\n" +
            "           (flowhook remove, or clean the stale .flowhook marker)");
      }
    }

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

  auto existing_marker = read_marker(task_id);
  if (existing_marker.isOk()) {
    std::string m_uuid = existing_marker.unwrap().uuid;
    for (auto it = task_runners.begin(); it != task_runners.end(); it++) {
      if ((*it) == nullptr)
        continue;
      if (!m_uuid.empty() && (*it)->get_task().uuid == m_uuid) {
        return Result<void>::Err(FWError::make(ErrorCode::DUPLICATE_ENTRY,
                                               "Error: task already exists ✗"));
      }
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
  // a from-scratch task owns its directory: stamp the identity marker
  TEST(write_marker(task_id, result->get_task()));

  FW_LOG("[DEBUG] logging task to config file completed. ✓");
  FW_VERBOSE("[FLOWHOOK] Task created: name='" + task_name + "' path='" +
             task_id + "' ✓");
  return Result<void>::Ok();
}

Result<void> FlowHookCore::delete_task(const std::string &task_id) {
  FW_LOG("[DEBUG] Deleting task...");
  std::string marker_uuid;
  auto marker = read_marker(task_id);
  if (marker.isOk()) {
    marker_uuid = marker.unwrap().uuid;
  }
  for (auto it = task_runners.begin(); it != task_runners.end(); it++) {
    string name = (*it)->get_task_id();
    if (name == task_id || (!marker_uuid.empty() && (*it)->get_task().uuid == marker_uuid)) {
      Task _task_to_be_deleted = (*it)->get_task();
      TEST(config_manager->delete_task(_task_to_be_deleted));
      TEST(delete_marker(task_id));
      if (_task_to_be_deleted.id != task_id) {
        delete_marker(_task_to_be_deleted.id);
      }
      task_runners.erase(it);
      FW_VERBOSE("[FLOWHOOK] Task deleted: " + task_id + "' ✓");
      return Result<void>::Ok();
    }
  }

  return Result<void>::Err(FWError::make(
      ErrorCode::TASK_NOT_FOUND, "Error: task not found " + task_id + " ✗"));
}

bool FlowHookCore::is_task(const std::string &task_id) {
  return find_task(task_id).isOk();
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
  auto t = find_task(task_id);
  if (t.isErr())
    return std::vector<std::string>();
  Task task = t.unwrap();

  // full mode: report the live watcher state
  for (auto it = task_runners.begin(); it != task_runners.end(); it++) {
    if ((*it)->get_task_id() == task_id)
      return (*it)->get_resolved_files();
  }

  // lazy mode: resolve from the persisted task without any watcher
  vector<string> files;
  for (auto &p : task.file_paths)
    resolve_files_lazy(task, p, 0, files);
  for (auto &p : task.dir_paths)
    resolve_files_lazy(task, p, 0, files);
  return files;
}

Result<int> FlowHookCore::get_task_depth(const std::string &task_id) {
  auto t = TRY(find_task(task_id), int);
  return Result<int>::Ok(t.watching_depth);
}

Result<int> FlowHookCore::get_task_timeout(const std::string &task_id) {
  auto t = TRY(find_task(task_id), int);
  return Result<int>::Ok(t.build_timeout);
}

Result<bool> FlowHookCore::is_task_active(const std::string &task_id) {
  auto t = TRY(find_task(task_id), bool);
  return Result<bool>::Ok(t.isActive);
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

  int started = 0;
  for (auto it = task_runners.begin(); it != task_runners.end(); it++) {
    if ((*it)->is_running()) {
      continue; // already running, not an error
    }
    FW_LOG("[DEBUG] Starting task: " + (*it)->get_task_id() + " ...");
    auto r = (*it)->start();
    if (r.isErr()) {
      // degraded (missing dir) tasks are skipped with a warning; starting
      // the rest must not be blocked by a single unreachable task
      FW_VERBOSE("[FLOWHOOK] Skipped task '" + (*it)->get_task_name() +
                 "': " + r.getErrMessage());
      continue;
    }
    started++;
  }
  if (started == 0) {
    return Result<void>::Err(
        FWError::make(ErrorCode::TASK_NOT_FOUND, "Error: no startable tasks ✗"));
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
      auto r = (*it)->start();
      if (r.isErr()) {
        // degraded (missing dir) tasks are skipped with a warning; starting
        // the rest must not be blocked by a single unreachable task
        FW_VERBOSE("[FLOWHOOK] Skipped task '" + (*it)->get_task_name() +
                   "': " + r.getErrMessage());
        continue;
      }
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
  // full mode: report the live runner state
  if (!task_runners.empty()) {
    vector<Task> tasks;
    for (auto it = task_runners.begin(); it != task_runners.end(); it++) {
      tasks.push_back((*it)->get_task());
    }
    return tasks;
  }
  // lazy mode with a config manager: report persisted tasks
  if (config_manager != nullptr) {
    auto _ts = config_manager->get_tasks();
    if (_ts.isOk())
      return _ts.unwrap();
    return vector<Task>();
  }
  return vector<Task>();
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
