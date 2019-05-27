@startDocuBlock post_admin_backup_upload
@brief delete a specific local backup

@RESTHEADER{POST /_admin/backup/upload, Upload a hot backup to remote
`S3` repository}

@RESTDESCRIPTION

Upload a specific local backup to a remote `S3` respoditory

@RESTBODYPARAM{id,string,required,string}
The identifier for this backup.

@RESTBODYPARAM{remoteRepository,string,required,string}
URL of remote `S3` reporsitory

@RESTBODYPARAM{config,object,required,object}
Configuration of remote `S3` repository

@RESTRETURNCODES

@RESTRETURNCODE{400}
If the create command is invoced with bad parameters or any HTTP
method other than `POST`, then a *HTTP 400* is returned.

@RESTRETURNCODE{401}
If the authentication to the rempote repository failes, then a *HTTP
400* is returned.

@RESTRETURNCODE{404}
If a backup corresponding to the identifier, `id`,  cannot be found.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestBackupListBackup}
    var url = "/_api/backup/upload";
    var body = {"id" : "2019-05-01T00.00.00Z_some-label",
                "remoteRepository": "S3://<repository-url>",
                "config": {
                  "S3": {
                    "type":"s3",
                    "provider":"aws",
                    "env_auth":"false",
                    "access_key_id":"XXX",
                    "secret_access_key":"XXXXXX",
                    "region":"us-west-2",
                    "acl":"private"}}};

    var reponse = logCurlRequest('POST', url, body);

    assert(response.code === 200);

    logJSONResponse(response);
    body = {
      result: {
        uploadId: "10046"
      }
    };
@END_EXAMPLE_ARANGOSH_RUN

@EXAMPLE_ARANGOSH_RUN{RestBackupUploadBackupStarted}
    var url = "/_api/backup/upload";
    var body = {"uploadId" : "10046"};

    var reponse = logCurlRequest('POST', url, body);

    assert(response.code === 200);

    logJSONResponse(response);
    body = {
      "result": {
        "Timestamp": "2019-05-14T14:50:56Z",
        "BackupId": "2019-05-01T00.00.00Z_some-label",
        "DBServers": {
          "SNGL": {
            "Status": "COMPLETED"
          }
        }
      }
    };
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
