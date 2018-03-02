Functions
=========

AQL supports functions to allow more complex computations. Functions can be
called at any query position where an expression is allowed. The general
function call syntax is:

    FUNCTIONNAME(arguments)

where *FUNCTIONNAME* is the name of the function to be called, and *arguments*
is a comma-separated list of function arguments. If a function does not need any
arguments, the argument list can be left empty. However, even if the argument
list is empty the parentheses around it are still mandatory to make function
calls distinguishable from variable names.

Some example function calls:

    HAS(user, "name")
    LENGTH(friends)
    COLLECTIONS()

In contrast to collection and variable names, function names are case-insensitive, 
i.e. *LENGTH(foo)* and *length(foo)* are equivalent.

#### Extending AQL
 
Since ArangoDB 1.3, it is possible to extend AQL with user-defined functions. 
These functions need to be written in JavaScript, and be registered before usage
in a query.

Please refer to [Extending AQL](../AqlExtending/README.md) for more details on this.

By default, any function used in an AQL query will be sought in the built-in 
function namespace *_aql*. This is the default namespace that contains all AQL
functions that are shipped with ArangoDB. 
To refer to a user-defined AQL function, the function name must be fully qualified
to also include the user-defined namespace. The *::* symbol is used as the namespace
separator:

    MYGROUP::MYFUNC()

    MYFUNCTIONS::MATH::RANDOM()
    
As all AQL function names, user function names are also case-insensitive.
