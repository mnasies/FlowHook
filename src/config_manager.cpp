#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <filesystem>
#include <cstdlib>

#include <chrono>
#include <format>

#include "include/config_manager.h"
#include "include/macros.hpp"
#include "version.h"

namespace fs = std::filesystem;
using namespace nlohmann;
using namespace std;

namespace flowhook
{
    std::filesystem::path get_config_path()
    {
        const char *override = getenv("FLOWHOOK_CONFIG_DIR_TEST");
        const char *home = std::getenv("HOME");

        if (override)
        {
            return std::filesystem::path(override) / "config.json";
        }
        return std::filesystem::path(home) / ".config" / "flowhook" / "config.json";
    }

    Result<void> ensure_config_dir()
    {
        std::error_code ec;
        std::filesystem::create_directories(get_config_path().parent_path(), ec);
        if (ec)
        {
            return Result<void>::Err(FWError::make(ErrorCode::SYS_IO_FAILED, ec.message()));
        }
        return Result<void>::Ok();
    }

    Result<ConfigManager *> ConfigManager::create()
    {
        ConfigManager *cm = new ConfigManager();
        auto config_path = get_config_path();
        if (!fs::exists(config_path))
        {
            fs::create_directories(config_path.parent_path());
            ofstream(config_path).close();
            auto now = std::chrono::system_clock::now();
            auto ts = std::format("{:%Y-%m-%dT%H:%M:%SZ}", now);

            cm->config_obj["version"] = FLOWHOOK_VERSION;
            cm->config_obj["created_at"] = ts;
            cm->config_obj["last_modified"] = ts;
            cm->config_obj["tasks"] = json::array();
        }
        else
        {
            fstream file;
            file.open(config_path, ios::in | ios::out);
            if (!file.is_open())
            {
                return Result<ConfigManager *>::Err(FWError::make(
                    ErrorCode::SYS_IO_FAILED, "Error: opening config file"));
            }
            TEST_OVERLOADED(cm->load(file), ConfigManager *);
            file.close();
        }
        return Result<ConfigManager *>::Ok(cm);
    }

    ConfigManager::~ConfigManager()
    {
        if (!isflushed)
            flush();
    }

    Result<void> ConfigManager::load(std::fstream &file)
    {
        if (file.peek() == ifstream::traits_type::eof())
        {
            return Result<void>::Ok();
        }
        else
        {
            try {
                config_obj = json::parse(file);
            }
            catch (const json::parse_error& e)
            {
                return Result<void>::Err(FWError::make(
                    ErrorCode::CONFIG_PARSE_FAILED, "Error: parsing config file failed"));
            }
        }

        if (file.fail())
        {
            return Result<void>::Err(FWError::make(
                ErrorCode::SYS_IO_FAILED, "Error: reading config file failed"));
        }
        return Result<void>::Ok();
    }

    Result<Task> ConfigManager::convert_json_to_task(const json &json_task)
    {
        Task _task;
        _task.name = json_task.at("task_name");
        _task.id = json_task.at("working_directory");

        vector<string> _commands;
        for (auto &cmd : json_task.at("commands"))
        {
            _commands.push_back(cmd);
        }
        _task.commands = _commands;

        vector<string> _file_paths;
        for (auto &cmd : json_task.at("file_paths"))
        {
            _file_paths.push_back(cmd);
        }
        _task.file_paths = _file_paths;

        vector<string> _dir_paths;
        for (auto &cmd : json_task.at("dir_paths"))
        {
            _dir_paths.push_back(cmd);
        }
        _task.dir_paths = _dir_paths;

        vector<string> _on_success;
        for (auto &cmd : json_task.at("on_success"))
        {
            _on_success.push_back(cmd);
        }
        _task.on_success = _on_success;

        vector<string> _on_failure;
        for (auto &cmd : json_task.at("on_failure"))
        {
            _on_failure.push_back(cmd);
        }
        _task.on_failure = _on_failure;

        vector<string> _ignored_paths;
        for (auto &path : json_task.at("ignored_paths"))
        {
            _ignored_paths.push_back(path);
        }
        _task.ignored_paths = _ignored_paths;

        vector<string> _ignored_patterns;
        for (auto &pattern : json_task.at("ignored_patterns"))
        {
            _ignored_patterns.push_back(pattern);
        }
        _task.ignored_patterns = _ignored_patterns;

        _task.isActive = json_task.at("isActive");
        _task.watching_depth = json_task.at("watching_depth");
        return Result<Task>::Ok(_task);
    }

    Result<json> ConfigManager::convert_task_to_json(const Task &task)
    {
        json _json_task = json::object();
        _json_task["task_name"] = task.name;
        _json_task["working_directory"] = task.id;
        _json_task["commands"] = json::array();
        for (auto &cmd : task.commands)
        {
            _json_task["commands"].push_back(cmd);
        }
        _json_task["file_paths"] = json::array();
        for (auto &path : task.file_paths)
        {
            _json_task["file_paths"].push_back(path);
        }

        _json_task["dir_paths"] = json::array();
        for (auto &path : task.dir_paths)
        {
            _json_task["dir_paths"].push_back(path);
        }

        _json_task["on_success"] = json::array();
        for (auto &cmd : task.on_success)
        {
            _json_task["on_success"].push_back(cmd);
        }
        _json_task["on_failure"] = json::array();
        for (auto &cmd : task.on_failure)
        {
            _json_task["on_failure"].push_back(cmd);
        }
        for (auto &path : task.ignored_paths)
        {
            _json_task["ignored_paths"].push_back(path);
        }
        for (auto &pattern : task.ignored_patterns)
        {
            _json_task["ignored_patterns"].push_back(pattern);
        }
        _json_task["isActive"] = task.isActive;
        _json_task["watching_depth"] = task.watching_depth;
        return Result<json>::Ok(_json_task);
    }

    Result<void> ConfigManager::log_task(const Task &task)
    {
        // check if task is empty
        if (task.isNull())
        {
            return Result<void>::Err(FWError::make(
                ErrorCode::EMPTY_VALUE, "Error: task is empty"));
        }
        // check if task already exists
        for (auto &t : config_obj["tasks"])
        {
            Task _ts = TRY(convert_json_to_task(t), void);
            if (_ts.id == task.id)
            {
                return Result<void>::Err(FWError::make(
                    ErrorCode::TASK_ALREADY_EXISTS, "Error: task already exists, use update_task to update it"));
            }
        }

        json _json_task = TRY(convert_task_to_json(task), void);
        config_obj["tasks"].push_back(_json_task);

        return Result<void>::Ok();
    }

    Result<void> ConfigManager::update_task(const Task &task)
    {
        for (auto it = config_obj["tasks"].begin(); it != config_obj["tasks"].end(); it++)
        {
            string id = it->at("working_directory");
            if (id == task.id)
            {
                json _json_task = TRY(convert_task_to_json(task), void);
                *it = _json_task;
                isflushed = false;
                flush();
                return Result<void>::Ok();
            }
        }
        return Result<void>::Err(FWError::make(ErrorCode::TASK_NOT_FOUND, "Error: task not found"));
    }

    Result<void> ConfigManager::purge_config()
    {
        auto config_path = get_config_path();
        fstream file(config_path, ios::out | ios::trunc);
        if(!file.is_open())
        {
            return Result<void>::Err(FWError::make(ErrorCode::SYS_IO_FAILED, "Error: opening config file failed"));
        }
        file.close();
        config_obj = json::object();
        return Result<void>::Ok();
    }

    Result<void> ConfigManager::log_task_inbatch(const vector<Task> &tasks)
    {
        for (auto &task : tasks)
        {
            TEST(log_task(task));
        }
        return Result<void>::Ok();
    }

    Result<void> ConfigManager::delete_task(const Task &task)
    {
        for (auto it = config_obj["tasks"].begin(); it != config_obj["tasks"].end(); it++)
        {
            string id = it->at("working_directory");
            if (id == task.id)
            {
                config_obj["tasks"].erase(it);
                return Result<void>::Ok();
            }
        }
        return Result<void>::Err(FWError::make(ErrorCode::TASK_NOT_FOUND, "Error: task not found"));
    }

    Result<vector<Task>> ConfigManager::get_tasks()
    {
        vector<Task> tasks;
        for (auto it = config_obj["tasks"].begin(); it != config_obj["tasks"].end(); it++)
        {
            Task _task;
            _task.name = it->at("task_name");
            _task.id = it->at("working_directory");
            _task.commands = it->at("commands");
            _task.file_paths = it->at("file_paths");
            _task.dir_paths = it->at("dir_paths");
            _task.on_success = it->at("on_success");
            _task.on_failure = it->at("on_failure");
            _task.ignored_paths = it->at("ignored_paths");
            _task.ignored_patterns = it->at("ignored_patterns");
            _task.isActive = it->at("isActive");
            _task.watching_depth = it->at("watching_depth");
            tasks.push_back(_task);
        }
        return Result<vector<Task>>::Ok(tasks);
    }

    Result<void> ConfigManager::flush()
    {
        if (isflushed)
        {
            return Result<void>::Ok();
        }

        auto config_path = get_config_path();
        if (fs::exists(config_path))
        {
            auto now = std::chrono::system_clock::now();
            auto ts = std::format("{:%Y-%m-%dT%H:%M:%SZ}", now);

            config_obj["last_modified"] = ts;

            fstream file(config_path, ios::in | ios::out | ios::trunc);
            file << config_obj.dump(4) << endl;
            if (file.fail())
            {
                return Result<void>::Err(FWError::make(
                    ErrorCode::SYS_IO_FAILED, "Error: writing to config file failed"));
            }
            isflushed = true;
            file.close();
            return Result<void>::Ok();
        } else {
            return Result<void>::Err(FWError::make(
                ErrorCode::SYS_IO_FAILED, "Error: config file not found"));
        }
    }
}
