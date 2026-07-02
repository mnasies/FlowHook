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
3. [Why FlowHook?](#why-flowhook)
4. [Installation](#installation)
5. [Command Reference & Usage](#command-reference--usage)


### Introduction

FlowHook is an open-source CLI tool that can automate build processes by watching filesystem events and executing build commands automatically. It is designed to be simple, efficient, and easy to use. It enables you to define your build commands using a simple CLI interface. It supports multiple build commands. And it also enables you to hook other set of commands to be triggered after a successful build or a failed build.

It can watch your directory for changes and execute build commands automatically when changes are detected.

### Quick Demo

[![asciinema demo](https://asciinema.org/a/Wc2xprecUM1YnUK0.svg)](https://asciinema.org/a/Wc2xprecUM1YnUK0)

*Click the player above to watch a full 3-minute walkthrough of FlowHook tracking file modifications and running build commands automatically including the hook feature.*


### Why FlowHook?

FlowHook started out of personal necessity and a desire to look under the hood. A few months ago, I began diving deep into C++ and exploring systems programming. While falling in love with low-level control, I quickly hit a massive developer bottleneck: the constant, frustrating friction of having to manually re-compile and execute code every single time I made a minor file modification just to see if my changes worked. 

While heavy, pre-existing file-watching frameworks exist, I decided to build FlowHook as a raw learning tool to solve this exact pain point. As the project evolved, my focus shifted toward tailoring the tool to handle the specific real-world workflow issues I face every night, shaping it into something I actually want to use every day. 

What began as a simple automation script grew into a highly optimized, zero-bloat systems utility. Because it operates close to the iron and leverages the Linux kernel’s native `inotify` subsystem, it monitors deep project directories with near-zero CPU overhead and a virtually invisible memory footprint. 

### What Makes it Different?
While there are other file watchers out there, FlowHook was designed with a few core strengths that are difficult to find out-of-the-box in popular alternatives:

* **True Persistence:** FlowHook configurations don't evaporate. Your active watch environments and tasks are securely serialized to disk. If your system fully shuts down or reboots, FlowHook recovers its exact state gracefully on reload without locking up on stale paths. Once you set up your system you only need to run the `flowhook run` command whenever you want to start monitoring your project and start a session.
* **Multi-Project Task Orchestration:** It isn't restricted to a single folder. A single instance can keep track of completely separate projects, mapping out individual boundaries and firing off independent hook configurations concurrently.
* **Contextual Branching (`on_success` / `on_failure`):** Instead of blindly executing a single catch-all command string on save, FlowHook intrinsically tracks execution exit states. This allows you to uniquely fork separate execution paths—like running an extensive test suite only if the compilation passes, or sending a desktop notification the exact millisecond a build breaks.
* **Language Agnostic:** Though engineered in native C++, FlowHook is entirely toolchain-agnostic. Whether you are compiling C++, watching TypeScript builds, restarting a Go backend, or running Python scripts, it hooks into any CLI pipeline seamlessly.
* **Session Tracking:** FlowHook keeps track of your active sessions, and logs information about each of your sessions, into a log file called `<YOUR_PROJECT_NAME>-flowhook.log` right in your project directory. So you can easily review the history of your sessions and your build history including the terminal output and several other informations, and debug any issues that arise at any time.

### Installation

#### Option 1

This is the easiest way to get started with FlowHook.

1. You can grab the latest release from the [GitHub releases page](https://github.com/etcoder-642/FlowHook/releases/latest). Steps to download the binary release can be found in the release page.

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
3. Prevent heavy transient directories from overloading tracking tables
```bash
flowhook add --ignored-path "build/" --ignored-pattern "*.o"
```
4. Spin up the background thread filewatcher engine
```bash
flowhook run
```

### Global Flags
These flags can be appended to the base flowhook command from anywhere:

`-h`, `--help` — Displays structural help layouts and options for any command or subcommand.

`--version` — Prints the active engine version layout (0.0.0) and immediately exits.

`--verbose` — Enables verbose logging output.

### Subcommands

*Tip: Use `flowhook --help <subcommand>` to see detailed usage and options for each subcommand.

#### 1. init
Initializes a flowhook instance in your current directory and registers it inside a config.json file under `$HOME/.config/flowhook/config.json`.

Usage: `flowhook init`

Options: 
  `--task TEXT`[OPTIONAL] — enables you to name your flowhook instance a unique name. If not provided the filename of your current directory will be taken as name. 

#### 2. add

Usage: `flowhook add [options]`

Options:

`--command "<cmd>"` — The base target compilation or execution string to trigger on file change.

`--on-success "<cmd>"` — Hook execution path to fork immediately if the base target command returns exit code 0.

`--on-failure "<cmd>"` — Hook execution path to fork immediately if the base target command returns a non-zero exit code.

`--ignored-path "<path>"` — Tells the internal filewatcher module to explicitly skip registering inotify bounds on this directory.

`--ignored-pattern "<glob>"` — Skips filing tracking updates for files matching specific extensions (e.g., *.tmp, *.o).

#### 3. list
Dumps an immediate structural breakdown of the currently registered configuration data into your terminal.

Usage: `flowhook list [options]`

Flags:

  `--tasks` — lists all registered tasks and their associated paths in your machine.
  
  `--paths` — lists all registered paths and watched paths in the working directory.

  `--commands` — lists all build commands registered in the current directory.

  `--ignored` — lists all ignored paths and patterns in the current directory.

  `--on-success` — lists all on success commands to run in the current directory.

  `--on-failure` — lists all on failure commands to run in the current directory.

#### 4. remove
Drops a specified path boundary, target match pattern, or hook configuration from the active registry.

Usage: `flowhook remove [options]`

Options:

`-f` — Bypasses structural verification checks to force immediate state eviction.

#### 5. set
Tweaks global engine performance constants and environmental execution behaviors.

Usage: `flowhook set [options]`

Options:

`--depth <int>` — Adjusts recursive folder watch limits. Accepts integers mapping from 0 (shallow directory tracking) up to 10.

Flag:

 `--active` — sets the the initialized task in the current directory as active. This enables you to watch for only tasks labeled as active later using the command `flowhook run --active`

#### 6. check
It checks for the status set by the `set` command.

Usage: `flowhook check`
Options: 
  `--depth`  - Adjusts recursive folder watch limits.
  `--active`  - checks if current flowhook instance in the current working directory is labeled active.
  `--deactive`  - checks if current flowhook instance in the current working directory is labeled deactive.

#### 7. run
Spins up the underlying filewatcher background-thread, mounts native kernel inotify tracking hooks across the working directory, and starts orchestrating loops.

Usage: `flowhook run`

Flags:

 `--quiet` — Silences non-essential lifecycle output streams, funneling thread-safe logs strictly to background buffers instead of flooding stdout.

 `--all` - Runs all registered tasks across the entire machine.
 `--active` - Runs all tasks that are labeled as active.

Your complete layout is officially complete.

## Author

[mnasie](https://github.com/etcoder-642) | aspiring systems engineer

## Used 

- [nlohmann/json library](https://github.com/nlohmann/json)
- [CLI11 library](https://github.com/CLIUtils/CLI11)

```
