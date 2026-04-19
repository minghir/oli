
      


# oli (One Line Interpreter) 🚀

**oli** is a lightweight, modular, and extensible scripting engine written in C++20. Originally conceived as a "One Line Interpreter," it has evolved into a robust language capable of handling complex data structures, deep recursion with memoization, and external extensibility via plugins.

## ✨ Features
- **vData System**: Flexible type handling (INT, FLOAT, STRING, BOOL, ARRAY, MAP) powered by `std::variant`.
- **Native Memoization**: Built-in support for optimizing recursive algorithms (e.g., calculating Fibonacci(100) in milliseconds).
- **Advanced Scoping**: Local and global variable management via a structured Call Stack.
- **AST-Driven**: Robust expression and logic evaluation using an Abstract Syntax Tree.
- **Plugin System**: Extend core functionality through dynamic libraries (`.dll` on Windows / `.so` on Linux).
- **Cross-Platform**: Fully functional on both Windows (x86/x64) and Linux.

## 🛠️ Installation

### Prerequisites
- A C++20 compliant compiler (GCC 10+, Clang 10+, or MSVC 2019+).
- `readline` library (for the CLI interface on Linux).

### Building on Linux
```bash
git clone https://github.com/minghir/oli.git
cd oli
make all
./oli


Building on Windows
You can use MinGW-w64 with make or include the source files in a Visual Studio project.

Note: For deep recursion on Windows x64, it is recommended to increase the Stack Reserve Size to 8MB in the Linker settings to prevent Stack Overflow.

📖 Language Example: Fibonacci with Memoization


# Initialize global cache
set $memo = []

func FIBO_MEMO n
    # Base cases
    if $n <= 1 then 
        return $n 
    endif

    # Check cache
    if @memo[$n] != NULL then 
        return @memo[$n] 
    endif

    # Recursive step
    set $res = FIBO_MEMO($n - 1) + FIBO_MEMO($n - 2)
    
    # Store in global cache
    set global @memo[$n] = $res
    
    return $res
endfunc

echo "Fibonacci(50) Result: " + FIBO_MEMO(50)

🏗️ Project Structure
src/ - Core engine logic (vOliEngine, Parser, AST).

include/ - Header files and interface definitions.

oli_plugin/ - External plugin examples for command extensions.

build/ - Compiled object files.

🚀 Roadmap
[ ] BigInt Support: Prevent integer overflow for numbers exceeding 64-bit limits.

[ ] Object Oriented Programming: Implementation of "Blueprints" (Classes and Objects).

[ ] Integrated Debugger: Detailed traceback and step-by-step execution.

[ ] Stack Optimization: Moving towards iterative evaluation to reduce memory footprint.

📄 License
This project is licensed under the MIT License.
