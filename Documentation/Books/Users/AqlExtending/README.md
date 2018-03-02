Extending AQL with User Functions
=================================

AQL comes with a built-in set of functions, but it is not a
fully-featured programming language.

To add missing functionality or to simplify queries, users
may add their own functions to AQL in the selected database. 
These functions can be written in JavaScript, and must be 
registered via the API; see [Registering Functions](../AqlExtending/Functions.md).

In order to avoid conflicts with existing or future built-in 
function names, all user functions must be put into separate
namespaces. Invoking a user functions is then possible by referring
to the fully-qualified function name, which includes the namespace,
too; see [Conventions](../AqlExtending/Conventions.md). 
