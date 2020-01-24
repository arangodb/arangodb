
@startDocuBlock get_admin_database_version
@brief returns the version of the database.

@RESTHEADER{GET /_admin/database/target-version, Return the required version of the database, RestAdminDatabaseHandler}

@RESTDESCRIPTION
Returns the database version that this server requires.
The version is returned in the *version* attribute of the result.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Is returned in all cases.

@endDocuBlock
