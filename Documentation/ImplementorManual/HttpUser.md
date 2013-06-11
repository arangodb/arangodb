HTTP Interface for User Management {#HttpUser}
==============================================

@NAVIGATE_HttpUser
@EMBEDTOC{HttpUserTOC}

User Management {#HttpUserIntro}
================================

This is an introduction to ArangoDB's Http interface for managing users.

The interface provides a simple means to add, update, and remove users.
All users managed through this interface will be stored in the system 
collection `_users`.

This specialised interface intentionally does not provide all functionality 
that is available in the regular document REST API.

Operations on users may become more restricted than regular document operations, 
and extra privilege and security security checks may be introduced in the 
future for this interface.

@anchor HttpUserSave
@copydetails JSF_post_api_user

@CLEARPAGE
@anchor HttpUserReplace
@copydetails JSF_put_api_user

@CLEARPAGE
@anchor HttpUserUpdate
@copydetails JSF_patch_api_user

@CLEARPAGE
@anchor HttpUserRemove
@copydetails JSF_delete_api_user

@CLEARPAGE
@anchor HttpUserDocument
@copydetails JSF_get_api_user
