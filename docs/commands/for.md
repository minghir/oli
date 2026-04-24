# COMMAND: FOR
Executes a block of code a specific number of times using a counter variable.

## SYNTAX
FOR $<var> = <start> TO <end> [BY <step>] DO
    <instructions>
ENDFOR

## KEYWORDS
- **TO**: Mandatory. Defines the target limit for the counter.
- **BY**: Optional. Defines the increment value (default is 1).
- **DO**: Mandatory. Marks the start of the loop body.
- **ENDFOR**: Mandatory. Marks the end of the FOR block.

## BEHAVIOR
1.  **Initialization**: The variable is set to the `<start>` value.
2.  **Condition**: 
    - If `step` is positive, the loop runs as long as `variable <= end`.
    - If `step` is negative, the loop runs as long as `variable >= end`.
3.  **Iteration**: After each cycle, the `step` is added to the variable.

## USAGE EXAMPLES

### 1. Simple Incremental Loop
FOR $i = 1 TO 5 DO
    echo "Number: " + $i
ENDFOR

### 2. Using a Custom Step (BY)
FOR $i = 0 TO 10 BY 2 DO
    echo "Even: " + $i
ENDFOR

### 3. Counting Down (Negative Step)
FOR $i = 10 TO 1 BY -1 DO
    echo "Countdown: " + $i
ENDFOR

## CONTROL COMMANDS
- **BREAK**: Exit the loop immediately.
- **CONTINUE**: Skip the current iteration and move to the next increment step.

## NOTES
- **Safety**: Like the WHILE loop, the FOR command has a safety limit of **10,000 iterations**.
- **Expressions**: You can use variables or complex math for start, end, and step values (e.g., `FOR $i = $min TO $max / 2`).

---
"A robot must be able to perform repetitive tasks without succumbing to the fatigue of the mind." - **Oli Engine**