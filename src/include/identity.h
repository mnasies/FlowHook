#pragma once
#include <string>

#include "types.h"
#include "error/result.h"

namespace flowhook {

    // Project-local identity marker. Every directory registered with FlowHook
    // carries a .flowhook file whose uuid mirrors the task's uuid in the
    // global config. A uuid match means "this is the same task", no matter
    // where the directory moved.
    constexpr const char* MARKER_FILE_NAME = ".flowhook";
    constexpr int MARKER_VERSION = 1;

    struct Identity {
        std::string uuid = "";
        std::string task_name = "";

        bool isNull() const { return uuid.empty(); }
    };

    // v4-style random uuid (8-4-4-4-12)
    std::string generate_uuid();

    // Writes <dir>/.flowhook. Fails on IO errors and missing directories.
    Result<void> write_marker(const std::string &dir, const Task &task);

    // Reads <dir>/.flowhook. PATH_NOT_FOUND when no marker exists,
    // CONFIG_PARSE_FAILED when it exists but is corrupted.
    Result<Identity> read_marker(const std::string &dir);

    // Deletes <dir>/.flowhook. Ok when the marker is already absent.
    Result<void> delete_marker(const std::string &dir);

    // <dir>/.flowhook as a path string
    std::string marker_path(const std::string &dir);
}
