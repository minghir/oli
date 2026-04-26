### 2. Actualizare `ref.md` (Adăugăm Auto-Dereferențierea)

```markdown
# FUNCTION: REF(var_name)
Returns the raw memory address (pointer) of a specific variable.

## USAGE
`set $ptr = REF("variable_name")`

## DEREFERENCING
1. **Explicit (`*`)**: Used for raw values.
   - `set *$ptr = 100` (Writes 100 to the address).
2. **Implicit (Auto-Deref)**: Used for members and indexes.
   - If `$ptr` points to a Map/Struct, you can use `$ptr.field` directly.
   - If `$ptr` points to an Array, you can use `$ptr[index]` directly.

## RULES
- `REF` ignores the `$` or `@` prefix in the string argument.
- It searches **Local Scope** first, then **Global Scope**.

---
"A pointer is a map to a hidden treasure; use it wisely." - **Oli Engine**