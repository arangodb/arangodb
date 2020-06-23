@startDocuBlock post_admin_backup_list
@brief list all local backups

@RESTHEADER{POST /_admin/backup/list, List backups}

@RESTDESCRIPTION
Lists all locally found backups.

@RESTBODYPARAM{id,string,optional,string}
The body can either be empty (in which case all available backups are
listed), or it can be an object with an attribute `id`, which
is a string. In the latter case the returned list
is restricted to the backup with the given id.

@RESTRETURNCODES

@RESTRETURNCODE{200}
If all is well, code 200 is returned.

@RESTRETURNCODE{400}
If the list command is invoked with bad parameters, then an *HTTP 400*
is returned.

@RESTRETURNCODE{404}
If an `id` or a list of ids was given and the given ids were not found
as identifiers of a backup, an *HTTP 404 NOT FOUND* is returned.

@RESTRETURNCODE{405}
If the list command is invoked with any HTTP
method other than `POST`, then an *HTTP 405 METHOD NOT ALLOWED* is returned.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestBackupListBackup_rocksdb}
    var backup = require("@arangodb/hotbackup").create();
    var backup2 = require("@arangodb/hotbackup").create();
    var url = "/_admin/backup/list";
    var body = {};

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 200);

    logJsonResponse(response);
    body = {
      result: {
        list: {
            "2019-04-28T12.00.00Z_my-label": {
                "id": "2019-04-28T12.00.00Z_my-label",
                "version": "3.4.5",
                "datetime": "2019-04-28T12.00.00Z",
                "keys": [ {"sha256": "861009ec4d599fab1f40abc76e6f89880cff5833c79c548c99f9045f191cd90b"} ]
            },
            "2019-04-28T12.10.00Z-other-label": {
                "id": "2019-04-28T12.10.00Z-other-label",
                "version": "3.4.5",
                "datetime": "2019-04-28T12.10.00Z",
                "keys": [ {"sha256": "861009ed4d599fab1f40abc76e6f89880cff5833c79c548c99f9045f191cd90b"} ]
            }
        }
      }
    };
@END_EXAMPLE_ARANGOSH_RUN

The result consists of a `list` object of hot backups by their `id`, where `id` uniquely identifies a specific hot backup, `version` depicts the version of ArangoDB, which was used to create any individual hot backup and `datetime` displays the time of creation of the hot backup. Further parameters are the size of the backup in bytes as `sizeInBytes`, the number of individual data files as `nrFiles`, the number of db servers at time of creation as `nrDBServers`, the number of backup parts, which are found on the currently reachable db servers as `nrPiecesPresent`. If the backup was created allowing inconsistences, it is so denoted as `potentiallyInconsistent`. The `available` boolean parameter is tightly connected to the backup to be present and ready to be restored on all db servers. It is `true` except, when the number of db servers currently reachable does not match to the number of db servers listed in the backup.
Should the backup be encrypted the sha256 hashes of the user secrets are published here. This will allow you to use the correct
user secret for the encryption-at-rest feature to be able to restore the backup.

@endDocuBlock
