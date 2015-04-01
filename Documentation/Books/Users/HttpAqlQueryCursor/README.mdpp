!CHAPTER HTTP Interface for AQL Query Cursors

!SUBSECTION Database Cursors 

This is an introduction to ArangoDB's HTTP Interface for Queries. Results of AQL
and simple queries are returned as cursors in order to batch the communication
between server and client. Each call returns a number of documents in a batch
and an indication if the current batch has been the final batch. Depending on
the query, the total number of documents in the result set might or might not be
known in advance. In order to free server resources the client should delete the
cursor as soon as it is no longer needed.

To execute a query, the query details need to be shipped from the client to
the server via an HTTP POST request.
