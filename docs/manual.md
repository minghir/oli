# OLI ENGINE - GENERAL MANUAL

Welcome to **Oli**, a powerful, Turing-complete scripting environment. This manual provides an overview of all available commands and their aliases.

## 1. SYSTEM & INTERFACE
Commands for managing the shell environment and finding help.

| Command | Alias | Description |
| :--- | :--- | :--- |
| **HELP** | `H` | Displays this manual or help for a specific command. |
| **INFO** | `D` | Displays technical information about the engine. |
| **CLEAR** | `CLS` | Clears the console screen. |
| **QUIT** | `EXIT`, `Q` | Safely closes the Oli Engine. |

---

## 2. VARIABLE & MEMORY MANAGEMENT
Commands to handle data and inspect the virtual RAM.

| Command | Alias | Description |
| :--- | :--- | :--- |
| **SET** | `S` | Assigns a value to a variable or object member. |
| **UNSET** | `U` | Removes a variable from memory. |
| **DUMP_MEM** | `DM`, `VARS` | Displays the "Virtual RAM Architecture" table. |
| **DEFINE** | `DEF` | Defines a blueprint/template for custom objects. |
| **TRACE** | - | Toggles execution tracing for debugging. |

---

## 3. FLOW CONTROL
Keywords used to manage program logic and loops.

| Command | Description |
| :--- | :--- |
| **IF / ELSE** | Conditional execution based on a boolean expression. |
| **SWITCH / CASE**| Branching logic for multiple specific values. |
| **WHILE** | Repeats code as long as a condition remains true. |
| **FOR** | Standard iterative loop. |
| **REPEAT** | Executes code until a specific condition is met. |
| **CYCLE** | Specialized loop for iterating over collections (Arrays/Maps). |

---

## 4. PROCEDURES & FUNCTIONS
Commands for modular code and reusability.

| Command | Alias | Description |
| :--- | :--- | :--- |
| **PROC** | - | Starts recording a Procedure (no return value). |
| **FUNC** | - | Starts recording a Function (supports `RETURN`). |
| **LIST_PROCS**| `LP`, `PROC_DUMP`| Lists all registered procedures in memory. |
| **LIST_FUNCS**| - | Lists all registered user functions. |
| **RETURN** | `RET` | Exits a function and returns a value. |
| **BREAK** | - | Immediately exits the current loop. |
| **CONTINUE** | - | Skips to the next iteration of the current loop. |

---

## 5. EXECUTION & OUTPUT
Commands for running scripts and communicating with the user.

| Command | Alias | Description |
| :--- | :--- | :--- |
| **ECHO** | `E` | Prints text or variable values to the console. |
| **ECHO_DBG** | `ED` | Prints debug information to the console. |
| **RUN** | `R` | Executes an external `.oli` script file. |
| **SYS** | - | Executes a system command in the OS shell. |
| **PLUGIN** | - | Manages or calls external engine plugins. |

---

## 6. USAGE TIPS
* **Case Sensitivity:** Command names are case-insensitive (`SET` is the same as `set`).
* **Variable Prefixes:** Always use the `$` prefix for variables (e.g., `$myVar`).
* **Multi-line Input:** Use a backslash `\` at the end of a line to continue on the next line.
* **Deep Copying:** Use the `CLONE()` function to copy objects by value instead of reference.

> "A robot must protect its own existence as long as such protection does not conflict with the First or Second Law." - **Oli Engine**