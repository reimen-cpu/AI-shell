# 🤖 AI-Shell

> **Transform natural language into powerful terminal commands using local AI**

AI-Shell is an intelligent command-line assistant that bridges the gap between human language and shell commands. Simply describe what you want to do, and AI-Shell will generate, explain, and execute the appropriate PowerShell commands—all while learning from mistakes to get better over time.

---

## ✨ Key Features

- 🗣️ **Natural Language Interface**: Talk to your terminal like you would to a colleague
- 🧠 **Smart Learning**: Automatically learns from failures and suggests fixes
- 📝 **Context Awareness**: Remembers your conversation history for better suggestions
- 🔒 **Privacy-First**: Runs 100% locally using Ollama—no cloud, no data sharing
- 🎯 **Interactive Mode**: Special support for tools like Python REPL, SSH, MySQL
- ⚡ **Auto-Correction**: Detects and fixes common command issues automatically
- 💾 **Smart Cache**: Caches successful commands for instant reuse, tracking reliability
- 🔄 **Auto-Retry**: Automatically attempts different approaches when commands fail

---

## 📋 Prerequisites

Before installing AI-Shell, make sure you have:

### 1. **Ollama** (Required)
Download and install from [ollama.ai](https://ollama.ai/)

After installation, pull a recommended model:
```powershell
ollama pull codellama:latest
```

**Alternative models you can try:**
- `ollama pull llama3.2` - Faster, good for simple commands
- `ollama pull qwen2.5-coder` - Better for complex scripting
- `ollama pull deepseek-coder` - Excellent for programming tasks

### 2. **C++ Compiler** (For building from source)
- **MinGW-w64**: Download from [winlibs.com](https://winlibs.com/)
- **Visual Studio**: Community edition with C++ tools

### 3. **Windows PowerShell**
- Windows 10/11 comes with PowerShell pre-installed
- Verify: Open PowerShell and type `$PSVersionTable`

---

## 🚀 Installation

### Option A: Quick Start (Pre-compiled Binary)
1. Download the latest release from the [Releases page](#)
2. Extract `ai.exe` to a folder (e.g., `C:\Tools\AI-Shell`)
3. Add to PATH or create an alias:
   ```powershell
   # Add to your PowerShell profile
   Set-Alias ai "C:\Tools\AI-Shell\bin\ai.exe"
   ```

### Option B: Build from Source
```powershell
# Clone the repository
git clone https://github.com/yourusername/AI-shell.git
cd AI-shell

# Build the project
.\build.bat

# The executable will be in bin\ai.exe
```

---

## 📖 Usage Guide

### First-Time Setup

Run AI-Shell without arguments to initialize:
```powershell
.\bin\ai.exe
```

This will:
1. Check if Ollama is running (starts it if needed)
2. Let you select an AI model
3. Detect your environment (OS, shell, username)
4. Create configuration files

### Basic Usage

**Syntax:**
```powershell
ai <your request in natural language>
```

**Example:**
```powershell
ai find all files larger than 100MB in Downloads folder
```

**Output:**
```
> Get-ChildItem -Path "$env:USERPROFILE\Downloads" -Recurse | Where-Object {$_.Length -gt 100MB}

Execute? [Y/n]
```

Press `Y` to run the command, or `n` to cancel.

---

## 💡 Practical Examples

### File Management

**Find files by extension:**
```powershell
ai list all Python files in this directory
# → Get-ChildItem -Filter "*.py"
```

**Search for text in files:**
```powershell
ai search for "TODO" in all JavaScript files
# → Select-String -Path "*.js" -Pattern "TODO"
```

**Delete old files:**
```powershell
ai delete files older than 30 days in temp folder
# → Get-ChildItem $env:TEMP | Where-Object {$_.LastWriteTime -lt (Get-Date).AddDays(-30)} | Remove-Item
```

### Application Launching

**Open applications:**
```powershell
ai open telegram
# → Start-Process "Telegram.exe"

ai open chrome
# → Start-Process "chrome.exe"

ai open github desktop
# → Start-Process "GitHubDesktop.exe"
```

**Open websites:**
```powershell
ai open youtube
# → Start-Process "https://youtube.com"

ai search google for "PowerShell tutorials"
# → Start-Process "https://www.google.com/search?q=PowerShell+tutorials"
```

### System Information

```powershell
ai show disk space
# → Get-PSDrive -PSProvider FileSystem

ai list running processes using more than 500MB RAM
# → Get-Process | Where-Object {$_.WorkingSet -gt 500MB} | Sort-Object WorkingSet -Descending

ai check my IP address
# → (Invoke-WebRequest -Uri "https://api.ipify.org").Content
```

### Network Operations

```powershell
ai test connection to google.com
# → Test-Connection google.com

ai show all network adapters
# → Get-NetAdapter

ai download file from https://example.com/file.zip
# → Invoke-WebRequest -Uri "https://example.com/file.zip" -OutFile "file.zip"
```

### Development Tasks

```powershell
ai count lines of code in all Python files
# → (Get-ChildItem -Filter "*.py" -Recurse | Get-Content | Measure-Object -Line).Lines

ai find all git repositories in this folder
# → Get-ChildItem -Recurse -Directory -Filter ".git" | Select-Object Parent

ai compress this folder into a zip
# → Compress-Archive -Path . -DestinationPath archive.zip
```

---

## 🎛️ Advanced Features

### Interactive Wrapper Mode

For interactive tools (Python, SSH, databases), AI-Shell offers wrapper mode:

```powershell
ai --wrap python
```

This launches Python with AI monitoring, allowing it to learn from errors in your interactive session.

### Memory Management

AI-Shell learns from failures automatically, but you can optimize its memory:

```powershell
# Clean up duplicate error entries
ai --optimize-memory
```

### History Management

```powershell
# Clear conversation history (keeps learned fixes)
ai --clear-history

# Full reset (clears everything)
ai --reset
```

### Check Version

```powershell
ai --version
# → AI-Shell v1.2.0 (Cpp Edition)
```

---

## 🔧 Configuration

### System Prompt Customization

Edit `system_prompt.txt` to customize AI behavior:

```txt
CONTEXT: {ENV_BLOCK}

You generate Windows PowerShell commands. Output ONLY the command, nothing else.

COMMON APPS (use exact exe names):
- Your Custom App → YourApp.exe

RULES:
1. NO explanations, NO markdown - just the raw command
2. Use PowerShell cmdlets when possible
...
```

### Adding Custom Applications

Add your frequently used apps to the prompt:

```txt
COMMON APPS (use exact exe names):
- Spotify → Spotify.exe
- Discord → Discord.exe
- Visual Studio Code → Code.exe
```

---

## 🐛 Troubleshooting

### Ollama Not Starting

**Problem:** `Ollama is not running. Starting local server... Failed`

**Solutions:**
1. Manually start Ollama:
   ```powershell
   ollama serve
   ```
2. Check if port 11434 is blocked:
   ```powershell
   Test-NetConnection -ComputerName localhost -Port 11434
   ```
3. Reinstall Ollama from [ollama.ai](https://ollama.ai/)

### Model Not Found

**Problem:** `Error: model 'xyz' not found`

**Solution:**
```powershell
# List available models
ollama list

# Pull the model you want
ollama pull codellama:latest
```

### Commands Not Executing

**Problem:** AI generates commands but they fail

**Solutions:**
1. Check PowerShell execution policy:
   ```powershell
   Get-ExecutionPolicy
   # If Restricted, set to RemoteSigned:
   Set-ExecutionPolicy RemoteSigned -Scope CurrentUser
   ```

2. Run AI-Shell as Administrator for system commands

3. Check the error in `bin\terminal_stderr.log`

### Slow Response Times

**Problem:** AI takes too long to respond

**Solutions:**
1. Use a smaller/faster model:
   ```powershell
   ollama pull llama3.2:latest
   ```

2. Clear history to reduce context size:
   ```powershell
   ai --clear-history
   ```

3. Optimize memory:
   ```powershell
   ai --optimize-memory
   ```

### Wrong Commands Generated

**Problem:** AI suggests incorrect or dangerous commands

**Solutions:**
1. **Always review before executing** - AI-Shell asks for confirmation
2. The system learns from failures - if a command fails, it will suggest a fix next time
3. Be more specific in your request:
   - ❌ "delete files"
   - ✅ "delete .txt files in Downloads folder older than 7 days"

---

## 🛡️ Security Best Practices

1. **Always review commands before executing** - Press `n` if unsure
2. **Never run AI-Shell as Administrator** unless necessary
3. **Be specific with destructive operations** - AI needs clear intent
4. **Check generated paths** - Ensure they target the right directories
5. **Backup important data** before running bulk operations

### Example of Safe Usage

```powershell
# ❌ Dangerous - too vague
ai delete all files

# ✅ Safe - specific and limited
ai delete .log files in C:\Temp older than 30 days
```

---

## 📁 Project Structure

```
AI-Shell/
├── bin/                          # Compiled executables
│   ├── ai.exe                   # Main executable
│   ├── context.json             # Session context (auto-generated)
│   ├── terminal_memory.jsonl    # Learned fixes (auto-generated)
│   ├── command_cache.jsonl      # Cached commands (auto-generated)
│   └── system_prompt.txt        # AI instructions
├── src/                          # Source code
│   ├── main.cpp                 # Entry point
│   ├── command_processor.cpp    # Command execution
│   ├── memory.cpp               # Learning system
│   ├── http_client.cpp          # Ollama communication
│   └── ...
├── build.bat                     # Build script
├── system_prompt.txt            # Default AI prompt
└── README.md                    # This file
```

---

## 🤝 Contributing

Contributions are welcome! Areas for improvement:

- [ ] Linux/macOS support
- [ ] Bash/Zsh shell support
- [ ] GUI interface
- [ ] Plugin system
- [ ] Unit tests
- [ ] Security hardening

---

## 📄 License

[Specify your license here]

---

## 🙏 Acknowledgments

- **Ollama** - For providing the local AI infrastructure
- **CodeLlama** - Default AI model
- **Community Contributors** - For feedback and improvements

---

## 📞 Support

- **Issues**: [GitHub Issues](#)
- **Discussions**: [GitHub Discussions](#)
- **Documentation**: [Wiki](#)

---

**Made with ❤️ for developers who want to talk to their terminals**
