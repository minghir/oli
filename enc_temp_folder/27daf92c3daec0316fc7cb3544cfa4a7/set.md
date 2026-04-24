# COMMAND: SET
Assigns a value to a variable or an object member.

## SYNTAX:
set $variable = <expression>
set $object.member = <expression>

## EXAMPLES:
set $score = 100
set $player = {"name": "Galahad", "hp": 100}
set $player.hp = 95

## NOTES:
- If the variable does not exist, it will be created in the global scope.
- You can use 'set' to modify values via pointers: set *$ptr = 50.