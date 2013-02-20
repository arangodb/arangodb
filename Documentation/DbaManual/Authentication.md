Authentication and Authorisation{#DbaManualAuthentication}
==========================================================

@NAVIGATE_DbaManualAuthentication
@EMBEDTOC{DbaManualAuthenticationTOC}

Authentication and Authorisation{#DbaManualAuthenticationIntro}
===============================================================

ArangoDB only provides a very simple authentication interface and no
authorisation. We plan to add authorisation features in later releases, which
will allow the administrator to restrict access to collections and queries to
certain users, given them either read or write access.

Currently, you can only secure the access to ArangoDB in an all-or-nothing
fashion. The collection `_users` contains all users and a salted SHA256 hash
of their passwords. A user can be active or inactive. A typical document of this
collection is

@EXAMPLE_ARANGOSH_OUTPUT{AuthenticationExample1}
    db._users.firstExample("user", "root")
@END_EXAMPLE_ARANGOSH_OUTPUT

Command-Line Options for the Authentication and Authorisation{#DbaManualAuthenticationCommandLine}
--------------------------------------------------------------------------------------------------

@copydetails triagens::rest::ApplicationEndpointServer::_disableAuthentication

Introduction to User Management{#UserManagementIntro}
=====================================================

ArangoDB provides basic functionality to add, modify and remove
database users programmatically. The following functionality is
provided by the `users` module and can be used from inside arangosh
and arangod.

Please note that this functionality is not available from within the
web interface.

@anchor UserManagementSave
@copydetails JSF_saveUser

@CLEARPAGE
@anchor UserManagementDocument
@copydetails JSF_documentUser

@CLEARPAGE
@anchor UserManagementReplace
@copydetails JSF_replaceUser

@CLEARPAGE
@anchor UserManagementUpdate
@copydetails JSF_updateUser

@CLEARPAGE
@anchor UserManagementRemove
@copydetails JSF_removeUser

@CLEARPAGE
@anchor UserManagementReload
@copydetails JSF_reloadUsers
