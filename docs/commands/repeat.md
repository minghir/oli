# COMMAND: REPEAT
A post-test loop that executes a block of code at least once and continues until a condition becomes true.

## SYNTAX
REPEAT
    <instructions>
UNTIL <condition>
ENDREPEAT

## KEYWORDS
- **REPEAT**: Marks the beginning of the loop.
- **UNTIL**: Mandatory. Marks the end of the body and evaluates the exit condition.
- **ENDREPEAT**: Mandatory. Marks the structural end of the command block.

## BEHAVIOR
1.  **Execution First**: The code block is executed immediately, regardless of the condition.
2.  **Condition Check**: After execution, the `<condition>` is evaluated.
3.  **Exit Logic**: Unlike `WHILE`, this loop **STOPS** when the condition is **TRUE** and continues if it is **FALSE**.

## USAGE EXAMPLES

### 1. Guaranteed Execution
set $i = 10
REPEAT
    echo "This will print once even if $i is 10: " + $i
    set $i = $i + 1
UNTIL $i >= 10
ENDREPEAT

### 2. Waiting for a State
set $energy = 0
REPEAT
    echo "Charging..."
    set $energy = $energy + 25
UNTIL $energy == 100
ENDREPEAT
echo "System Ready!"

## REPEAT vs WHILE
| Feature | WHILE | REPEAT |
| :--- | :--- | :--- |
| **Check Time** | Before body (Pre-test) | After body (Post-test) |
| **Min. Executions** | 0 | 1 |
| **Stop Condition** | Stops when False | Stops when True |

## NOTES
- **Safety Limit**: The REPEAT command has a safety trigger of **1,000 iterations**. 
- **Control**: Supports **BREAK** (exits immediately) and **CONTINUE** (jumps to the UNTIL check).

---
"A robot's duty is never finished until the objective is reached, but even a robot knows when to check the exit parameters." - **Oli Engine**