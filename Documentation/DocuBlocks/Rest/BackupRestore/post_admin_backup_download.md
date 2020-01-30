@startDocuBlock post_admin_backup_download
@brief download a specific local backup

@RESTHEADER{POST /_admin/backup/download, Download a backup from a remote repository}

@RESTDESCRIPTION
Download a specific local backup from a remote repository, or query
progress on a previously scheduled download operation, or abort
a running download operation.

@RESTBODYPARAM{id,string,optional,string}
The identifier for this backup. This is required when a download
operation is scheduled. In this case leave out the `downloadId`
attribute.

@RESTBODYPARAM{remoteRepository,string,required,string}
URL of remote repository. This is required when a download operation is
scheduled. In this case leave out the `downloadId` attribute. Provided
repository URLs are normalized and validated as follows: One single colon must
appear separating the configuration section name and the path. The URL prefix
up to the colon must exist as a key in the config object below. No slashes must
appear before the colon. Multiple back to back slashes are collapsed to one, as
`..` and `.` are applied accordingly. Local repositories must be absolute paths
and must begin with a `/`. Trailing `/` are removed.

@RESTBODYPARAM{config,object,required,object}
Configuration of remote repository. This is required when a download
operation is scheduled. In this case leave out the `downloadId`
attribute. See the description of the _arangobackup_ program in the manual
for a description of the `config` object.

@RESTBODYPARAM{downloadId,string,optional,string}
Download ID to specify for which download operation progress is queried, or
the download operation to abort.
If you specify this, leave out all the above body parameters.

@RESTBODYPARAM{abort,boolean,optional,}
Set this to `true` if a running download operation should be aborted. In
this case, the only other body parameter which is needed is `downloadId`.

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

@EXAMPLE_ARANGOSH_RUN{RestBackupDownloadBackup_rocksdb}
    var hotbackup = require("@arangodb/hotbackup");
    try {
      require("fs").makeDirectory("/tmp/backups");
    } catch(e) {
    }
    var backup = hotbackup.create();
    var upload = hotbackup.upload(backup.id, "local://tmp/backups",
                                  {local:{type:"local"}});
    // Wait until upload complete:
    for (var count = 0; count < 30; ++count) {
      var progress = hotbackup.uploadProgress(upload.uploadId);
      try {
        if (progress.DBServers.SNGL.Status === "COMPLETED") {
          break;
        }
      } catch(e) {
      }
      internal.wait(0.5);
    }
    hotbackup.delete(backup.id);
    var url = "/_admin/backup/download";
    body = {"id" : backup.id,
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

The `result` object of the body holds the `downloadId` string attribute which
can be used to follow the download process.

@EXAMPLE_ARANGOSH_RUN{RestBackupDownloadBackupStarted_rocksdb}
    var hotbackup = require("@arangodb/hotbackup");
    try {
      require("fs").makeDirectory("/tmp/backups");
    } catch(e) {
    }
    var backup = hotbackup.create();
    var body = {"id" : backup.id,
                "remoteRepository": "local://tmp/backups",
                "config": {
                  "local": {
                    "type":"local"
                  }
                }
               };
    var upload = hotbackup.upload(backup.id, "local://tmp/backups",
                                  {local:{type:"local"}});
    // Wait until upload complete:
    for (var count = 0; count < 30; ++count) {
      var progress = hotbackup.uploadProgress(upload.uploadId);
      try {
        if (progress.DBServers.SNGL.Status === "COMPLETED") {
          break;
        }
      } catch(e) {
      }
      internal.wait(0.5);
    }
    hotbackup.delete(backup.id);
    var download = hotbackup.download(backup.id, "local://tmp/backups",
                                      {local:{type:"local"}});

    body = {"downloadId" : download.downloadId};
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
