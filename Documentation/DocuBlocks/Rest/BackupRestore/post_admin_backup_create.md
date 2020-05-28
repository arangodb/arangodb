@startDocuBlock post_admin_backup_create
@brief creates a local backup

@RESTHEADER{POST /_admin/backup/create, Create backup}

@RESTDESCRIPTION
Creates a consistent backup "as soon as possible", very much
like a snapshot in time, with a given label. The ambiguity in the
phrase "as soon as possible" refers to the next window during which a
global write lock across all databases can be obtained in order to
guarantee consistency. Note that the backup at first resides on the
same machine and hard drive as the original data. Make sure to upload
it to a remote site for an actual backup.

@RESTBODYPARAM{label,string,optional,string}
The label for this backup. The label is used together with a
timestamp string create a unique backup identifier, `<timestamp>_<label>`.
If no label is specified, the empty string is assumed and a default
UUID is created for this part of the ID.

@RESTBODYPARAM{timeout,number,optional,double}
The time in seconds that the operation tries to get a consistent
snapshot. The default is 120 seconds.

@RESTBODYPARAM{allowInconsistent,boolean,optional,}
If this flag is set to `true` and no global transaction lock can be
acquired within the given timeout, a possibly inconsistent backup
is taken. The default for this flag is `false` and in this case
a timeout results in an HTTP 408 error.

@RESTBODYPARAM{force,boolean,optional,}
If this flag is set to `true` and no global transaction lock can be acquired
within the given timeout, all running transactions are forcefully aborted to
ensure that a consistent backup can be created. This does not include 
JavaScript transactions. It waits for the transactions to be aborted at most 
`timeout` seconds. Thus using `force` the request timeout is doubled.
To abort transactions is almost certainly not what you want for your application. 
In the presence of intermediate commits it can even destroy the atomicity of your
transactions. Use at your own risk, and only if you need a consistent backup at 
all costs. The default and recommended value is `false`. If both 
`allowInconsistent` and `force` are set to `true`, then the latter takes 
precedence and transactions are aborted. This is only available in the cluster.

@RESTRETURNCODES

@RESTRETURNCODE{201}
If all is well, code 201 is returned.

@RESTRETURNCODE{400}
If the create command is invoked with bad parameters or any HTTP
method other than `POST`, then an *HTTP 400* is returned. The specifics
are detailed in the returned error document.

@RESTRETURNCODE{408}
If the operation cannot obtain a global transaction lock
within the timeout, then an *HTTP 408* is returned.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestBackupCreateBackup_rocksdb}
    var url = "/_admin/backup/create";
    var body = {
      label: "foo"
    };

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 201);

    logJsonResponse(response);
    body = {
      error:false, code:201,
      result: {
        id: "2019-04-28T12.00.00Z_foo"
      }
    };
@END_EXAMPLE_ARANGOSH_RUN

The result `body` contains besides the above discussed error codes the `result` object, if `code` is equal to `201`, which holds the unique identifier of this hot backup as the string attibute `id`, the full size in bytes as `sizeInBytes`, the number of idividual files as `nrFiles` and the number of database servers as `nrDBServers`. Single server deployments list potentially misleadingly `nrDBServers: 1`. Furthermore, the body contains a `datetime` time stamp and the flag `potentiallyInconsistent`, which indicates that the backup could inconsistent. This only happens if `allowInconsistent` has happened.

@endDocuBlock
