#include "../../src/include/flowhook_core.h"
#include "../include/CLI11.hpp"
#include "../../src/include/macros.hpp"
#include "../include/cli_utils.h"


namespace fs = std::filesystem;
namespace flowhook_cli {

void register_remove(CLI::App *app, flowhook::FlowHookCore *&fh) {

    // add subcommand
  auto *remove =
      app->add_subcommand("remove", "remove an element from a task\n // removes the task itself if no OPTION is provided");

  // this is a purely ceremonial flag(so that CLI11 won't say there is unrecognized flag)
  // the actual parsing of verbose and setting of the environment variable is done in main.cpp
  remove->add_flag("--debug", FLOWHOOK_DEBUG, "Enable debug output. \n// is an extremely detailed output that shows every step of the process.");
  remove->add_flag("--verbose", FLOWHOOK_VERBOSE, "Enable verbose output. \n// shows a summary of the process.");

  static std::string task_id = "";
  static std::string removed_path = "";
  remove->add_option("--path", removed_path, "Path to the task [optional] \n// use to add a new path to be watched.\n");

  static std::string removed_command = "";
  remove->add_option("--command", removed_command, "Remove an existing command from task [optional] \n// if not provided, the task will not run a command when triggered\n");

  static std::string command_on_success = "";
  static std::string command_on_failure = "";
  static std::string ignored_path = "";
  static std::string ignored_pattern = "";

  remove->add_option("--on-success", command_on_success,
                  "Remove on_success command from task [optional] \n// if not provided, no command will be run on success\n");
  remove->add_option("--on-failure", command_on_failure,
                  "Remove on_failure command from task [optional] \n// if not provided, no command will be run on failure\n");
  remove->add_option("--ignored-path", ignored_path, "Ignored path [optional] \n// if not provided, all files will be watched in the working directory\n");
  remove->add_option("--ignored-pattern", ignored_pattern, "Ignored pattern [optional] \n// if not provided, no pattern will be ignored in the working directory\n");

  static bool force = false;
  remove->add_flag("-f", force, "removes flowhook instance without providing a dialog to check certainity");

  remove->callback([=, &fh]() mutable {
      auto _fh = flowhook::FlowHookCore::create();
      fh = handle_fh_error(_fh);

      fs::path cwd = fs::current_path();
    task_id = cwd.string();

    bool remove_task = false;
    if(removed_path.empty() && removed_command.empty() &&
       command_on_success.empty() && command_on_failure.empty() &&
       ignored_path.empty() && ignored_pattern.empty()) {
            remove_task = true;
    }

    if (remove_task) {
        if(!force){
            std::cout << "Are you sure you want to remove the flowhook instance from the current directory?" << std::endl;
            std::cout << "Type 'yes' to confirm: ";
            std::string confirmation;
            std::cin >> confirmation;
            if (confirmation != "yes") {
                std::cout << "Removal cancelled." << std::endl;
                return;
            }
        }
        auto r = fh->delete_task(task_id);
        if (r.isErr()) {
            std::cout << "Failed to remove task: " << r.getErrMessage() << std::endl;
        } else {
            std::cout << "Task removed successfully." << std::endl;
        }
        return;
    }


    if (!removed_path.empty()) {
      auto r = fh->delete_task_path(task_id, removed_path);
      if (r.isErr()) {
        std::cerr << "Failed to removed task path: " << r.getErrMessage()
                  << std::endl;
        return;
      }
      std::cout << "Task path " << removed_path << " removed successfully." << std::endl;
    }

    if (!removed_command.empty()) {
      auto r = fh->delete_task_command(task_id, removed_command);
      if (r.isErr()) {
        std::cerr << "Failed to removed task command: " << r.getErrMessage()
                  << std::endl;
        return;
      }
      std::cout << "Task command " << removed_command << " removed successfully." << std::endl;
    }
    if (!command_on_success.empty()) {
      auto r = fh->delete_task_on_success(task_id, command_on_success);
      if (r.isErr()) {
        std::cerr << "Failed to remove task on success: " << r.getErrMessage()
                  << std::endl;
        return;
      }
      std::cout << "Task on success command " << command_on_success << " removed successfully." << std::endl;
    }
    if (!command_on_failure.empty()) {
      auto r = fh->delete_task_on_failure(task_id, command_on_failure);
      if (r.isErr()) {
        std::cerr << "Failed to remove task on failure: " << r.getErrMessage()
                  << std::endl;
        return;
      }
      std::cout << "Task on failure command " << command_on_failure << "removed successfully." << std::endl;
    }
    if (!ignored_path.empty()) {
      auto r = fh->remove_ignored_path(task_id, ignored_path);
      if (r.isErr()) {
        std::cerr << "Failed to remove ignored path: " << r.getErrMessage()
                  << std::endl;
        return;
      }
      std::cout << "Ignored path " << ignored_path << " removed successfully." << std::endl;
    }
    if (!ignored_pattern.empty()) {
      auto r = fh->remove_ignored_pattern(task_id, ignored_pattern);
      if (r.isErr()) {
        std::cerr << "Failed to remove ignored pattern: " << r.getErrMessage()
                  << std::endl;
        return;
      }
      std::cout << "Ignored pattern " << ignored_pattern << " removed successfully." << std::endl;
    }
  });
}

} // namespace flowhook_cli
