# ADVANCED MEMORY CONCEPTS: INDIRECTION VS POINTERS

In Oli, there are two ways to refer to data indirectly. Understanding the difference is key to writing advanced scripts.

---

## 1. NAME INDIRECTION ($$$)
This uses the **String Value** of a variable as a name for another variable.



- **How it works**: The engine looks at the text inside the variable and treats it as a new variable name.
- **Usage**: Best for dynamic variable access where you don't want to deal with memory addresses.
- **Rabbit Hole Rule**: The number of `$` must match the depth of the names + 1 for the value.
  - `$v1 = "target"` -> `$$v1` is the value of `$target`.

---

## 2. MEMORY POINTERS (*)
This uses the **Physical Address** in the Virtual RAM.



- **How it works**: `REF()` captures the exact location in the Heap.
- **Performance**: Faster for large structures as it bypasses name lookups.
- **Auto-Dereference**: When using `.` or `[]`, Oli automatically "jumps" from the pointer to the object.
  - `set $p = REF("player")`
  - `$p.hp = 100` (Updates `player.hp` automatically).

---

## 3. GLOBAL SCOPE DIPLOMACY (@)
Oli handles the combination of global scope and indirection seamlessly:
- `@$$p` will resolve the names locally/globally but ensure the final write happens in the **Global Map**.

---
"Memory is the canvas upon which logic paints its masterpiece." - **R. Daneel Olivaw**