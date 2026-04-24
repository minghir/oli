# COMMAND: IF
Executes a block of code only if a specific condition evaluates to true.

## SYNTAX
if <expression> THEN
    <instructions>
else
    <instructions>
endif

## KEYWORDS
- **THEN**: Mandatory. Marks the end of the condition and the start of the "true" block.
- **ELSE**: Optional. Executes if the condition is false.
- **ENDIF**: Mandatory. Marks the end of the entire IF structure.

## USAGE EXAMPLE
set $hp = 15
if $hp < 20 THEN
    echo "Warning: Low health!"
    echo "Please find a medkit."
else
    echo "Status: Healthy"
endif

## LOGICAL OPERATORS
You can use standard operators in the expression:
`==`, `!=`, `<`, `>`, `<=`, `>=`, `&&` (AND), `||` (OR), `!` (NOT).

## NOTES
- **Case Sensitivity**: While Oli is case-insensitive (you can use `then` or `THEN`), using uppercase is recommended for readability.
- **Nesting**: You can nest multiple IF blocks inside each other.
- **Single Line**: You can write a short IF on a single line if needed, as long as all keywords are present.

---
"A robot must not injure a human being or, through inaction, allow a human being to come to harm." - **Oli Engine**