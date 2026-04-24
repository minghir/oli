# FUNCTION: CLONE(object)
Creates a deep copy of a Map or an Array.

## DESCRIPTION:
Unlike a simple assignment (which only copies the reference), 
CLONE creates a completely new object in memory with the same values.

## EXAMPLE:
set $original = {"hp": 10}
set $copy = CLONE($original)

set $original.hp = 99
echo $copy.hp    # Still displays 10 (Independent copy)