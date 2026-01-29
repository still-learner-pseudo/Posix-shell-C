# Custom POSIX Shell in C

[![Build Status](https://backend.codecrafters.io/progress/shell/37339c58-61ea-4bcf-90de-aea1198bf763)](https://app.codecrafters.io/users/codecrafters-bot?r=2qF)

A feature-rich, POSIX-compliant Unix shell implementation built from scratch in C, demonstrating proficiency in systems programming, process management, and low-level I/O operations.

---

## Project Overview

This project is a fully functional Unix shell developed as part of the [CodeCrafters "Build Your Own Shell" Challenge](https://app.codecrafters.io/courses/shell/overview). It implements core shell functionality including command parsing, process execution, I/O redirection, pipelines, and command history—all written in pure C with minimal dependencies.

### Key Engineering Highlights

- **Complex String Parsing**: Robust tokenization engine handling quoted strings, escape sequences, and edge cases
- **Process Management**: Multi-process coordination using `fork()`, `exec()`, and `wait()` system calls
- **Inter-Process Communication**: Full pipeline implementation with `pipe()` system calls for chaining commands
- **File Descriptor Manipulation**: Advanced I/O redirection using `dup2()` for stdin/stdout/stderr
- **Memory Management**: Careful allocation and deallocation to prevent memory leaks
- **GNU Readline Integration**: Interactive command-line editing with tab completion and history

---

## Features

### Core Shell Functionality

#### 1. **Built-in Commands**
- `exit [n]` - Exit shell with optional status code
- `echo <text>` - Print text to stdout with escape sequence support
- `type <command>` - Display command type (builtin vs. external)
- `pwd` - Print current working directory
- `cd <path>` - Change directory (supports `~` for home directory)
- `history` - Display command history

#### 2. **External Command Execution**
- Searches `$PATH` directories to locate and execute binaries
- Full argument passing with proper memory management
- Exit status propagation from child processes

#### 3. **Advanced Parsing**
- **Single quotes** (`'text'`) - Literal string interpretation
- **Double quotes** (`"text"`) - String with escape sequence processing
- **Escape sequences** - Backslash escaping for special characters (`\"`, `\\`, `\$`)
- **Adjacent token concatenation** - Handles `abc"def"'ghi'` → `abcdefghi`
- **Whitespace handling** - Proper tokenization across spaces and tabs

#### 4. **I/O Redirection**
- Input redirection: `< file`
- Output redirection: `> file` (overwrite)
- Append redirection: `>> file`
- Error redirection: `2> file` and `2>> file`
- Works with both built-in and external commands

#### 5. **Pipeline Support**
- Multi-command pipelines: `cmd1 | cmd2 | cmd3 | ...`
- Supports up to 16 chained commands
- Proper file descriptor management across process boundaries
- Handles both built-in and external commands in pipelines

#### 6. **Command History**
- Persistent history via `$HISTFILE` environment variable
- History navigation with arrow keys (GNU Readline)
- `history` command to view previous commands
- Automatic save/load on shell startup/exit

#### 7. **Tab Completion**
- Auto-completion for built-in commands
- Auto-completion for executables in `$PATH`
- Duplicate detection and alphabetical ordering
- Real-time directory scanning for available commands

---

## Architecture & Design

### Code Organization

```
src/main.c              # Main implementation (~690 lines)
├── Parsing Engine      # Tokenization and argument extraction
├── Built-in Handlers   # Command implementations
├── Process Management  # Fork/exec for external commands
├── Redirection Logic   # File descriptor manipulation
├── Pipeline Execution  # Multi-process pipe coordination
├── History Management  # File I/O for command persistence
└── Tab Completion      # Readline integration
```

### Key Technical Components

#### **Parsing Engine** (`parseInput`)
- State-machine based tokenizer
- Handles nested quotes and escape sequences
- Supports concatenation of adjacent tokens
- Extracts redirection operators during parsing

#### **Redirection Wrapper** (`run_with_redirection`)
- Generic wrapper for built-in commands
- Saves/restores file descriptors
- Opens files with appropriate modes (`O_RDONLY`, `O_WRONLY`, `O_CREAT`, `O_APPEND`)
- Ensures clean state restoration even on errors

#### **Pipeline Executor** (`execute_pipeline`)
- Creates N-1 pipes for N commands
- Forks child processes for each command
- Sets up `stdin`/`stdout` chaining via `dup2()`
- Parent waits for all children to complete

#### **Command Lookup** (`isValidInPath`)
- Parses `$PATH` environment variable
- Iterates through directories to find executables
- Uses `access()` with `X_OK` to verify executability
- Returns full path for `exec()` family calls

#### **Tab Completion** (`command_generator`, `my_completion`)
- Builds match list on first invocation
- Scans `$PATH` directories using `opendir()`/`readdir()`
- Filters by `stat()` to verify executable permissions
- Prevents duplicates across multiple directories

---

## Technical Skills Demonstrated

### Systems Programming
- **Process Control**: `fork()`, `execvp()`, `wait()`, `waitpid()`
- **File I/O**: `open()`, `close()`, `dup2()`, `pipe()`
- **Directory Operations**: `opendir()`, `readdir()`, `stat()`
- **Environment Variables**: `getenv()`, `$PATH` parsing

### Memory Management
- Dynamic allocation with `malloc()`, `realloc()`, `free()`
- String duplication with `strdup()`
- Proper cleanup in error paths
- Prevention of memory leaks in iterative loops

### String Processing
- Custom parser for shell grammar
- State machine for quote handling
- Escape sequence interpretation
- Tokenization with `strtok_r()` for thread safety

### Error Handling
- Return code checking for system calls
- Graceful degradation on errors
- User-friendly error messages
- Resource cleanup on failure paths

---

## Building & Running

### Prerequisites
- **C Compiler**: GCC or Clang
- **CMake**: Version 3.10 or higher
- **GNU Readline**: Development libraries
  ```bash
  # Ubuntu/Debian
  sudo apt-get install libreadline-dev
  
  # macOS
  brew install readline
  ```

### Build Instructions

```bash
# Clone the repository
git clone <repository-url>
cd codecrafters-shell-c

# Build with CMake
mkdir -p build
cd build
cmake ..
make

# Or use the provided script
./your_program.sh
```

### Running the Shell

```bash
# Start interactive shell
./shell

# Run with history file
HISTFILE=~/.shell_history ./shell
```

---

## Usage Examples

### Basic Commands
```bash
$ pwd
/home/user/projects

$ cd ~
$ pwd
/home/user

$ echo "Hello, World!"
Hello, World!
```

### Redirection
```bash
$ echo "error message" 2> error.log
$ cat error.log
error message

$ ls -la > files.txt
$ cat files.txt
# (directory listing)
```

### Pipelines
```bash
$ cat file.txt | grep "pattern" | wc -l
42

$ ps aux | grep shell | awk '{print $2}'
12345
```

### Tab Completion
```bash
$ ec<TAB>        # Completes to 'echo'
$ /bin/ba<TAB>   # Shows 'bash', 'base64', etc.
```

### Command History
```bash
$ history
1  pwd
2  cd ~
3  echo "test"

$ # Use arrow keys to navigate history
```

---

## Testing

The implementation passes all CodeCrafters test stages including:

- Basic REPL loop
- Invalid command handling
- Built-in commands (`exit`, `echo`, `type`, `pwd`, `cd`)
- External command execution
- Quote and escape handling
- I/O redirection (input, output, append, stderr)
- Multi-command pipelines
- Command history persistence
- Tab completion

---

## Learning Outcomes

Through this project, I gained hands-on experience with:

1. **Unix System Calls**: Deep understanding of process creation, file descriptors, and inter-process communication
2. **Shell Internals**: How shells parse input, resolve commands, and manage processes
3. **Low-Level C Programming**: Pointer manipulation, memory management, and string processing
4. **Debugging Skills**: Using GDB and Valgrind to track down segfaults and memory leaks
5. **Software Architecture**: Designing modular, maintainable code for a complex system
6. **POSIX Standards**: Understanding shell behavior as defined by POSIX specifications

---

## Future Enhancements

Potential improvements to explore:

- Job control (`bg`, `fg`, `jobs`, Ctrl-Z)
- Variable expansion (`$VAR`, command substitution)
- Conditional execution (`&&`, `||`)
- Subshells and command grouping
- Wildcard expansion (`*`, `?`, `[...]`)
- Signal handling (Ctrl-C, SIGINT, SIGTERM)
- Shell scripting support (reading from files)
- Custom prompt configuration

---

## Technical Notes

### Design Decisions

1. **Single-file architecture**: Kept all code in one file for simplicity and build speed
2. **Static array for built-ins**: Faster lookup than hash table for small command set
3. **Pipeline limit of 16**: Reasonable constraint preventing excessive process creation
4. **GNU Readline integration**: Leverages battle-tested library instead of reinventing input handling

### Known Limitations

- No variable substitution or environment variable expansion
- No wildcard/glob expansion
- No shell scripting (control flow, loops, functions)

---

## License

This project was created as part of the CodeCrafters challenge. See [CodeCrafters](https://codecrafters.io) for more information.

---

## Acknowledgments

- **CodeCrafters**: For providing the structured challenge and testing infrastructure
- **GNU Readline**: For the robust command-line editing library
- **POSIX Standards**: For defining shell behavior and system call interfaces

---

## Author
**Saathvikk Muthyala**
Built with ❤️ as a learning project to understand Unix systems programming and shell internals.
