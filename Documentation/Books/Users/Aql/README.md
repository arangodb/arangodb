<a name="introduction"></a>
# Introduction

The ArangoDB query language (AQL) can be used to retrieve data that is stored in
ArangoDB. The general workflow when executing a query is as follows:

- A client application ships an AQL query to the ArangoDB server. The query text
  contains everything ArangoDB needs to compile the result set
- ArangoDB will parse the query, execute it and compile the results. If the
  query is invalid or cannot be executed, the server will return an error that
  the client can process and react to. If the query can be executed
  successfully, the server will return the query results to the client

AQL is mainly a declarative language, meaning that in a query it is expressed
what result should be achieved and not how. AQL aims to be human- readable and
therefore uses keywords from the English language. Another design goal of AQL
was client independency, meaning that the language and syntax are the same for
all clients, no matter what programming language the clients might use.  Further
design goals of AQL were to support complex query patterns, and to support the
different data models ArangoDB offers.

In its purpose, AQL is similar to the Structured Query Language (SQL), but the
two languages have major syntactic differences. Furthermore, to avoid any
confusion between the two languages, the keywords in AQL have been chosen to be
different from the keywords used in SQL.

AQL currently supports reading data only. That means you can use the language to
issue read-requests on your database, but modifying data via AQL is currently
not supported.

For some example queries, please refer to the page [AQL Examples](../AqlExamples/README.md).

