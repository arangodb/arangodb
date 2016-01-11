

@brief executes a transaction
`db._executeTransaction(object)`

Executes a server-side transaction, as specified by *object*.

*object* must have the following attributes:
- *collections*: a sub-object that defines which collections will be
  used in the transaction. *collections* can have these attributes:
  - *read*: a single collection or a list of collections that will be
    used in the transaction in read-only mode
  - *write*: a single collection or a list of collections that will be
    used in the transaction in write or read mode.
- *action*: a Javascript function or a string with Javascript code
  containing all the instructions to be executed inside the transaction.
  If the code runs through successfully, the transaction will be committed
  at the end. If the code throws an exception, the transaction will be
  rolled back and all database operations will be rolled back.

Additionally, *object* can have the following optional attributes:
- *waitForSync*: boolean flag indicating whether the transaction
  is forced to be synchronous.
- *lockTimeout*: a numeric value that can be used to set a timeout for
  waiting on collection locks. If not specified, a default value will be
  used. Setting *lockTimeout* to *0* will make ArangoDB not time
  out waiting for a lock.
- *params*: optional arguments passed to the function specified in
  *action*.


