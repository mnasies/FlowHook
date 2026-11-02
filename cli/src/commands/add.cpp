#include "../../src/include/flowhook_core.h"
#include "../include/CLI11.hpp"
#include "../../src/include/macros.hpp"
#include "../include/cli_utils.h"


namespace fs = std::filesystem;
namespace flowhook_cli {

void register_add(CLI::App *app, flowhook::FlowHookCore *fh) {
    auto _fh = flowhook::FlowHookCore::create();
    fh = handle_fh_error(_fh);

    // add subcommand
  auto *add =
      app->add_subcommand("add", "adding a new task to the FlowHook project \n");

  // this is a purely ceremonial flag(so that CLI11 won't say there is unrecognized flag)
  // the actual parsing of verbose and setting of the environment variable is done in main.cpp
  add->add_flag("--debug", FLOWHOOK_DEBUG, "Enable debug output. \n// is an extremely detailed output that shows every step of the process.");
  add->add_flag("--verbose", FLOWHOOK_VERBOSE, "Enable verbose output. \n// shows a summary of the process.");


  static std::string task_id = "";
  static std::string add_n_path = "";
  add->add_option("--path", add_n_path, "Path to the task [optional] \n// use to add a new path to be watched.\n");

  static std::string add_command = "";
  add->add_option("--command", add_command, "Command to run for the task [optional] \n// if not provided, the task will not run a command when triggered\n");

  static std::string command_on_success = "";
  static std::string command_on_failure = "";
  static std::string ignored_path = "";
  static std::string ignored_pattern = "";

  add->add_option("--on-success", command_on_success,
                  "Command to run on success [optional] \n// if not provided, no command will be run on success\n");
  add->add_option("--on-failure", command_on_failure,
                  "Command to run on failure [optional] \n// if not provided, no command will be run on failure\n");
  add->add_option("--ignored-path", ignored_path, "Ignored path [optional] \n// if not provided, all files will be watched in the working directory\n");
  add->add_option("--ignored-pattern", ignored_pattern, "Ignored pattern [optional] \n// if not provided, no pattern will be ignored in the working directory\n");

  add->callback([=]() mutable {
    fs::path cwd = fs::current_path();
    task_id = cwd.string();

    if (add_n_path.empty() && add_command.empty() &&
        command_on_success.empty() && command_on_failure.empty() &&
        ignored_path.empty() && ignored_pattern.empty()) {
      std::cout << "Error: At least one of --path, --command, --on-success, --on-failure, --ignored-path, or --ignored-pattern is required\n"
                << std::endl;
      return;
    }

    if (!add_n_path.empty()) {
      auto r = fh->set_task_path(task_id, add_n_path);
      if (r.isErr()) {
        std::cout << "Failed to set task path: " << r.getErrMessage()
                  << std::endl;
        return;
      }
      std::cout << "Task path " << add_n_path << " set successfully." << std::endl;
    }
    if (!add_command.empty()) {
      auto r = fh->set_task_command(task_id, add_command);
      if (r.isErr()) {
        std::cout << "Failed to set task command: " << r.getErrMessage()
                  << std::endl;
        return;
      }
      std::cout << "Task command " << add_command << " set successfully." << std::endl;
    }
    if (!command_on_success.empty()) {
      auto r = fh->set_task_on_success(task_id, command_on_success);
      if (r.isErr()) {
        std::cout << "Failed to set task on success: " << r.getErrMessage()
                  << std::endl;
        return;
      }
      std::cout << "Task on success command " << command_on_success << " set successfully." << std::endl;
    }
    if (!command_on_failure.empty()) {
      auto r = fh->set_task_on_failure(task_id, command_on_failure);
      if (r.isErr()) {
        std::cout << "Failed to set task on failure: " << r.getErrMessage()
                  << std::endl;
        return;
      }
      std::cout << "Task on failure command " << command_on_failure << " set successfully." << std::endl;
    }
    if (!ignored_path.empty()) {
      auto r = fh->set_ignored_path(task_id, ignored_path);
      if (r.isErr()) {
        std::cout << "Failed to set ignored path: " << r.getErrMessage()
                  << std::endl;
        return;
      }
      std::cout << "Ignored path " << ignored_path << " set successfully." << std::endl;
    }
    if (!ignored_pattern.empty()) {
      auto r = fh->set_ignored_pattern(task_id, ignored_pattern);
      if (r.isErr()) {
        std::cout << "Failed to set ignored pattern: " << r.getErrMessage()
                  << std::endl;
        return;
      }
      std::cout << "Ignored pattern " << ignored_pattern << " set successfully." << std::endl;
    }
  });
}

} // namespace flowhook_cli
