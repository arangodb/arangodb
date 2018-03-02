HTTP Interface for User Management
==================================

This is an introduction to ArangoDB's HTTP interface for managing users.

The interface provides a simple means to add, update, and remove users.  All
users managed through this interface will be stored in the system collection
*_users*.

This specialized interface intentionally does not provide all functionality that
is available in the regular document REST API.

You should not manipulate the *_users* collection directly. While the user management interface aims to maintain backwards compatibility, the underlying collection is now managed by the bundled *users* Foxx app.

Please note that user operations are not included in ArangoDB's replication.


<!-- js/actions/api-user.js -->

@startDocuBlock JSF_api_user_create


<!-- js/actions/api-user.js -->

@startDocuBlock JSF_api_user_replace


<!-- js/actions/api-user.js -->

@startDocuBlock JSF_api_user_update


<!-- js/actions/api-user.js -->

@startDocuBlock JSF_api_user_delete


<!-- js/actions/api-user.js -->

@startDocuBlock JSF_api_user_fetch

<!-- js/actions/api-user.js -->

@startDocuBlock JSF_api_user_fetch_list