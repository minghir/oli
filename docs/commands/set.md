# COMMAND: SET
The primary command for memory allocation and value assignment. It bridges the gap between raw expressions and the Oli Virtual RAM.

## SYNTAX
`SET $<variable> = <expression>`  
`SET @<global_variable> = <expression>`  
`SET $<object>.<member> = <expression>`  
`$<variable> = <expression>` (Implicit Shortcut)

## KEY FEATURES

### 1. Scoping Control
- **Local (Default)**: `set $a = 5`. Inside a function or procedure, this creates a protected variable that exists only within that local stack frame.
- **Global Override (@)**: `set @a = "global"`. Forces the engine to bypass the local stack and write directly to the Global Variable Map.

### 2. Variable Indirection (The `$` Chain)
Oli supports multi-level dynamic referencing, allowing you to treat strings as variable names recursively:
- **Example**:
  ```oli
  $a = 1
  $b = "a"
  $c = "b"
  echo $$$c  # Output: 1 
  # Logic: $c -> "b", then $b -> "a", then $a -> 1

### 3. Pointer Manipulation
Modify values directly at a specific memory address if the variable holds a valid memory reference (pointer):
`SET *$ptr = 100`

### 4. Dynamic Function & Procedure Calls
Variables can act as "function pointers". If a variable stores the name of a function, it can be invoked using the variable itself:
- **Example**:
set $f = "FACT"
echo $f(5)  # Executes FACT(5) -> 120

Supports advanced resolution like $var()("MESSAGE").

USAGE EXAMPLES
Basic and Compound Assignment

set $x = 10
$x += 5        # Compound operators (+=, -=, *=, /=)
set $name = "Oli" + " Engine"

Global vs Local Scope Test

set $a = "original"
func testScope
    set @a = "changed_globally" # Overwrites global 'a'
    set $a = "stay_local"       # Local 'a' shadowing
endfunc

Recursion & Complex Expressions

func fact n
    if $n <= 1 then return 1 endif
    return $n * fact($n - 1)
endfunc

set $total = fact(5) + fact(3) # Recursive evaluation in SET

NOTES
Implicit SET: In the interactive shell, the SET keyword can be omitted for direct assignments (e.g., $i = 0).

Type Fluidity: Oli is dynamically typed. A variable can change from INT to STRING, ARRAY, or MAP on the fly.

Reference Persistence: Global variables remain in the Heap until unset all is called or the session terminates.

"To assign a value is to define a reality. To refer to it is to remember that reality exists." - **Oli Engine**
