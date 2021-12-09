
@startDocuBlock put_admin_license
@brief Set a new license

@RESTHEADER{PUT /_admin/license, Set a new license, putLicense}

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{force,boolean,optional}
Set to `true` to change the license even if it expires sooner than the current one.

@RESTALLBODYPARAM{license,string,required}
The body has to contain the Base64 encoded string wrapped in double quotes.

@RESTDESCRIPTION
Set a new license for an Enterprise Edition instance.
Can be called on single servers, Coordinators, and DB-Servers.

@RESTRETURNCODES

@RESTRETURNCODE{400}
If the license expires earlier than the previously installed one

@RESTRETURNCODE{201}
License successfully deployed.

@EXAMPLES

```
shell> curl -XPUT http://localhost:8529/_admin/license -d '"<license-string>"'
```

Server response in case of success:

```json
{
  "result": {
    "error": false,
    "code": 201
  }
}
```

Server response if the new license expires sooner than the current one (requires
`?force=true` to update the license anyway):

```json
{
  "code": 400,
  "error": true,
  "errorMessage": "This license expires sooner than the existing. You may override this by specifying force=true with invocation.",
  "errorNum": 9007
}
```

In case of a different error related to an expired or invalid license, please
contact ArangoDB sales.

@endDocuBlock
