HTTP Interface for the information about the current database {#HttpCurrentDatabase}
====================================================================================

@NAVIGATE_HttpCurrentDatabase
@EMBEDTOC{HttpCurrentDatabaseTOC}

The Current Database {#HttpCurrentDatabaseIntro}
================================================

When a client connects to the ArangoDB server, the connection is always made to a
specific database.

ArangoDB provides an Http interface to retrieve information about the database
the client has connected to.

Retrieving the properties of the current database {#HttpCurrentDatabaseHttp}
============================================================================

@anchor HttpCurrentDatabaseGet
@copydetails JSF_get_api_current_database

