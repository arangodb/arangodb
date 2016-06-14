!CHAPTER Foxx

Traditionally, server-side projects have been developed as standalone applications
that guide the communication between the client-side frontend and the database
backend. This has led to applications that were either developed as single
monoliths or that duplicated data access and domain logic across all services
that had to access the database. Additionally, tools to abstract away the
underlying database calls could incur a lot of network overhead when using remote
databases without careful optimization.

ArangoDB allows application developers to write their data access and domain logic
as microservices running directly within the database with native access to
in-memory data. The **Foxx microservice framework** makes it easy to extend
ArangoDB's own REST API with custom HTTP endpoints using modern JavaScript running
on the same V8 engine you know from Node.js and the Google Chrome web browser.

Unlike traditional approaches to storing logic in the database (like stored
procedures), these microservices can be written as regular structured JavaScript
applications that can be easily distributed and version controlled. Depending on
your project's needs Foxx can be used to build anything from optimized REST
endpoints performing complex data access to entire standalone applications
running directly inside the database.
