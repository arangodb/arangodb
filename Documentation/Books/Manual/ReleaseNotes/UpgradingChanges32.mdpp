Incompatible changes in ArangoDB 3.2
====================================

It is recommended to check the following list of incompatible changes **before**
upgrading to ArangoDB 3.2, and adjust any client programs if necessary.

AQL
---

* AQL breaking change in cluster:
  The SHORTEST_PATH statement using edge-collection names instead
  of a graph name now requires to explicitly name the vertex-collection names 
  within the AQL query in the cluster. It can be done by adding `WITH <name>`
  at the beginning of the query.
  
  Example:
  ```
  FOR v,e IN OUTBOUND SHORTEST_PATH @start TO @target edges [...]
  ```

  Now has to be:

  ```
  WITH vertices
  FOR v,e IN OUTBOUND SHORTEST_PATH @start TO @target edges [...]
  ```

  This change is due to avoid dead-lock sitations in clustered case.
  An error stating the above is included.


REST API
--------

* Removed undocumented internal HTTP API:
  * PUT _api/edges

  The documented GET _api/edges and the undocumented POST _api/edges remains unmodified.

* change undocumented behaviour in case of invalid revision ids in
  `If-Match` and `If-None-Match` headers from returning HTTP status code 400 (bad request) 
  to returning HTTP status code 412 (precondition failed).


JavaScript API
--------------

* change undocumented behaviour in case of invalid revision ids in
  JavaScript document operations from returning error code 1239 ("illegal document revision")
  to returning error code 1200 ("conflict").


Foxx
----

* JWT tokens issued by the built-in [JWT session storage](../Foxx/Sessions/Storages/JWT.md) now correctly specify the `iat` and `exp` values in seconds rather than milliseconds as specified in the JSON Web Token standard.

  This may result in previously expired tokens using milliseconds being incorrectly accepted. For this reason it is recommended to replace the signing `secret` or set the new `maxExp` option to a reasonable value that is smaller than the oldest issued expiration timestamp.

  For example setting `maxExp` to `10**12` would invalidate all incorrectly issued tokens before 9 September 2001 without impairing new tokens until the year 33658 (at which point these tokens are hopefully no longer relevant).


Command-line options changed
----------------------------

* --server.maximal-queue-size is now an absolute maximum. If the queue is
  full, then 503 is returned. Setting it to 0 means "no limit". The default
  value for this option is now `0`.

* the default value for `--ssl.protocol` has been changed from `4` (TLSv1) to `5` (TLSv1.2).

* the startup options `--database.revision-cache-chunk-size` and
  `--database.revision-cache-target-size` are now obsolete and do nothing

* the startup option `--database.index-threads` option is now obsolete

* the option `--javascript.v8-contexts` is now an absolute maximum. The server
  may start less V8 contexts for JavaScript execution at startup. If at some
  point the server needs more V8 contexts it may start them dynamically, until
  the number of V8 contexts reaches the value of `--javascript.v8-contexts`.

  the minimum number of V8 contexts to create at startup can be configured via
  the new startup option `--javascript.v8-contexts-minimum`.
