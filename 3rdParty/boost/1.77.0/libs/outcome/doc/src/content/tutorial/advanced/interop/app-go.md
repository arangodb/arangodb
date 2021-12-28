+++
title = "In use"
weight = 50
+++

This is how you might now write application code using these three libraries:

{{% snippet "finale.cpp" "app_go" %}}

The curiosity will be surely the `ext()` markup function, which needs
explaining. It was felt
important during Outcome's design that `value_or_error` conversions never
be implicit, as they almost always represent a transition across an
ABI or semantic boundary. They are also usually non-trivial to implement
and compile, and it was felt important that the programmer ought to
always mark the semantic boundary transition at the point of every use,
as considerable amounts of code may execute.

How the end user chooses to mark up their code is up to them, however
above we use a simple `ext()` function to mark up that the function
being called is *external* to the application. This ticks our box of
requiring the documentation, at the point of use, of every transition
in failure handling boundaries.

Note that we are able to use `TRY` as normal throughout this function.
Everything "just works".

And most especially note that we never, **at any stage**, needed to modify
the source code of `httplib`, `tidylib` nor `filelib`, nor inject
custom things into their namespaces. This entire worked example was
achieved solely by `app` based customisation points, and via `convert`.
