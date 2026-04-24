\# COMMAND: DEF (Definition)

The `DEF` command is used to create blueprints (templates) for structured data types. It allows you to define the shape of an object before instantiating it.



\## SYNTAX

`DEF STRUCT <TypeName> { <field1>, <field2>, ... }`  

`DEF CLASS <TypeName> { <field1>, <field2>, ... }`



\## KEYWORDS

\- \*\*DEF\*\*: Mandatory. Initiates the definition of a new blueprint.

\- \*\*STRUCT\*\*: Defines a data-heavy structure (isClass = false).

\- \*\*CLASS\*\*: Defines an object-oriented structure (isClass = true).

\- \*\*{ }\*\*: Braces are used to enclose the comma-separated list of fields or members.



\## BEHAVIOR

1\. \*\*Blueprint Registration\*\*: The engine saves the name and the list of fields in the `m\_blueprints` registry.

2\. \*\*Validation\*\*: The command checks for the presence of the type name and the mandatory braces.

3\. \*\*Normalization\*\*: The subtype (`struct` or `class`) is case-insensitive, while the blueprint name preserves its case for strict referencing.



\## USAGE EXAMPLES



\### 1. Defining a Simple Coordinate Structure

```oli

def struct Point { x, y, z }

\# Recorded a 'Point' blueprint with 3 fields.

```



\### 2. Defining a Complex Class

```oli

def class Player { name, health, inventory, position }

\# Recorded a 'Player' class blueprint.

```



\## RULES \& CONSTRAINTS

\- \*\*Field Separation\*\*: Fields inside the braces \*\*must\*\* be separated by commas (`,`).

\- \*\*Syntax Integrity\*\*: Both the opening `{` and closing `}` are mandatory.

\- \*\*No Direct Data\*\*: `DEF` only defines the \*structure\*. To fill it with data, you must create an instance of that blueprint.



\## NOTES

\- \*\*Quote Safety\*\*: Using `wexplodeQuoteSafe` allows the engine to handle field names properly even if they contain complex characters.

\- \*\*Memory Efficiency\*\*: Blueprints are stored as templates and do not consume RAM like active variables until they are instantiated.



\---

> "To define a structure is to impose order upon the chaos of raw data; it is the first step toward true artificial intelligence." - \*\*R. Daneel Olivaw\*\*

```





