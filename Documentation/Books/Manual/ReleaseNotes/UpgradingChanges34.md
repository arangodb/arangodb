Incompatible changes in ArangoDB 3.3
====================================

It is recommended to check the following list of incompatible changes **before**
upgrading to ArangoDB 3.4, and adjust any client programs if necessary.

The following incompatible changes have been made in ArangoDB 3.4:


Client tools
------------


REST API
--------

- GET /_api/aqlfunction/ now also returns the 'isDeterministic' property.
- DELETE /_api/aqlfunction/ now returns the number of deleted functions.


Miscellaneous
-------------
