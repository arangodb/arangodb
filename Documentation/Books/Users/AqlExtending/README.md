!CHAPTER Extending AQL with User Functions

AQL comes with a built-in set of functions, but it is no
full-feature programming language.

To add missing functionality or to simplify queries, users
may add own functions to AQL. These functions can be written
in Javascript, and must be registered via an API. 

In order to avoid conflicts with existing or future built-in 
function names, all user functions must be put into separate
namespaces. Invoking a user functions is then possible by referring
to the fully-qualified function name, which includes the namespace,
too. 
