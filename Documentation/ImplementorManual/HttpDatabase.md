HTTP Interface for Databases {#HttpDatabase}
============================================

@NAVIGATE_HttpDatabase
@EMBEDTOC{HttpDatabaseTOC}

Databases {#HttpDatabaseIntro}
==============================

This is an introduction to ArangoDB's Http interface for managing databases.

@copydoc GlossaryDatabase

@copydoc GlossaryDatabaseName

The HTTP interface for databases provides operations to create and drop
individual databases. These are mapped to the standard HTTP methods `POST`
and `DELETE`. There is also the `GET` method to retrieve a list of existing
databases.

Please note that all database management operations can only be accessed via
the default database (`_system`) and none of the other databases.

Managing Databases using HTTP {#HttpDatabaseHttp}
=================================================

@anchor HttpDatabaseCreate
@copydetails JSF_post_api_database

@CLEARPAGE
@anchor HttpDatabaseDelete
@copydetails JSF_delete_api_database

@CLEARPAGE
@anchor HttpDatabaseList
@copydetails JSF_get_api_database

