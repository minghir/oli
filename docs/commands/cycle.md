# COMMAND: CYCLE
Iterates through each element of a collection (Array, Map, or String).

## SYNTAX
cycle <collection> as <iterator> DO
    <instructions>
endcycle

## KEYWORDS
- **AS**: Mandatory. Used to specify the variable name that will hold the current element.
- **DO**: Mandatory. Marks the start of the loop body.
- **ENDCYCLE**: Mandatory. Marks the end of the cycle block.

## BEHAVIOR BY TYPE
The command adapts its behavior based on what it is iterating over:

| Data Type | Iterator Value |
| :--- | :--- |
| **Array** | The current element value. |
| **Map** | The current **Key** (use indexing inside to get the value). |
| **String** | The current character. |

## USAGE EXAMPLES

### 1. Iterating an Array
set $fruits = ["Apple", "Banana", "Cherry"]
cycle $fruits as $f DO
    echo "I like " + $f
endcycle

### 2. Iterating a Map (Keys)
set $prices = {"Gas": 7.5, "Power": 0.8}
cycle $prices as $key DO
    echo $key + " costs " + $prices[$key]
endcycle

### 3. Iterating a String
set $msg = "Oli"
cycle $msg as $char DO
    echo "Letter: " + $char
endcycle

## SPECIAL FEATURES
### Shadowing Protection
Oli is a "clean" engine. If you use an iterator name that already exists (e.g., you use `$i` as an iterator but `$i` was already `100`), Oli will **restore** the original value once the cycle finishes.

### Nested Cycles
The engine supports nested cycles. Each `DO` is correctly matched with its corresponding `ENDCYCLE` at the same level.

---
"To iterate is human, to cycle is robotic." - **Oli Engine**