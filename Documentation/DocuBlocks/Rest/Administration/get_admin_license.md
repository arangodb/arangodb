
@startDocuBlock get_admin_license
@brief Get license information

@RESTHEADER{GET /_admin/license, Return information about the current license, getLicense}

@RESTDESCRIPTION
View the license information and status of an Enterprise Edition instance.
Can be called on single servers, Coordinators, and DB-Servers.

@RESTRETURNCODES

@RESTRETURNCODE{200}

@RESTREPLYBODY{features,object,required,license_features}

@RESTSTRUCT{expires,license_features,number,required,}
The `expires` key lists the expiry date as Unix timestamp (seconds since
January 1st, 1970 UTC).

@RESTREPLYBODY{license,string,required,}
The encrypted license key in Base64 encoding.

@RESTREPLYBODY{version,number,required,}
The license version number.

@RESTREPLYBODY{status,string,required,}
The `status` key allows you to confirm the state of the installed license on a
glance. The possible values are as follows:

- `good`: The license is valid for more than 2 weeks.
- `expiring`: The license is valid for less than 2 weeks.
- `expired`: The license has expired. In this situation, no new
  Enterprise Edition features can be utilized.
- `read-only`: The license is expired over 2 weeks. The instance is now
  restricted to read-only mode.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestAdminLicenseGet_cluster}
    var assertTypeOf = require("jsunity").jsUnity.assertions.assertTypeOf;
    var url = "/_admin/license";
    var response = logCurlRequest('GET', url);

    assert(response.code === 200);
    var body = JSON.parse(response.body);
    assertTypeOf("string", body.license);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
