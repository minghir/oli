# COMMAND: PROC (Procedure)
Procedures are reusable blocks of code designed to perform specific actions. Unlike functions, procedures do not return a value directly to the caller.

## SYNTAX
proc procedureName param1 param2 ...
    <instructions>
endproc

## KEYWORDS
- **PROC**: Mandatory. Begins the recording of a new procedure.
- **ENDPROC**: Mandatory. Saves the procedure to memory and exits recording mode.
- **RETURN**: Optional. Exits the procedure execution immediately and returns control to the caller.

## USAGE EXAMPLE
# 1. Defining a procedure with parameters
proc announceStatus user state
    echo "[SYSTEM]: User " + $user + " is now " + $state
endproc

# 2. Calling the procedure
# You can pass literal values or global variables
set $myUser = "Daneel"
announceStatus $myUser "ONLINE"

## PARAMETER RULES
- **Definition**: When defining the procedure, parameters are written **without** the `$` sign (e.g., `proc test a b`).
- **Body**: Inside the procedure body, these parameters **MUST** be accessed using the `$` sign (e.g., `echo $a + $b`).
- **Scope**: Variables defined inside a procedure are local to that procedure and will not overwrite global variables unless explicitly targeted (e.g., using `@globalVar`).

## NOTES
- **Early Exit**: If a certain condition is met, use `return` to stop execution before reaching `endproc`.
- **Shadowing**: Procedures can access global variables, but local parameters will take priority if they share the same name.

---
"A procedure is more than a list of commands; it is the logical manifestation of a robot's purpose." - **Oli Engine**