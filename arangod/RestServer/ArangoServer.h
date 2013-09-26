////////////////////////////////////////////////////////////////////////////////
/// @brief arango server
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_REST_SERVER_ARANGO_SERVER_H
#define TRIAGENS_REST_SERVER_ARANGO_SERVER_H 1

#ifdef _WIN32
  #include "BasicsC/win-utils.h"
#endif

#include "Rest/AnyServer.h"
#include "Rest/OperationMode.h"

#include "VocBase/vocbase.h"

struct TRI_server_s;
struct TRI_vocbase_defaults_s;

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

namespace triagens {
  namespace rest {
    class ApplicationDispatcher;
    class ApplicationEndpointServer;
    class ApplicationScheduler;
    class AsyncJobManager;
    class HttpServer;
    class HttpsServer;
  }

  namespace admin {
    class ApplicationAdminServer;
  }

  namespace arango {
    class ApplicationMR;
    class ApplicationV8;

// -----------------------------------------------------------------------------
// --SECTION--                                                class ArangoServer
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoDB server
////////////////////////////////////////////////////////////////////////////////

  class ArangoServer : public rest::AnyServer {
      private:
        ArangoServer (const ArangoServer&);
        ArangoServer& operator= (const ArangoServer&);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        ArangoServer (int argc, char** argv);

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~ArangoServer ();

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 AnyServer methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        void buildApplicationServer ();

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        int startupServer ();

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                             public static methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the JavaScript emergency console
////////////////////////////////////////////////////////////////////////////////

        int executeConsole (triagens::rest::OperationMode::server_operation_mode_e);

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the ruby emergency console
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MRUBY
        int executeRubyConsole ();
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief opens the system database
////////////////////////////////////////////////////////////////////////////////

        void openDatabases ();

////////////////////////////////////////////////////////////////////////////////
/// @brief closes the database
////////////////////////////////////////////////////////////////////////////////

        void closeDatabases ();
        
////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief number of command line arguments
////////////////////////////////////////////////////////////////////////////////

        int _argc;

////////////////////////////////////////////////////////////////////////////////
/// @brief command line arguments
////////////////////////////////////////////////////////////////////////////////

        char** _argv;

////////////////////////////////////////////////////////////////////////////////
/// @brief temporary path
////////////////////////////////////////////////////////////////////////////////

        std::string _tempPath;

////////////////////////////////////////////////////////////////////////////////
/// @brief application scheduler
////////////////////////////////////////////////////////////////////////////////

        rest::ApplicationScheduler* _applicationScheduler;

////////////////////////////////////////////////////////////////////////////////
/// @brief application dispatcher
////////////////////////////////////////////////////////////////////////////////

        rest::ApplicationDispatcher* _applicationDispatcher;

////////////////////////////////////////////////////////////////////////////////
/// @brief application endpoint server
////////////////////////////////////////////////////////////////////////////////

        rest::ApplicationEndpointServer* _applicationEndpointServer;

////////////////////////////////////////////////////////////////////////////////
/// @brief constructed admin server application
////////////////////////////////////////////////////////////////////////////////

        admin::ApplicationAdminServer* _applicationAdminServer;

////////////////////////////////////////////////////////////////////////////////
/// @brief asynchronous job manager
////////////////////////////////////////////////////////////////////////////////

        rest::AsyncJobManager* _jobManager;

////////////////////////////////////////////////////////////////////////////////
/// @brief application MR
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MRUBY
        ApplicationMR* _applicationMR;
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief application V8
////////////////////////////////////////////////////////////////////////////////

        ApplicationV8* _applicationV8;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not only requests to internal URLs need authentication
///
/// @CMDOPT{\--server.authenticate-system-only @CA{boolean}}
///
/// Controls whether incoming requests need authentication only if they are
/// directed to the ArangoDB's internal APIs and features, located at `/_api/`,
/// `/_admin/` etc. 
///
/// IF the flag is set to @LIT{true}, then HTTP authentication is only
/// required for requests going to URLs starting with `/_`, but not for other 
/// URLs. The flag can thus be used to expose a user-made API without HTTP
/// authentication to the outside world, but to prevent the outside world from
/// using the ArangoDB API and the admin interface without authentication.
/// Note that checking the URL is performed after any database name prefix
/// has been removed. That means when the actual URL called is 
/// `/_db/_system/myapp/myaction`, the URL `/myapp/myaction` will be used for
/// `authenticate-system-only` check.
///
/// The default is @LIT{false}.
///
/// Note that authentication still needs to be enabled for the server regularly
/// in order for HTTP authentication to be forced for the ArangoDB API and the
/// web interface.  Setting only this flag is not enough.
///
/// You can control ArangoDB's general authentication feature with the 
/// `--server.disable-authentication` flag. 
////////////////////////////////////////////////////////////////////////////////

        bool _authenticateSystemOnly;

////////////////////////////////////////////////////////////////////////////////
/// @brief disable authentication for ALL client requests
///
/// @CMDOPT{\--server.disable-authentication @CA{value}}
///
/// Setting @CA{value} to true will turn off authentication on the server side
/// so all clients can execute any action without authorisation and privilege
/// checks.
///
/// The default value is @LIT{false}.
////////////////////////////////////////////////////////////////////////////////

        bool _disableAuthentication;

////////////////////////////////////////////////////////////////////////////////
/// @brief number of dispatcher threads for non-database worker
///
/// @CMDOPT{\--server.threads @CA{number}}
///
/// Specifies the @CA{number} of threads that are spawned to handle action
/// requests using Rest, JavaScript, or Ruby.
////////////////////////////////////////////////////////////////////////////////

        int _dispatcherThreads;

////////////////////////////////////////////////////////////////////////////////
/// @brief maximum size of the dispatcher queue for asynchronous requests
///
/// @CMDOPT{\--scheduler.maximal-queue-size @CA{size}}
///
/// Specifies the maximum @CA{size} of the dispatcher queue for asynchronous
/// task execution. If the queue already contains @CA{size} tasks, new tasks
/// will be rejected until other tasks are popped from the queue. Setting this
/// value may help preventing from running out of memory if the queue is filled
/// up faster than the server can process requests.
////////////////////////////////////////////////////////////////////////////////

        size_t _dispatcherQueueSize;

////////////////////////////////////////////////////////////////////////////////
/// @brief path to the database
///
/// @CMDOPT{\--database.directory @CA{directory}}
///
/// The directory containing the collections and datafiles. Defaults
/// to @LIT{/var/lib/arango}. When specifying the database directory, please
/// make sure the directory is actually writable by the arangod process.
///
/// You should further not use a database directory which is provided by a
/// network filesystem such as NFS. The reason is that networked filesystems
/// might cause inconsistencies when there are multiple parallel readers or
/// writers or they lack features required by arangod (e.g. flock()).
///
/// @CMDOPT{@CA{directory}}
///
/// When using the command line version, you can simply supply the database
/// directory as argument.
///
/// @EXAMPLES
///
/// @verbinclude option-database-directory
////////////////////////////////////////////////////////////////////////////////

        string _databasePath;

////////////////////////////////////////////////////////////////////////////////
/// @brief default journal size
///
/// @CMDOPT{\--database.maximal-journal-size @CA{size}}
///
/// Maximal size of journal in bytes. Can be overwritten when creating a new
/// collection. Note that this also limits the maximal size of a single
/// document.
///
/// The default is @LIT{32MB}.
////////////////////////////////////////////////////////////////////////////////

        TRI_voc_size_t _defaultMaximalSize;

////////////////////////////////////////////////////////////////////////////////
/// @brief default wait for sync behavior
///
/// @CMDOPT{\--database.wait-for-sync @CA{boolean}}
///
/// Default wait-for-sync value. Can be overwritten when creating a new
/// collection.
///
/// The default is @LIT{false}.
////////////////////////////////////////////////////////////////////////////////

        bool _defaultWaitForSync;

////////////////////////////////////////////////////////////////////////////////
/// @brief development mode
///
/// @CMDOPT{\--development-mode}
///
/// Specifying this option will start the server in development mode. The
/// development mode forces reloading of all actions and Foxx applications on
/// every HTTP request. This is very resource-intensive and slow, but makes
/// developing server-side actions and Foxx applications much easier.
///        
/// Never use this option in production.
////////////////////////////////////////////////////////////////////////////////

        bool _developmentMode;

////////////////////////////////////////////////////////////////////////////////
/// @brief force syncing of collection properties
///
/// @CMDOPT{\--database.force-sync-properties @CA{boolean}}
///
/// Force syncing of collection properties to disk after creating a collection 
/// or updating its properties.
///
/// If turned off, syncing will still happen for collection that have a 
/// waitForSync value of @LIT{true}. If turned on, syncing of properties will
/// always happen, regardless of the value of waitForSync.
///
/// The default is @LIT{true}.
////////////////////////////////////////////////////////////////////////////////

        bool _forceSyncProperties;

////////////////////////////////////////////////////////////////////////////////
/// @brief force syncing of shape data
///
/// @CMDOPT{\--database.force-sync-shapes @CA{boolean}}
///
/// Force syncing of shape data to disk when writing shape information.
/// If turned off, syncing will still happen for shapes of collections that
/// have a waitForSync value of @LIT{true}. If turned on, syncing of shape data
/// will always happen, regardless of the value of waitForSync.
///
/// The default is @LIT{true}.
////////////////////////////////////////////////////////////////////////////////

        bool _forceSyncShapes;

////////////////////////////////////////////////////////////////////////////////
/// @brief disable the replication logger on server startup
///
/// @CMDOPT{\--server.disable-replication-logger @CA{flag}}
///
/// If @LIT{true} the server will start with the replication logger turned off,
/// even if the replication logger is configured with the `autoStart` option.
/// Using this option will not change the value of the `autoStart` option in
/// the logger configuration, but will suppress auto-starting the replication 
/// logger just once.
///
/// If the option is not used, ArangoDB will read the logger configuration from
/// the file `REPLICATION-LOGGER-CONFIG` on startup, and use the value of the
/// `autoStart` attribute from this file.
///
/// The default is @LIT{false}.
////////////////////////////////////////////////////////////////////////////////

        bool _disableReplicationLogger;

////////////////////////////////////////////////////////////////////////////////
/// @brief disable the replication applier on server startup
///
/// @CMDOPT{\--server.disable-replication-applier @CA{flag}}
///
/// If @LIT{true} the server will start with the replication applier turned off,
/// even if the replication applier is configured with the `autoStart` option.
/// Using the command-line option will not change the value of the `autoStart`
/// option in the applier configuration, but will suppress auto-starting the 
/// replication applier just once.
///
/// If the option is not used, ArangoDB will read the applier configuration from
/// the file `REPLICATION-APPLIER-CONFIG` on startup, and use the value of the
/// `autoStart` attribute from this file.
///
/// The default is @LIT{false}.
////////////////////////////////////////////////////////////////////////////////

        bool _disableReplicationApplier;

////////////////////////////////////////////////////////////////////////////////
/// @brief remove on compaction
///
/// @CMDOPT{\--database.remove-on-compaction @CA{flag}}
///
/// Normally the garbage collection will removed compacted datafile. For debug
/// purposes you can use this option to keep the old datafiles. You should
/// never set it to @LIT{false} on a live system.
///
/// The default is @LIT{true}.
////////////////////////////////////////////////////////////////////////////////

        bool _removeOnCompacted;

////////////////////////////////////////////////////////////////////////////////
/// @brief remove on drop
///
/// @CMDOPT{\--database.remove-on-drop @CA{flag}}
///
/// If @LIT{true} and you drop a collection, then they directory and all
/// associated datafiles will be removed from disk. If @LIT{false}, then they
/// collection directory will be renamed to @LIT{deleted-...}, but remains on
/// hard disk. To restore such a dropped collection, you can rename the
/// directory back to @LIT{collection-...}, but you must also edit the file
/// @LIT{parameter.json} inside the directory.
///
/// The default is @LIT{true}.
////////////////////////////////////////////////////////////////////////////////

        bool _removeOnDrop;

////////////////////////////////////////////////////////////////////////////////
/// @brief unit tests
///
/// @CMDOPT{\--javascript.unit-tests @CA{test-file}}
///
/// Runs one or more unit tests.
////////////////////////////////////////////////////////////////////////////////

        vector<string> _unitTests;

////////////////////////////////////////////////////////////////////////////////
/// @brief files to jslint
///
/// @CMDOPT{\--jslint @CA{test-file}}
///
/// Runs jslint on one or more files.
////////////////////////////////////////////////////////////////////////////////

        vector<string> _jslint;

////////////////////////////////////////////////////////////////////////////////
/// @brief run script file
///
/// @CMDOPT{\--javascript.script @CA{script-file}}
///
/// Runs the script file.
////////////////////////////////////////////////////////////////////////////////

        vector<string> _scriptFile;

////////////////////////////////////////////////////////////////////////////////
/// @brief parameters to script file
///
/// @CMDOPT{\--javascript.script-parameter @CA{script-parameter}}
///
/// Parameter to script.
////////////////////////////////////////////////////////////////////////////////

        vector<string> _scriptParameters;

////////////////////////////////////////////////////////////////////////////////
/// @brief server default language for sorting strings
///
/// @CMDOPT{\-\-default-language @CA{default-language}}
///
/// The default language ist used for sorting and comparing strings.
/// The language value is a two-letter language code (ISO-639) or it is
/// composed by a two-letter language code with and a two letter country code
/// (ISO-3166). Valid languages are "de", "en", "en_US" or "en_UK".
///
/// The default default-language is set to be the system locale on that platform.
////////////////////////////////////////////////////////////////////////////////

        string _defaultLanguage;

////////////////////////////////////////////////////////////////////////////////
/// @brief the server
////////////////////////////////////////////////////////////////////////////////

        struct TRI_server_s* _server;

    };
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
