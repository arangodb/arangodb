Control flow functions
======================

AQL offers the following functions to let the user control the flow of operations:

- *NOT_NULL(alternative, ...)*: Returns the first alternative that is not *null*, 
  and *null* if all alternatives are *null* themselves

- *FIRST_LIST(alternative, ...)*: Returns the first alternative that is an array, and
  *null* if none of the alternatives is an array

- *FIRST_DOCUMENT(alternative, ...)*: Returns the first alternative that is a document,
  and *null* if none of the alternatives is a document

Miscellaneous functions
-----------------------

Finally, AQL supports the following functions that do not belong to any of the other
function categories:

- *COLLECTIONS()*: Returns an array of collections. Each collection is returned as a document
  with attributes *name* and *_id*

- *CURRENT_USER()*: Returns the name of the current user. The current user is the user 
  account name that was specified in the *Authorization* HTTP header of the request. It will
  only be populated if authentication on the server is turned on, and if the query was executed
  inside a request context. Otherwise, the return value of this function will be *null*.

- *DOCUMENT(collection, id)*: Returns the document which is uniquely identified by
  the *id*. ArangoDB will try to find the document using the *_id* value of the document
  in the specified collection. If there is a mismatch between the *collection* passed and
  the collection specified in *id*, then *null* will be returned. Additionally, if the
  *collection* matches the collection value specified in *id* but the document cannot be
  found, *null* will be returned. This function also allows *id* to be an array of ids.
  In this case, the function will return an array of all documents that could be found. 

*Examples*
  
      DOCUMENT(users, "users/john")
      DOCUMENT(users, "john")

      DOCUMENT(users, [ "users/john", "users/amy" ])
      DOCUMENT(users, [ "john", "amy" ])

  Note: The *DOCUMENT* function is polymorphic since ArangoDB 1.4. It can now be used with
  a single parameter *id* as follows:

- *DOCUMENT(id)*: In this case, *id* must either be a document handle string
  (consisting of collection name and document key) or an array of document handle strings, e.g.

      DOCUMENT("users/john")
      DOCUMENT([ "users/john", "users/amy" ])

- *CALL(function, arg1, ..., argN)*: Dynamically calls the function with name *function*
  with the arguments specified. Both built-in and user-defined functions can be called. 
  Arguments are passed as separate parameters to the called function.

      /* "this" */
      CALL('SUBSTRING', 'this is a test', 0, 4) 

- *APPLY(function, arguments)*: Dynamically calls the function with name *function* 
  with the arguments specified. Both built-in and user-defined functions can be called. 
  Arguments are passed as separate parameters to the called function.

      /* "this is" */
      APPLY('SUBSTRING', [ 'this is a test', 0, 7 ]) 

