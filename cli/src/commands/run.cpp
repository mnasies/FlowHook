#include "../../src/include/flowhook_core.h"
#include "../../src/include/macros.hpp"
#include "../include/CLI11.hpp"
#include "../include/cli_utils.h"

#include <filesystem>

namespace fs = std::filesystem;

namespace flowhook_cli {
void register_run(CLI::App *app, flowhook::FlowHookCore *&fh) {
  auto *run = app->add_subcommand("run", "running a task");

  // this is a purely ceremonial flag(so that CLI11 won't say there is
  // unrecognized flag) the actual parsing of verbose and setting of the
  // environment variable is done in main.cpp
  run->add_flag("--debug", FLOWHOOK_DEBUG,
                "Enable debug output. \n// is an extremely detailed output "
                "that shows every step of the process.");
  run->add_flag("--verbose", FLOWHOOK_VERBOSE,
                "Enable verbose output. \n// shows a summary of the process.");
  run->add_flag("--quiet", FLOWHOOK_QUIET,
                "Removes build output from terminal");

  static bool run_all = false;
  run->add_flag("--all", run_all, "Run all task instances labeled as active.");

  static bool run_active = false;
  run->add_flag("--active", run_active, "Run active task instances");

  run->callback([=, &fh]() mutable {
      auto _fh = flowhook::FlowHookCore::create();
      fh = handle_fh_error(_fh);

      std::string run_task_id = fs::current_path().string();

    if (run_all) {
      auto r = fh->start_all();
      if (r.isErr()) {
        std::cout << "Failed to run all tasks: " << r.getErrMessage()
                  << std::endl;
        return;
      }
      pause();
    } else if (run_active) {
      auto r = fh->start_active();
      if (r.isErr()) {
        std::cout << "Failed to run active tasks: " << r.getErrMessage()
                  << std::endl;
        return;
      }
      pause();
    } else if (!run_task_id.empty()) {
      auto r = fh->start_task(run_task_id);
      if (r.isErr()) {
        std::cout << "Failed to run task: " << r.getErrMessage() << std::endl;
        return;
      }
    }
    std::cout << "Watching " << run_task_id
              << "  (use --quiet to suppress build output, press Ctrl+C to stop)" << std::endl;
    pause();
  });
}
} // namespace flowhook_cli
