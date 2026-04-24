# COMMAND: UNSET
Removes a variable, a map key, or an array element from memory.

## SYNTAX:
- `unset $variable` : Removes a local or global variable.
- `unset @variable` : Forces the removal of a global variable.
- `unset $map.key`  : Removes a specific key from a Map.
- `unset $array[index]` : Removes an element from an Array.
- `unset all` : Clears all global variables.

## EXAMPLES:
### 1. Simple Variables
set $temp = "Data"
unset $temp
# $temp is now gone.

### 2. Global Forcing
# Removes 'score' from the global scope even if a local 'score' exists.
unset @score

### 3. Maps and Arrays
set $hero = {"name": "Daneel", "role": "Robot"}
unset $hero.role
# $hero is now {"name": "Daneel"}

set $list = [10, 20, 30]
unset $list[1]
# $list is now [10, 30]. Note: Elements shift to fill the gap.

## NOTES:
- **Array Re-indexing**: When you unset an array element, all subsequent elements shift down. 
- **Error Handling**: If you try to unset a non-existent path, Oli will report a "Variable not found" or "Key not found" error.
- **Cleaning Up**: Use `unset all` during development to quickly reset your environment.

---
"A robot must not allow a human to suffer harm through inaction... including the harm of a cluttered memory." - **Oli Engine**