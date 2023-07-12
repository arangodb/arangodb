@startDocuBlock get_admin_database_target_version

@RESTHEADER{GET /_admin/database/target-version, Get the required database version, getDatabaseVersion}

@RESTDESCRIPTION
Returns the database version that this server requires.
The version is returned in the `version` attribute of the result.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Is returned in all cases.

@endDocuBlock
