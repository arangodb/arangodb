

@brief returns a list of all endpoints
`db._endpoints()`

Returns a list of all endpoints and their mapped databases.

Please note that managing endpoints can only be performed from out of the
*_system* database. When not in the default database, you must first switch
to it using the "db._useDatabase" method.

