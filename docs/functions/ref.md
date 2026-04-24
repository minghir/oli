# FUNCTION: REF(variable_name)
Returns the memory address (pointer) of a specific variable.

## DESCRIPTION:
Creates a "pointer" to the memory slot where the variable's value is stored. 
If you modify the value via this pointer, the original variable is updated.

## USAGE:
set $a = 10
set $p = REF("a")
echo $p        # Displays [PTR: 0x...]

## DEREFERENCING:
To read or modify the value at the address stored in $p, 
use the '*' operator before the variable name.

echo *$p       # Displays 10
set *$p = 20   # $a is now 20!