/*jshint strict: false */

////////////////////////////////////////////////////////////////////////////////
/// @brief administration actions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014-2015 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2014-2015, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var internal = require("internal");
var actions = require("org/arangodb/actions");

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_put_admin_wal_flush
/// @brief Sync the WAL to disk.
///
/// @RESTHEADER{PUT /_admin/wal/flush, Flushes the write-ahead log}
///
/// @RESTURLPARAMETERS
///
/// @RESTQUERYPARAM{waitForSync,boolean,optional}
/// Whether or not the operation should block until the not-yet synchronized
/// data in the write-ahead log was synchronized to disk.
///
/// @RESTQUERYPARAM{waitForCollector,boolean,optional}
/// Whether or not the operation should block until the data in the flushed
/// log has been collected by the write-ahead log garbage collector. Note that
/// setting this option to *true* might block for a long time if there are
/// long-running transactions and the write-ahead log garbage collector cannot
/// finish garbage collection.
///
/// @RESTDESCRIPTION
///
/// Flushes the write-ahead log. By flushing the currently active write-ahead
/// logfile, the data in it can be transferred to collection journals and
/// datafiles. This is useful to ensure that all data for a collection is
/// present in the collection journals and datafiles, for example, when dumping
/// the data of a collection.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// Is returned if the operation succeeds.
///
/// @RESTRETURNCODE{405}
/// is returned when an invalid HTTP method is used.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_admin/wal/flush",
  prefix : false,

  callback : function (req, res) {
    if (req.requestType !== actions.PUT) {
      actions.resultUnsupported(req, res);
      return;
    }

    internal.wal.flush(req.parameters.waitForSync === "true",
                       req.parameters.waitForCollector === "true");
    actions.resultOk(req, res, actions.HTTP_OK);
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_put_admin_wal_properties
/// @brief configure parameters of the wal
///
/// @RESTHEADER{PUT /_admin/wal/properties, Configures the write-ahead log}
///
/// @RESTDESCRIPTION
///
/// Configures the behavior of the write-ahead log. The body of the request
/// must be a JSON object with the following attributes:
/// - *allowOversizeEntries*: whether or not operations that are bigger than a
///   single logfile can be executed and stored
/// - *logfileSize*: the size of each write-ahead logfile
/// - *historicLogfiles*: the maximum number of historic logfiles to keep
/// - *reserveLogfiles*: the maximum number of reserve logfiles that ArangoDB
///   allocates in the background
/// - *throttleWait*: the maximum wait time that operations will wait before
///   they get aborted if case of write-throttling (in milliseconds)
/// - *throttleWhenPending*: the number of unprocessed garbage-collection
///   operations that, when reached, will activate write-throttling. A value of
///   *0* means that write-throttling will not be triggered.
///
/// Specifying any of the above attributes is optional. Not specified attributes
/// will be ignored and the configuration for them will not be modified.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// Is returned if the operation succeeds.
///
/// @RESTRETURNCODE{405}
/// is returned when an invalid HTTP method is used.
/// @endDocuBlock
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestWalPropertiesPut}
///     var url = "/_admin/wal/properties";
///     var body = {
///       logfileSize: 32 * 1024 * 1024,
///       allowOversizeEntries: true
///     };
///     var response = logCurlRequest('PUT', url, body);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_get_admin_wal_properties
/// @brief fetch the current configuration.
///
/// @RESTHEADER{GET /_admin/wal/properties, Retrieves the configuration of the write-ahead log}
///
/// @RESTDESCRIPTION
///
/// Retrieves the configuration of the write-ahead log. The result is a JSON
/// object with the following attributes:
/// - *allowOversizeEntries*: whether or not operations that are bigger than a
///   single logfile can be executed and stored
/// - *logfileSize*: the size of each write-ahead logfile
/// - *historicLogfiles*: the maximum number of historic logfiles to keep
/// - *reserveLogfiles*: the maximum number of reserve logfiles that ArangoDB
///   allocates in the background
/// - *syncInterval*: the interval for automatic synchronization of not-yet
///   synchronized write-ahead log data (in milliseconds)
/// - *throttleWait*: the maximum wait time that operations will wait before
///   they get aborted if case of write-throttling (in milliseconds)
/// - *throttleWhenPending*: the number of unprocessed garbage-collection
///   operations that, when reached, will activate write-throttling. A value of
///   *0* means that write-throttling will not be triggered.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// Is returned if the operation succeeds.
///
/// @RESTRETURNCODE{405}
/// is returned when an invalid HTTP method is used.
/// @endDocuBlock
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestWalPropertiesGet}
///     var url = "/_admin/wal/properties";
///     var response = logCurlRequest('GET', url);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_admin/wal/properties",
  prefix : false,

  callback : function (req, res) {
    var result;

    if (req.requestType === actions.PUT) {
      var body = actions.getJsonBody(req, res);
      if (body === undefined) {
        return;
      }

      result = internal.wal.properties(body);
      actions.resultOk(req, res, actions.HTTP_OK, result);
    }
    else if (req.requestType === actions.GET) {
      result = internal.wal.properties();
      actions.resultOk(req, res, actions.HTTP_OK, result);
    }
    else {
      actions.resultUnsupported(req, res);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_get_admin_wal_transactions
/// @brief returns information about the currently running transactions
///
/// @RESTHEADER{GET /_admin/wal/transactions, Returns information about the currently running transactions}
///
/// @RESTDESCRIPTION
///
/// Returns information about the currently running transactions. The result
/// is a JSON object with the following attributes:
/// - *runningTransactions*: number of currently running transactions
/// - *minLastCollected*: minimum id of the last collected logfile (at the
///   start of each running transaction). This is *null* if no transaction is
///   running.
/// - *minLastSealed*: minimum id of the last sealed logfile (at the
///   start of each running transaction). This is *null* if no transaction is
///   running.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// Is returned if the operation succeeds.
///
/// @RESTRETURNCODE{405}
/// is returned when an invalid HTTP method is used.
/// @endDocuBlock
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestWalTransactionsGet}
///     var url = "/_admin/wal/transactions";
///     var response = logCurlRequest('GET', url);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_admin/wal/transactions",
  prefix : false,

  callback : function (req, res) {
    var result;

    if (req.requestType === actions.GET) {
      result = internal.wal.transactions();
      actions.resultOk(req, res, actions.HTTP_OK, result);
    }
    else {
      actions.resultUnsupported(req, res);
    }
  }
});

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|/// @startDocuBlock\\|// --SECTION--\\|/// @\\}"
// End:
