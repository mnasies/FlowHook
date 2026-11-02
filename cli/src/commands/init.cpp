#include "../../src/include/flowhook_core.h"
#include "../include/CLI11.hpp"
#include "../../src/include/macros.hpp"
#include "../include/cli_utils.h"
#include <filesystem>


namespace fs = std::filesystem;
namespace flowhook_cli {
    void register_init(CLI::App* app, flowhook::FlowHookCore* &fh)
    {

        // init subcommand
        auto *init =
            app->add_subcommand("init", "initializing a new FlowHook task instance in the current folder");

        // this is a purely ceremonial flag(so that CLI11 won't say there is unrecognized flag)
        // the actual parsing of verbose and setting of the environment variable is done in main.cpp
        init->add_flag("--debug", FLOWHOOK_DEBUG, "Enable debug output. \n// is an extremely detailed output that shows every step of the process.");
        init->add_flag("--verbose", FLOWHOOK_VERBOSE, "Enable verbose output. \n// shows a summary of the process.");


        static std::string task_name = "";
        init->add_option("--task", task_name, "Task name to initialize [optional] \n// filename of the current folder will be taken as task name if not provided");

        init->callback([=, &fh]() mutable {
            auto _fh = flowhook::FlowHookCore::lazy_create();
            fh = handle_fh_error(_fh);

            fs::path cwd = fs::canonical(fs::current_path());

          if (task_name.empty())
            task_name = cwd.filename().string();
          std::string n_path = cwd.string();

          auto r = fh->create_task(task_name, n_path);
          if (r.isErr()) {
            std::cout << "Failed to create task: " << r.getErrMessage() << std::endl;
            return;
          }
          auto p = fh->set_task_path(n_path, n_path);
          if (p.isErr()) {
            std::cout << "Failed to set task path: " << p.getErrMessage() << std::endl;
            return;
          }

          std::cout << "Initialized flowhook task in " << n_path << std::endl;
        });
    }
}
