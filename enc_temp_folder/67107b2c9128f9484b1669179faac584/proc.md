# COMMAND: PROC (Procedure)
Defines a reusable block of code. Procedures perform actions but do not return a value to the caller.

## SYNTAX:
proc procedureName param1 param2 ...
    <instructions>
endproc

## USAGE EXAMPLE:
# Define it:
proc greet userPrefix
    echo $userPrefix + " Welcome to Oli!"
endproc

# Call it:
greet "Captain:"

## NOTES:
- Parameters are defined without the '$' sign in the 'proc' line.
- Inside the procedure, parameters MUST be accessed with the '$' sign.
- To exit a procedure early, you can use the 'return' command (without a value).