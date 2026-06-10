
      


# oli (One Line Interpreter) 🚀
oli (Oli Engine & Scripting Language) 🚀

**oli** is a fast, lightweight, and extensible bytecode-compiled scripting language and engine written in C++20. Originally conceived as a minimalist interpreter, it has evolved into a robust ecosystem featuring an efficient Virtual Machine (VM), object-oriented programming (OOP) structures, dynamic native plugin architectures, and its own standalone development environment (**Oli IDE**).

---

## ✨ Features

- **Bytecode Virtual Machine**: High-performance execution engine utilizing optimized bytecode instructions instead of direct AST evaluation, rendering heavy execution loops in milliseconds.
- **Dynamic Type System (`vData`)**: Flexible data handling (INT, FLOAT, STRING, BOOL, ARRAY, MAP) wrapped seamlessly using `std::variant` with smart pointer resolution (`getTrueData()`).
- **Object-Oriented Programming**: First-class support for blueprint declarations (`def class`), single inheritance (`extends`), and dynamic instance context execution (`$this`).
- **Auto-Balancing Stack**: Advanced VM compiler fallback mechanisms that automatically balance the expression stack on passive native identifiers—enabling clean JavaScript/Python-like side-effect function calls without forcing dummy assignments.
- **Dynamic Plugin Architecture**: Highly extensible via external native modules (`oli_system`, `oli_filesys`, `oli_compiler`, `oli_winui`) loaded dynamically (`.dll` on Windows / `.so` on Linux).
- **Oli IDE**: A tailored, multi-tab integrated development environment built entirely on top of the `oli_winui` native Win32 framework, featuring safe execution isolation, intelligent multi-tab tracking, line numbering (Gutter), native context menus (smart line commenting), and robust cross-cutting shortcut binds (`Ctrl+G`, `Ctrl+S`, `Ctrl+W`).

---

## 🛠️ Installation & Building

### Prerequisites
- A C++20 compliant compiler (MSVC 2019+, GCC 10+, or Clang 10+).
- Windows SDK / Common Controls library (for GUI and components).

### Building the Project
Clone the repository and compile using your preferred compiler suite or the provided Makefile:


git clone [https://github.com/minghir/oli.git](https://github.com/minghir/oli.git)
cd oli
make all

## 🛠️ Installation

### Prerequisites
- A C++20 compliant compiler (GCC 10+, Clang 10+, or MSVC 2019+).
- `readline` library (for the CLI interface on Linux).

### Building on Linux

git clone https://github.com/minghir/oli.git
cd oli
make all
./oli
```

### Building on Windows
You can use MinGW-w64 with `make` or include the source files in a Visual Studio project.

Note: For deep recursion on Windows x64, it is recommended to increase the Stack Reserve Size to 8MB in the Linker settings to prevent Stack Overflow.

📖 Language Example: Fibonacci with Memoization

```text
# Initialize global cache
set @memo = {}

func FIBO_MEMO($n)
    if $n <= 1 then return $n endif
    if @memo[$n] != NULL then return @memo[$n] endif

    set $res = FIBO_MEMO($n - 1) + FIBO_MEMO($n - 2)
    set @memo[$n] = $res
    return $res
endfunc

echo "Fibonacci(50): " .. FIBO_MEMO(50)
```

🏗️ Project Structure
src/ - Core engine logic (vOliEngine, Parser, AST).

oli_plugin/ - External plugin examples for command extensions.

🚀 Roadmap
[ ] BigInt Support: Prevent integer overflow for numbers exceeding 64-bit limits.

[ ] Integrated Debugger: Detailed traceback and step-by-step execution.

[ ] Stack Optimization: Moving towards iterative evaluation to reduce memory footprint.

📄 License
This project is licensed under the MIT License.
