@startDocuBlock post_admin_backup_restore
@brief restores from a local backup

@RESTHEADER{POST /_admin/backup/restore, Restore backup}

@RESTDESCRIPTION
Restores a consistent backup from a
snapshot in time, with a given id. The backup snapshot must reside on
the ArangoDB service locally.

@RESTBODYPARAM{id,string,required,string}
The id of the backup to restore from.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Is returned if the backup could be restored. Note that there is an
inevitable discrepancy between the single server and the cluster. In a
single server, the request returns successfully, but the restore is
only executed afterwards. In the cluster, the request only returns when
the restore operation has been completed successfully. The cluster
behaviour is obviously the desired one, but in a single instance, one
cannot keep a connection open across a restart.

@RESTRETURNCODE{400}
If the restore command is invoked with bad parameters or any HTTP
method other than `POST`, then an *HTTP 400* is returned. The specifics
are detailed in the returned error document.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestBackupRestoreBackup_rocksdb}
    var backup = require("@arangodb/hotbackup").create();
    var url = "/_admin/backup/restore";
    var body = {
      id: backup.id
    };

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 200);

    logJsonResponse(response);
    body = {
      error: false, code: 200,
      result: {
        "previous":"FAILSAFE", "isCluster":false
      }
    };
    // Need to wait for restore to have happened, then need to try any
    // request to reestablish connectivity such that the next request
    // will work again:
    var startTime = require("internal").time();
    var failureSeen = false;
    while (require("internal").time() - startTime < 10) {
      try {
        // GET can throw exceptions
        var r = internal.arango.GET("/_api/version");
        if (r.error === true) {
          failureSeen = true;
        } else {
          if (failureSeen) {
            break;
          }
        }
      } catch(err) {
      }
      require("internal").wait(0.1);
    }
@END_EXAMPLE_ARANGOSH_RUN

The `result` object returns the `id` of the fail safe hot backup to return to, if necessary. And the all telling `isCluster` boolean attribute.

@endDocuBlock
