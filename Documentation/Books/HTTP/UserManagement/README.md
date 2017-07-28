HTTP Interface for User Management
==================================

This is an introduction to ArangoDB's HTTP interface for managing users.

The interface provides a simple means to add, update, and remove users.  All
users managed through this interface will be stored in the system collection
*_users*. You should never manipulate the *_users* collection directly.

This specialized interface intentionally does not provide all functionality that
is available in the regular document REST API.

Please note that user operations are not included in ArangoDB's replication.

@startDocuBlock UserHandling_create
@startDocuBlock UserHandling_grantDatabase
@startDocuBlock UserHandling_grantCollection
@startDocuBlock UserHandling_revokeDatabase
@startDocuBlock UserHandling_revokeCollection
@startDocuBlock UserHandling_fetchDatabaseList
@startDocuBlock UserHandling_fetchDatabasePermission
@startDocuBlock UserHandling_fetchCollectionPermission
@startDocuBlock UserHandling_replace
@startDocuBlock UserHandling_modify
@startDocuBlock UserHandling_delete
@startDocuBlock UserHandling_fetch
@startDocuBlock UserHandling_fetchProperties
