Extending AQL with User Functions {#ExtendingAql}
=================================================

@NAVIGATE_ExtendingAql
@EMBEDTOC{ExtendingAqlTOC}

Conventions {#ExtendingAqlConventions}
======================================

AQL comes with a built-in set of functions, but it is no
full-feature programming language.

To add missing functionality or to simplifiy queries, users
may add own functions to AQL. These functions can be written
in Javascript, and must be registered via an API. 

In order to avoid conflicts with existing or future built-in 
function names, all user functions must be put into separate
namespaces. Invoking a user functions is then possible by referring
to the fully-qualified function name, which includes the namespace,
too. 

The `:` symbol is used inside AQL as the namespace separator. Using 
the namespace separator, users can create a multi-level hierarchy of 
function groups if required.

Examples:

    RETURN myfunctions:myfunc()

    RETURN myfunctions:math:random()

Note: as all function names in AQL, user function names are also
case-insensitive.

Built-in AQL functions reside in the namespace `_aql`, which is also
the default namespace to look in if an unqualified function name is
found. Adding user functions to the `_aql` namespace is disallowed and 
will fail.

User functions can take any number of input arguments and should 
provide one result. They should be kept purely functional and thus free of
side effects and state. 

User function code is late-bound, and may thus not rely on any variables
that existed at the time of declaration. If user function code requires
access to any external data, it must take care to set up the data by
itself.

User functions must only return primitive types (i.e. `null`, boolean
values, numeric values, string values) or aggregate types (lists or
documents) composed of these types.
Returning any other Javascript object type from a user function may lead 
to undefined behavior and should be avoided.


Registering and Unregistering User Functions {#ExtendingAqlHowTo}
=================================================================

AQL user functions can be registered using the `aqlfunctions` object as
follows:

    var aqlfunctions = require("org/arangodb/aql/functions");

To register a function, the fully qualified function name plus the
function code must be specified.

@copydetails JSF_aqlfunctions_register

It is possible to unregister a single user function, or a whole group of
user functions (identified by their namespace) in one go:

@copydetails JSF_aqlfunctions_unregister

@copydetails JSF_aqlfunctions_unregister_group

To get an overview of which functions are currently registered, the 
`toArray` function can be used:

@copydetails JSF_aqlfunctions_toArray

