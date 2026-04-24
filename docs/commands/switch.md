# COMMAND: SWITCH
Provides a clean way to execute different blocks of code based on the value of a single expression.

## SYNTAX
SWITCH <expression>
    CASE <value1>
        <instructions>
        BREAK
    CASE <value2>
        <instructions>
        BREAK
    DEFAULT
        <instructions>
ENDSWITCH

## KEYWORDS
- **SWITCH**: Mandatory. Evaluates the control expression once.
- **CASE**: Defines a specific value to compare against the control expression.
- **DEFAULT**: Optional. Executes if no `CASE` matches the value.
- **BREAK**: Mandatory (if you want to exit). Without it, Oli will continue to execute the next CASE (fall-through).
- **ENDSWITCH**: Mandatory. Marks the end of the block.

## BEHAVIOR
1. **Evaluation**: The expression after `SWITCH` is evaluated to a string or number.
2. **Matching**: Oli compares this value against each `CASE` using `compareVData`.
3. **Execution**: When a match is found, instructions are executed until a `BREAK` or `ENDSWITCH` is encountered.
4. **Fall-through**: If a `BREAK` is missing at the end of a `CASE`, the engine will execute the next `CASE` regardless of whether its value matches.



## USAGE EXAMPLES

### 1. Basic Routing
set $status = "ERROR"
SWITCH $status
    CASE "OK"
        echo "System is healthy."
        BREAK
    CASE "WARNING"
        echo "System check required."
        BREAK
    CASE "ERROR"
        echo "System failure detected!"
        BREAK
    DEFAULT
        echo "Unknown status."
ENDSWITCH

### 2. Using Fall-through (Advanced)
# Multiple cases executing the same code
set $grade = "B"
SWITCH $grade
    CASE "A"
    CASE "B"
    CASE "C"
        echo "You passed!"
        BREAK
    CASE "F"
        echo "You failed."
        BREAK
ENDSWITCH

## NOTES
- **Nesting**: You can nest `SWITCH` blocks inside `IF`, `WHILE`, or even other `SWITCH` blocks. The `findTopLevelSwitchKeyword` logic ensures keywords aren't confused between levels.
- **Comparison**: Comparisons are type-aware (a number `10` matches a numeric `10`, but might not match a string `"10"` depending on your `compareVData` rules).

---
"Choice is the only thing that separates a programmed path from a thinking mind." - **R. Daneel Olivaw**