

@brief whether or not to enable the AQL query cache
`--query.cache-mode`

Toggles the AQL query cache behavior. Possible values are:

* *off*: do not use query cache
* *on*: always use query cache, except for queries that have their *cache*
  attribute set to *false*
* *demand*: use query cache only for queries that have their *cache*
  attribute set to *true*
  set

