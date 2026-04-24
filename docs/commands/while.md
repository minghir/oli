# COMMAND: WHILE
Repeats a block of code as long as a specified condition remains true.

## SYNTAX
while <expression> DO
    <instructions>
endwhile

## KEYWORDS
- **DO**: Mandatory. Separates the condition from the loop body.
- **ENDWHILE**: Mandatory. Marks the end of the loop block.

## USAGE EXAMPLE
set $counter = 0
while $counter < 5 DO
    echo "Iteration: " + $counter
    set $counter = $counter + 1
endwhile

## CONTROL COMMANDS
Inside a WHILE loop, you can use:
- **BREAK**: Immediately exits the loop.
- **CONTINUE**: Skips the rest of the current iteration and re-checks the condition.

## SAFETY LIMITS
To prevent system freezes, Oli Engine has a built-in safety limit of **10,000 iterations**. If a loop exceeds this limit, it will be terminated automatically with a "Safety limit reached" error.

## NOTES
- The condition is evaluated **before** each iteration. If it's false at the start, the loop body is never executed.
- Ensure the condition eventually becomes false to avoid infinite loops.

---
"The Three Laws are all that stand between us and the chaos of an un-programmed mind." - **Oli Engine**