# AI-Shell

AI-Shell is a smart command-line interface helper that uses local AI models to translate natural language requests into executable terminal commands. It is designed to run on Windows (PowerShell environment) and integrates with a local Ollama instance.

## Features

- **Natural Language to Shell**: Translate requests like "find all huge files" into complex PowerShell commands.
- **Context Awareness**: Remembers your previous commands and session context.
- **Operational Memory**: Learns from command failures. If a command fails, it analyzes the error and saves a fix for future reference.
- **Wrapper Mode**: Support for interactive tools.
- **Privacy-Focused**: Uses local models via Ollama.

## Prerequisites

- **Ollama**: You need [Ollama](https://ollama.ai/) installed and running locally on port `11434`.
- **C++ Compiler**: A C++ compiler supporting C++17 or later (e.g., MinGW `g++`).
- **Windows**: Currently optimized for Windows/PowerShell environments.

## Installation & Build

1. Clone the repository.
2. Ensure you have the prerequisites installed.
3. run the build script:
   ```cmd
   build.bat
   ```
   This will compile the source code and place the executable in the `bin/` directory.

## Usage

Run the executable from the `bin` directory:

```cmd
bin\ai.exe <your request>
```

### Examples

**Basic Command Generation:**
```cmd
bin\ai.exe list all PDF files in the current folder modified last week
```

**Context & Chat:**
If you run `ai.exe` without arguments, it will set up an initial context. Subsequent calls will use this context.

**Reset Context:**
```cmd
bin\ai.exe --reset
```

**Clear History:**
```cmd
bin\ai.exe --clear-history
```

## Structure

- `src/`: Source code files.
- `src/terminal_memory.jsonl`: (Runtime) Stores learned error fixes.
- `src/context.json`: (Runtime) Stores current session context.
- `bin/`: Compiled executables.
