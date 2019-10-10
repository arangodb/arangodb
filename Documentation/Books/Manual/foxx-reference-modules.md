---
layout: default
description: These are some of the modules outside of Foxx you will find useful whenwriting Foxx services
---
Related modules
===============

These are some of the modules outside of Foxx you will find useful when
writing Foxx services.

Additionally there are modules providing some level of compatibility with
Node.js as well as a number of bundled NPM modules (like lodash and joi).
For more information on these modules see
[the JavaScript modules appendix](appendix-java-script-modules.html).

The `@arangodb` module
----------------------

`require('@arangodb')`

This module provides access to various ArangoDB internals as well as three of
the most important exports necessary to work with the database in Foxx:
`db`, `aql` and `errors`.

You can find a full description of this module in the
[ArangoDB module appendix](appendix-java-script-modules-arango-db.html).

The `@arangodb/locals` module
-----------------------------

`require('@arangodb/locals')`

This module provides a `context` object which is identical to the
[service context](foxx-reference-context.html) of whichever module requires it.

There is no advantage to using this module over the `module.context` variable
directly unless you're [using a tool like Webpack](foxx-guides-webpack.html)
to translate your code and can't use the `module` object Foxx provides directly.

The `@arangodb/request` module
------------------------------

`require('@arangodb/request')`

This module provides a function for making HTTP requests to external services.
Note that while this allows communicating with third-party services it may
affect database performance by blocking Foxx requests as ArangoDB waits for
the remote service to respond. If you routinely make requests to slow external
services and are not directly interested in the response it is probably a
better idea to delegate the actual request/response cycle to a gateway service
running outside ArangoDB.

You can find a full description of this module in the
[request module appendix](appendix-java-script-modules-request.html).

The `@arangodb/general-graph` module
------------------------------------

`require('@arangodb/general-graph')`

This module provides access to ArangoDB graph definitions and various low-level
graph operations in JavaScript. For more complex queries it is probably better
to use AQL but this module can be useful in your setup and teardown scripts to
create and destroy graph definitions.

For more information see the chapter on the
[general graph module](graphs-general-graphs.html).
