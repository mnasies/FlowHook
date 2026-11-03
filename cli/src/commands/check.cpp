#include "../../src/include/flowhook_core.h"
#include "../include/CLI11.hpp"
#include "../../src/include/macros.hpp"
#include "../include/cli_utils.h"


namespace fs = std::filesystem;

namespace flowhook_cli {
    void register_check(CLI::App* app, flowhook::FlowHookCore* &fh)
    {
        auto* check = app->add_subcommand("check", "check the status of the task");

        // this is a purely ceremonial flag(so that CLI11 won't say there is unrecognized flag)
        // the actual parsing of verbose and setting of the environment variable is done in main.cpp
        check->add_flag("--debug", FLOWHOOK_DEBUG, "Enable debug output. \n// is an extremely detailed output that shows every step of the process.");
        check->add_flag("--verbose", FLOWHOOK_VERBOSE, "Enable verbose output. \n// shows a summary of the process.");


        static bool active = false;
        check->add_flag("--active", active, "Check if task is labeled as active");

        static bool deactive = false;
        check->add_flag("--deactive", deactive, "Check if task is labeled as deactive");

        static bool depth;
        check->add_flag("--depth", depth, "Outputs the watching depth set from your working directory.");

        static bool timeout = false;
        check->add_flag("--timeout", timeout, "Outputs the timeout set for the commands executed by the task.");

        check->callback([=, &fh]() mutable {
            auto _fh = flowhook::FlowHookCore::lazy_create();
            fh = handle_fh_error(_fh);
            if (fh == nullptr)
                return;

            std::string check_task_id = fs::current_path().string();

          if(!active && !deactive && !depth && !timeout) {
              if(!fh->is_task(check_task_id)) {
                  std::cout << "FlowHook task has not been initialized in the current directory." << std::endl;
                  return;
              }else {
                  std::cout << "FlowHook task is initialized in the current directory." << std::endl;
                  return;
              }
          }

          if(timeout) {
              auto r = fh->get_task_timeout(check_task_id);
              if(r.isErr()) {
                  std::cout << "Failed to get task timeout: " << r.getErrMessage()
                            << std::endl;
                  return;
              }
              std::cout << "Task timeout: " << r.unwrap() << " seconds" << std::endl;
              return;
          }

          if (active) {
            auto r = fh->is_task_active(check_task_id);
            if (r.isErr()) {
              std::cout << "Failed to check if task is active: " << r.getErrMessage()
                        << std::endl;
              return;
            }
            if (r.unwrap()) {
              std::cout << "Task " << check_task_id << " is active" << std::endl;
            } else {
              std::cout << "Task " << check_task_id << " is not active" << std::endl;
            }
          }
          if(deactive) {
              auto r = fh->is_task_active(check_task_id);
              if (r.isErr()) {
                std::cout << "Failed to check if task is deactive: " << r.getErrMessage()
                          << std::endl;
                return;
              }
              if (r.unwrap()) {
                std::cout << "Task " << check_task_id << " is not deactive" << std::endl;
              } else {
                std::cout << "Task " << check_task_id << " is deactive" << std::endl;
              }
          }
          if(depth) {
            auto r = fh->get_task_depth(check_task_id);
            if (r.isErr()) {
              std::cout << "Failed to check task depth: " << r.getErrMessage()
                        << std::endl;
              return;
            }
            std::cout << "Task depth is " << r.unwrap() << std::endl;
          }
        });
    }
}
