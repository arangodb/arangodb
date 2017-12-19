!CHAPTER Functions

AQL supports functions to allow more complex computations. Functions can be
called at any query position where an expression is allowed. The general
function call syntax is:

```js
FUNCTIONNAME(arguments)
```

where *FUNCTIONNAME* is the name of the function to be called, and *arguments*
is a comma-separated list of function arguments. If a function does not need any
arguments, the argument list can be left empty. However, even if the argument
list is empty the parentheses around it are still mandatory to make function
calls distinguishable from variable names.

Some example function calls:

```js
HAS(user, "name")
LENGTH(friends)
COLLECTIONS()
```

In contrast to collection and variable names, function names are case-insensitive, 
i.e. *LENGTH(foo)* and *length(foo)* are equivalent.

!SUBSECTION Extending AQL
 
It is possible to extend AQL with user-defined functions. These functions need to
be written in JavaScript, and have to be registered before they can be used in a query.
Please refer to [Extending AQL](../Extending/index.html) for more details.
