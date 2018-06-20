Incompatible changes in ArangoDB 3.4
====================================

It is recommended to check the following list of incompatible changes **before**
upgrading to ArangoDB 3.4, and adjust any client programs if necessary.

The following incompatible changes have been made in ArangoDB 3.4:

Geo-Index
--------------

- The RocksDB on-disk storage format for indexes with type `geo` has changed
  (also affects `geo1` and `geo2` indexes).
  This **requires** users to start the arangod process with the
  `--database.auto-upgrade true` option to allow arangodb
  to recreate these indexes with the new on-disk format.

- Geo-Indexes will now reported themselves no longer as _geo1_ or _geo2_ but
  with type `geo` . The two previously known geo-index types (`geo1`and `geo2`)
  are **deprecated**. APIs to create indexes (`ArangoCollection.ensureIndex`)
  will continue to support `geo1`and `geo2`.

Client tools
------------

HTTP API
--------

- `GET /_api/aqlfunction` was migrated to match the general structure of
  ArangoDB Replies. It now returns an object with a "result" attribute that
  contains the list of available AQL user functions:

  ```json
  {
    "code": 200,
    "error": false,
    "result": [
      {
        "name": "UnitTests::mytest1",
        "code": "function () { return 1; }",
        "isDeterministic": false
      }
    ]
  }
  ```

  In previous versions, this REST API returned only the list of available
  AQL user functions on the top level of the response.
  Each AQL user function description now also contains the 'isDeterministic' attribute.

- `POST /_api/aqlfunction` now includes an "isNewlyCreated" attribute that indicates
  if a new function was created or if an existing one was replaced (in addition to the
  "code" attribute, which remains 200 for replacement and 201 for creation):

  ```json
  {
    "code": "201",
    "error": false,
    "isNewlyCreated": true
  }
  ```

- `DELETE /_api/aqlfunction` now returns the number of deleted functions:

  ```json
  {
    "code": 200,
    "error": false,
    "deletedCount": 10
  }
  ```

Miscellaneous
-------------

AQL Functions
-------------
- CALL / APPLY
  - may emmit `ERROR_QUERY_FUNCTION_NAME_UNKNOWN` or `ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH`
    instead of `ERROR_QUERY_FUNCTION_NOT_FOUND` in some situations.
  - are now able to be invoked recursive
