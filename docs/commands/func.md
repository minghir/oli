\# COMMAND: FUNC (Function)

Functions are reusable blocks of code designed to perform calculations and return a specific value to the caller.



\## SYNTAX

func functionName param1 param2 ...

&#x20;   <instructions>

&#x20;   return <expression>

endfunc



\## KEYWORDS

\- \*\*FUNC\*\*: Mandatory. Starts the recording of a new function.

\- \*\*ENDFUNC\*\*: Mandatory. Saves the function and ends recording mode.

\- \*\*RETURN\*\*: Mandatory (to provide output). Stops execution and passes the result back.



\## KEY FEATURES



\### 1. Case Insensitivity

Function names are automatically converted to \*\*UPPERCASE\*\*. Defining `func calculate` or `func CALCULATE` results in the same internal reference.



\### 2. Context Injection ($this)

When a function is called in the context of a Map or Object, Oli automatically injects a special local variable called `$this`. 

\- \*\*Example\*\*: If you call `$myObj.update()`, inside the function, `$this` refers to `$myObj`.



\### 3. Stack Safety \& Recursion

Each function call creates a new \*\*Stack Frame\*\*. This means:

\- Variables are local by default.

\- Functions can call themselves (Recursion) without corrupting the state of previous calls.

\- The `return` flag is restored correctly after nested calls.



\## USAGE EXAMPLES



\### Basic Math Function

```oli

func square x

&#x20;   return $x \* $x

endfunc



set $result = square(5) # Result: 25

```



\### Recursive Factorial

```oli

func fact n

&#x20;   if $n <= 1 then return 1 endif

&#x20;   return $n \* fact($n - 1)

endfunc



echo fact(5) # Output: 120

```



\### Contextual Usage ($this)

```oli

set $player = {"name": "Galahad", "hp": 100}



func heal amount

&#x20;   set $this.hp = $this.hp + $amount

&#x20;   return $this.hp

endfunc



\# Hypothetical context call

$player.heal(20) 

echo $player.hp # Output: 120

```



\## PARAMETER CLEANING

Oli is forgiving with syntax. In the definition line, parameters are automatically cleaned of extra characters like commas or brackets:

\- `func test \[a, b]` is internally cleaned to `func test a b`.



\## NOTES

\- \*\*Return Value\*\*: If no `return` is executed, the function returns `NULL`.

\- \*\*Global Access\*\*: Use the `@` prefix inside a function to modify global variables (e.g., `set @score = 100`).



\---

"A function is not just a calculation; it is a promise made by the logic to return a piece of the truth." - \*\*R. Daneel Olivaw\*\*

```

