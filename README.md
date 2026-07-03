# FlowHook

 ```text
 ███████╗██╗      ██████╗ ██╗    ██╗██╗  ██╗ ██████╗  ██████╗ ██╗  ██╗    
 ██╔════╝██║     ██╔═══██╗██║    ██║██║  ██║██╔═══██╗██╔═══██╗██║ ██╔╝    
 █████╗  ██║     ██║   ██║██║ █╗ ██║███████║██║   ██║██║   ██║█████╔╝     
 ██╔══╝  ██║     ██║   ██║██║███╗██║██╔══██║██║   ██║██║   ██║██╔═██╗     
 ██║     ███████╗╚██████╔╝╚███╔███╔╝██║  ██║╚██████╔╝╚██████╔╝██║  ██╗    
 ╚═╝     ╚══════╝ ╚═════╝  ╚══╝╚══╝ ╚═╝  ╚═╝ ╚═════╝  ╚═════╝ ╚═╝  ╚═╝    

```

[![Build Status](https://github.com/etcoder-642/FlowHook/actions/workflows/tests.yml/badge.svg)](https://github.com/etcoder-642/FlowHook/actions) 
![C++ Version](https://img.shields.io/badge/Language-C%2B%2B17%2F20-blue?logo=cplusplus)
![Platform](https://img.shields.io/badge/Platform-Linux-orange?logo=linux)
![License](https://img.shields.io/badge/License-MIT-green)


## Table of Contents
1. [Introduction](#introduction)
2. [Quick Demo](#quick-demo)
3. [Installation](#installation)
4. [Command Reference & Usage](#command-reference--usage)
5. [Limitations](#limitations)
6. [Contributing](#contributing)


### Introduction

Flowhook is a lightweight C++ file-watcher CLI using Linux inotify. It allows you to define build commands, branch on success/failure, log your build history to a log file and persist watch state across reboots.

### Quick Demo

[![asciinema demo](https://asciinema.org/a/Wc2xprecUM1YnUK0.svg)](https://asciinema.org/a/Wc2xprecUM1YnUK0)

*Click the player above to watch a full 3-minute walkthrough of FlowHook tracking file modifications and running build commands automatically including the hook feature.*

### What Makes it Different?
While there are other file watchers out there, FlowHook was designed with a few core strengths that are difficult to find out-of-the-box in popular alternatives:

* **True Persistence:** FlowHook configurations are saved to a config file, giving it true persistence. Once you set up your project configurations you only need to run the `flowhook run` command whenever you want to start monitoring your project and run your commands at file change.
* **Contextual Branching (`on_success` / `on_failure`):** It allows you to attach different commands to run based on the exit state of the previous command, so you can run different commands depending on whether the build succeeded or failed. Use cases include running different test suites depending on the build outcome, or sending desktop notifications based on the build result.
* **Language Agnostic:** Though engineered in native C++, FlowHook is entirely toolchain-agnostic. Whether you are compiling C++, watching TypeScript builds, restarting a Go backend, or running Python scripts, it hooks into any CLI pipeline seamlessly.
* **Logs Everything:** FlowHook keeps track of your active sessions, and logs information about each of your sessions, into a log file called `<YOUR_PROJECT_NAME>-flowhook.log` right in your project directory. So you can easily review the history of your sessions and your build history including the terminal output and several other informations. Use cases include debugging build issues, tracking build history, and monitoring build outcomes.

### Installation

#### Option 1

This is the easiest way to get started with FlowHook.

1. Grab the latest release from the [GitHub releases page](https://github.com/etcoder-642/FlowHook/releases/latest).
2. Unzip the downloaded file.
```bash
unzip flowhook.zip 
```
3. Grant execute permissions to the `flowhook` binary.
```bash
chmod +x flowhook
```
4. Move the `flowhook` binary to a directory in your `PATH` (e.g. `/usr/local/bin`).
```bash
sudo mv flowhook /usr/local/bin/
```

#### Option 2

If you prefer to compile from source or are modifying the codebase, ensure your system has a modern C++ toolchain supporting C++17/20, cmake, and make.

On Ubuntu/Debian or WSL (Ubuntu), install the build requirements:

```Bash
sudo apt update && sudo apt install -y build-essential cmake
```
Then compile and install:
1. Clone and navigate into the project

```Bash
git clone https://github.com/etcoder-642/FlowHook.git
cd FlowHook
```
2. Create and enter a clean build directory
```Bash
mkdir build && cd build
```

3. Generate an optimized Release build configuration
```Bash
cmake -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release ..
```
4. Compile the binary using all available CPU cores(to save some of your time😅)
```Bash
make -j$(nproc)
```
5. Install system-wide to `/usr/local/bin`
```Bash
sudo make install
```

#### Verifying the Installation

To verify the installation, run `flowhook --version` and ensure it outputs the version number.

## Command Reference & Usage

FlowHook provides a Git-like subcommand interface to seamlessly configure, track, and monitor your development pipelines.

### Quick Start Workflow

1. Initialize a new FlowHook environment in your project root
```bash
flowhook init
```
2. Add your execution bounds and build/test targets
```bash
flowhook add --command "make" --on-success "./run_tests" --on-failure "echo 'Fix the build!'"
```
3. Ignore build artifacts and transient directories from being tracked by FlowHook
```bash
flowhook add --ignored-path "build/" --ignored-pattern "*.o"
```
4. Spin up the background thread filewatcher
```bash
flowhook run
```

### Global Flags
These flags can be appended to the base flowhook command from anywhere:

`-h`, `--help` — Displays help information for the base flowhook command.

`--version` — Prints the version of the installed flowhook engine.

`--verbose` — Enables verbose logging output. *it is not global flag and must be used with a base flowhook subcommand. But all subcommands support it.*

`--debug` — Enables debug logging output. This is useful for troubleshooting and seeing detailed log output. *it is not global flag and must be used with a base flowhook subcommand. But all subcommands support it.*

### Subcommands

*Tip: Use `flowhook --help <subcommand>` to see detailed usage and options for each subcommand.

#### 1. init
Initializes a flowhook instance in your current directory and registers it inside a config.json file under `$HOME/.config/flowhook/config.json`.

Usage: `flowhook init`

Options: 
  `--task TEXT`[OPTIONAL] — enables you to name your flowhook instance a unique name. If not provided the filename of your current directory will be taken as name. 

#### 2. add
To register a property to your flowhook instance, use the `add` subcommand.

Usage: `flowhook add [options]`

Options:

`--command "<cmd>"` — The base target command to be executed on file change.

`--path "<path>"` — enables you to specify a path any directory or file from any where from your machine to be watched for file changes. *If it's a directory, all files within it until the depth limit is reached will be watched recursively.*

`--on-success "<cmd>"` — Hook a separate command for execution if the base target command returns exit code 0.

`--on-failure "<cmd>"` — Hook a separate command for execution if the base target command returns a non-zero exit code.

`--ignored-path "<path>"` — register a directory to be ignored by the filewatcher.

`--ignored-pattern "<glob>"` — register a file pattern to be ignored by the filewatcher. (e.g., `*.tmp`, `*.o`)

#### 3. list
To list all registered tasks, paths, commands, and ignored patterns, in your flowhook instance in the current directory, use the `list` subcommand.

Usage: `flowhook list [options]`

Flags:

  `--tasks` — lists all registered tasks and their associated paths in your machine.
  
  `--paths` — lists all registered paths and watched paths in the current directory.

  `--commands` — lists all build commands registered in the current directory.

  `--ignored` — lists all ignored paths and patterns in the current directory.

  `--on-success` — lists all on success commands to run in the current directory.

  `--on-failure` — lists all on failure commands to run in the current directory.

#### 4. remove
To remove a registered task, path, command, or ignored pattern, use the `remove` subcommand.

Usage: `flowhook remove [options]`

*if no options are provided, the `remove` subcommand will remove the flowhook configuration from the current directory, with a confirmation dialog.

Options:

`-f` — Bypasses verification checks to force immediate removal of the flowhook configuration in the current directory.

`--command "<cmd>"` — removes a base target command from the flowhook configuration.

`--path "<path>"` — removes a path from the flowhook configuration.

`--on-success "<cmd>"` — removes a separate command for execution if the base target command returns exit code 0.

`--on-failure "<cmd>"` — removes a separate command for execution if the base target command returns a non-zero exit code.

`--ignored-path "<path>"` — removes a directory to be ignored by the filewatcher.

`--ignored-pattern "<glob>"` — removes a file pattern to be ignored by the filewatcher. (e.g., `*.tmp`, `*.o`)

#### 5. set
To change different settings in the flowhook configuration, use the `set` subcommand.

Usage: `flowhook set [options]`

Options:

`--depth <int>` — Adjusts recursive folder watch limits. Accepts integers mapping from 0(shallow tracking) up to 10(deep recursive tracking).

Flag:

 `--active` — sets the the initialized task in the current directory as active. This enables you to watch for only tasks labeled as active later using the command `flowhook run --active`

#### 6. check
It checks for the status set by the `set` command.

Usage: `flowhook check`
Options: 
  `--depth`  - outputs the recursive folder watch limits set for the flowhook configuration in the current directory.
  `--active`  - checks if current flowhook instance in the current working directory is labeled active.
  `--deactive`  - checks if current flowhook instance in the current working directory is labeled deactive.

#### 7. run
Spins up the underlying filewatcher background-thread. Mounts registered commands and hooks for execution.

Usage: `flowhook run`

Flags:

 `--quiet` — Silences non-essential build terminal output streams, funneling terminal logs strictly to the log file instead of flooding stdout.

 `--all` - Runs all registered tasks across the entire machine.
 `--active` - Runs all tasks that are labeled as active.

## Limitations

FlowHook is early and has some known gaps:

- **Linux only.** Relies directly on the kernel's `inotify` subsystem. No macOS or Windows support. Planned in the near term.
- **Sequential command execution.** Commands run one at a time, not concurrently. A thread pool for concurrent execution is planned.
- **No write-ahead logging (WAL).** Config writes and log writes aren't crash-safe. A failure mid-write can corrupt the config file, losing all registered projects. Same risk applies to session logs.
- **No daemon/service mode.** `flowhook run` is a foreground process; use `nohup`, `tmux`, or a systemd unit if you need it to survive terminal closure.

These are known gaps, not surprises. Issues and PRs addressing any of them are welcome.

## Contributing
FlowHook is early-stage and contributions are genuinely welcome, from bug fixes to feature ideas.

- Found a bug? [Open an issue](https://github.com/etcoder-642/FlowHook/issues).
- Have a feature in mind? Open an issue to discuss before submitting a large PR.
- **PRs should target the `dev` branch, not `main`.** `main` is reserved for tagged releases. I merge from `dev` myself once changes are reviewed and versioned.

No formal contribution guide yet. For now, fork, branch off `dev`, and open a PR against `dev`.

## Author

[mnasie](https://github.com/etcoder-642) | aspiring systems engineer

## Used 

- [nlohmann/json library](https://github.com/nlohmann/json)
- [CLI11 library](https://github.com/CLIUtils/CLI11)

```
