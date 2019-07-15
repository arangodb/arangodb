@startDocuBlock post_admin_backup_download
@brief download a specific local backup

@RESTHEADER{POST /_admin/backup/download, Download a backup from a remote repository}

@RESTDESCRIPTION
Download a specific local backup from a remote repository, or query
progress on a previously scheduled download operation.

@RESTBODYPARAM{id,string,optional,string}
The identifier for this backup. This is required when a download
operation is scheduled. In this case leave out the `downloadId`
attribute.

@RESTBODYPARAM{remoteRepository,string,required,string}
URL of remote reporsitory. This is required when a download
operation is scheduled. In this case leave out the `downloadId`
attribute.

@RESTBODYPARAM{config,object,required,object}
Configuration of remote repository. This is required when a download
operation is scheduled. In this case leave out the `downloadId`
attribute.

@RESTBODYPARAM{downloadId,string,optional,string}
Download ID to specify for which download operation progress is queried.
If you specify this, leave out all other body parameters.

@RESTRETURNCODES

@RESTRETURNCODE{200}
If all is well, code 200 is returned if progress is inquired or the
operation is aborted.

@RESTRETURNCODE{202}
If all is well, code 202 is returned if a new operation is scheduled.

@RESTRETURNCODE{400}
If the download command is invoked with bad parameters or any HTTP
method other than `POST`, then an *HTTP 400* is returned.

@RESTRETURNCODE{401}
If the authentication to the rempote repository fails, then an *HTTP
401* is returned.

@RESTRETURNCODE{404}
If a backup corresponding to the identifier `id`  cannot be found, or if
there is no known download operation with the given `downloadId`.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestBackupDownloadBackup}
    try {
      require("fs").makeDirectory("/tmp/backups");
    } catch(e) {
    }
    var backup = internal.arango.POST("/_admin/backup/create","");
    var body = {"id" : backup.result.id,
                "remoteRepository": "local://tmp/backups",
                "config": {
                  "local": {
                    "type":"local"
                  }
                }
               };
    var upload = internal.arango.POST("/_admin/backup/upload",body);
    // Wait until upload complete:
    for (var count = 0; count < 30; ++count) {
      var prog = internal.arango.POST("/_admin/backup/upload",
        {"uploadId":upload.result.uploadId});
      try {
        if (prog.result.DBServers.SNGL.Status === "COMPLETED") {
          break;
        }
      } catch(e) {
      }
      internal.wait(0.5);
    }
    internal.arango.POST("/_admin/backup/delete",{id:backup.result.id});
    var url = "/_admin/backup/download";
    body = {"id" : backup.result.id,
            "remoteRepository": "local://tmp/backups",
            "config": {
              "local": {
                "type":"local"
              }
            }
           };
    var response = logCurlRequest('POST', url, body);

    assert(response.code === 202);

    logJsonResponse(response);
    body = {
      result: {
        downloadId: "10046"
      }
    };
@END_EXAMPLE_ARANGOSH_RUN

@EXAMPLE_ARANGOSH_RUN{RestBackupDownloadBackupStarted}
    try {
      require("fs").makeDirectory("/tmp/backups");
    } catch(e) {
    }
    var backup = internal.arango.POST("/_admin/backup/create","");
    var body = {"id" : backup.result.id,
                "remoteRepository": "local://tmp/backups",
                "config": {
                  "local": {
                    "type":"local"
                  }
                }
               };
    var upload = internal.arango.POST("/_admin/backup/upload",body);
    // Wait until upload complete:
    for (var count = 0; count < 30; ++count) {
      var prog = internal.arango.POST("/_admin/backup/upload",
        {"uploadId":upload.result.uploadId});
      try {
        if (prog.result.DBServers.SNGL.Status === "COMPLETED") {
          break;
        }
      } catch(e) {
      }
      internal.wait(0.5);
    }
    internal.arango.POST("/_admin/backup/delete",{id:backup.result.id});
    body = {"id" : backup.result.id,
            "remoteRepository": "local://tmp/backups",
            "config": {
              "local": {
                "type":"local"
              }
            }
           };
    var download = internal.arango.POST("/_admin/backup/download", body);

    body = {"downloadId" : download.result.downloadId};
    var url = "/_admin/backup/download";
    var response = logCurlRequest('POST', url, body);

    assert(response.code === 200);

    logJsonResponse(response);
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
