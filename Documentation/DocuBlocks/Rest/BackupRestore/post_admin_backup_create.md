@startDocuBlock post_admin_backup_create
@brief creates an online backup

@RESTHEADER{POST /_admin/backup/create, Create backup}

@RESTDESCRIPTION
Creates a consistent online backup, very much like a snapshot in time,
with a given label "as soon as possible". The ambiguity in the word
soon refers to the next window during which a global write lock
across all databases can be obtained in order to guarantee consistency.

The request must contain an object with the following attributes:

@RESTBODYPARAM{label,string,required,string}
The label for this backup. The label is used to together with a
timestamp string create a unique backup identifier, `<timestamp>_<label>`

@RESTRETURNCODES

@RESTRETURNCODE{400}
If the create command is invoced with bad parameters or any HTTP
method other than `POST`, then a *HTTP 400* is returned. The specifics
are detailed in the error document.

@RESTRETURNCODE{408}
Returned if the operation cannot obtain a global transaction lock within 120
seconds, then a *HTTP 408* is returned.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestBackupCreateBackup}
    var url = "/_api/backup/create";
    var body = {
      label: "foo"
    };

    var reponse = logCurlRequest('POST', url, body);
    
    assert(response.code === 201);
    
    logJSONResponse(response);
    body = {
      result: {
        id: "2019-04-28T12.00.00Z"
      }
    };
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
