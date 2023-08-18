@startDocuBlock get_admin_database_target_version

@RESTHEADER{GET /_admin/database/target-version, Get the required database version (deprecated), getDatabaseVersion}

@HINTS
{% hint 'warning' %}
This endpoint is deprecated and should no longer be used. It will be removed from version 3.12.0 on.
Use `/_admin/version` instead.
{% endhint %}

@RESTDESCRIPTION
Returns the database version that this server requires.
The version is returned in the `version` attribute of the result.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Is returned in all cases.

@endDocuBlock
