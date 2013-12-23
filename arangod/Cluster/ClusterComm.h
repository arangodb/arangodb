////////////////////////////////////////////////////////////////////////////////
/// @brief Library for intra-cluster communications
///
/// @file ClusterComm.h
///
/// DISCLAIMER
///
/// Copyright 2010-2013 triagens GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Copyright 2013, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_CLUSTER_COMM_H
#define TRIAGENS_CLUSTER_COMM_H 1

#include "BasicsC/common.h"
#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Thread.h"
#include "Rest/HttpRequest.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "VocBase/voc-types.h"

#include "Cluster/AgencyComm.h"
#include "Cluster/ClusterState.h"
#include "Cluster/ServerState.h"

namespace triagens {
  namespace arango {

// -----------------------------------------------------------------------------
// --SECTION--                                             forward declarations
// -----------------------------------------------------------------------------

    class ClusterCommThread;

// -----------------------------------------------------------------------------
// --SECTION--                                       some types for ClusterComm
// -----------------------------------------------------------------------------

    typedef string ClientTransactionID;         // Transaction ID from client
    typedef TRI_voc_tick_t CoordTransactionID;  // Coordinator transaction ID
    typedef TRI_voc_tick_t OperationID;         // Coordinator operation ID

    enum ClusterCommOpStatus {
      CL_COMM_SUBMITTED = 1,      // initial request queued, but not yet sent
      CL_COMM_SENDING = 2,        // in the process of sending
      CL_COMM_SENT = 3,           // initial request sent, response available
      CL_COMM_TIMEOUT = 4,        // no answer received until timeout
      CL_COMM_RECEIVED = 5,       // answer received
      CL_COMM_ERROR = 6,          // original request could not be sent
      CL_COMM_DROPPED = 7         // operation was dropped, not known
                                  // this is only used to report an error
                                  // in the wait or enquire methods
    };

    struct ClusterCommResult {
      bool                _deleteOnDestruction;
      ClientTransactionID clientTransactionID;
      CoordTransactionID  coordTransactionID;
      OperationID         operationID;
      ShardID             shardID;
      ServerID            serverID;   // the actual server ID of the sender
      ClusterCommOpStatus status;
      // The field result is != 0 ifs status is >= CL_COMM_SENT.
      // Note that if status is CL_COMM_TIMEOUT, then the result
      // field is a response object that only says "timeout"
      bool                dropped; // this is set to true, if the operation
                                   // is dropped whilst in state CL_COMM_SENDING
                                   // it is then actually dropped when sent
      httpclient::SimpleHttpResult* result;
      // the field answer is != 0 iff status is == CL_COMM_RECEIVED
      rest::HttpRequest* answer;

      ClusterCommResult () 
        : _deleteOnDestruction(true), dropped(false), result(0), answer(0) {}
      void doNotDeleteOnDestruction () {
        _deleteOnDestruction = false;
      }
      virtual ~ClusterCommResult () {
        if (_deleteOnDestruction && 0 != result) {
          delete result;
        }
        if (_deleteOnDestruction && 0 != answer) {
          delete answer;
        }
      }
    };

    struct ClusterCommCallback {
      // The idea is that one inherits from this class and implements
      // the callback.
      
      ClusterCommCallback () {}
      virtual ~ClusterCommCallback ();

      // Result indicates whether or not the returned result shall be queued.
      // If one returns false here, one has to call delete on the
      // ClusterCommResult*.
      virtual bool operator() (ClusterCommResult*);
    };

    typedef double ClusterCommTimeout;    // in milliseconds

    struct ClusterCommOperation : public ClusterCommResult {
      rest::HttpRequest::HttpRequestType reqtype;
      string path;
      char const* body;
      size_t bodyLength;
      map<string, string>* headerFields;
      ClusterCommCallback* callback;
      ClusterCommTimeout timeout;

      ClusterCommOperation () {}
      virtual ~ClusterCommOperation () {
        if (_deleteOnDestruction && 0 != body) {
          TRI_Free(TRI_UNKNOWN_MEM_ZONE, 
                   reinterpret_cast<void*>(const_cast<char*>(body)));
        }
        if (_deleteOnDestruction && 0 != headerFields) {
          delete headerFields;
        }
        if (_deleteOnDestruction && 0 != callback) {
          delete callback;
        }

      }
    };

    struct ClusterCommOptions {
      double _connectTimeout;
      double _requestTimeout;
      size_t _connectRetries;
      double _singleRequestTimeout;
      uint32_t _sslProtocol;
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief global callback for asynchronous REST handler
////////////////////////////////////////////////////////////////////////////////

void ClusterCommRestCallback(string& coordinator, rest::HttpResponse* response);


// -----------------------------------------------------------------------------
// --SECTION--                                                      ClusterComm
// -----------------------------------------------------------------------------

    class ClusterComm {
      
      friend class ClusterCommThread;

// -----------------------------------------------------------------------------
// --SECTION--                                     constructors and destructors
// -----------------------------------------------------------------------------
      
////////////////////////////////////////////////////////////////////////////////
/// @brief initialises library
///
/// We are a singleton class, therefore nobody is allowed to create
/// new instances or copy them, except we ourselves.
////////////////////////////////////////////////////////////////////////////////
      
        ClusterComm ( );
        ClusterComm (ClusterComm const&);    // not implemented
        void operator= (ClusterComm const&); // not implemented

////////////////////////////////////////////////////////////////////////////////
/// @brief shuts down library
////////////////////////////////////////////////////////////////////////////////
      
      public:

        ~ClusterComm ( );

// -----------------------------------------------------------------------------
// --SECTION--                                                   public methods
// -----------------------------------------------------------------------------
      
////////////////////////////////////////////////////////////////////////////////
/// @brief get the unique instance
////////////////////////////////////////////////////////////////////////////////
      
        static ClusterComm* instance ( );

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise function to call once when still single-threaded
////////////////////////////////////////////////////////////////////////////////
        
        static void initialise ();

////////////////////////////////////////////////////////////////////////////////
/// @brief cleanup function to call once when shutting down
////////////////////////////////////////////////////////////////////////////////
        
        static void cleanup () {
          delete _theinstance;
          _theinstance = 0;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief submit an HTTP request to a shard asynchronously.
///
/// This function is only called when arangod is in coordinator mode. It
/// queues a single HTTP request to one of the DBServers to be sent by
/// ClusterComm in the background thread. This request actually orders
/// an answer, which is an HTTP request sent from the target DBServer
/// back to us. Therefore ClusterComm also creates an entry in a list of
/// expected answers. One either has to use a callback for the answer,
/// or poll for it, or drop it to prevent memory leaks. The result of
/// this call is just a record that the initial HTTP request has been
/// queued (`status` is CL_COMM_SUBMITTED). Use @ref enquire below to get
/// information about the progress. The actual answer is then delivered
/// either in the callback or via poll. The caller has to call delete on
/// the resulting ClusterCommResult*. The library takes ownerships of
/// the pointers `body`, `headerFields` and `callback` and releases
/// the memory when the operation has been finished. One has to use
/// TRI_Allocate with memory zone TRI_UNKNOWN_MEM_ZONE to allocate the
/// memory to which `body` points.
////////////////////////////////////////////////////////////////////////////////

        ClusterCommResult* asyncRequest (
                ClientTransactionID const           clientTransactionID,
                CoordTransactionID const            coordTransactionID,
                ShardID const                       shardID,
                rest::HttpRequest::HttpRequestType  reqtype,
                string const                        path,
                char const*                         body,
                size_t const                        bodyLength,
                map<string, string> *               headerFields,
                ClusterCommCallback*                callback,
                ClusterCommTimeout                  timeout);

////////////////////////////////////////////////////////////////////////////////
/// @brief submit a single HTTP request to a shard synchronously.
///
/// This function does an HTTP request synchronously, waiting for the
/// result. Note that the result has `status` field set to `CL_COMM_SENT`
/// and the field `result` is set to the HTTP response. The field `answer`
/// is unused in this case. In case of a timeout the field `status` is
/// `CL_COMM_TIMEOUT` and the field `result` points to an HTTP response
/// object that only says "timeout". Note that the ClusterComm library
/// does not keep a record of this operation, in particular, you cannot
/// use @ref enquire to ask about it.
////////////////////////////////////////////////////////////////////////////////

        ClusterCommResult* syncRequest (
                ClientTransactionID const&         clientTransactionID,
                CoordTransactionID const           coordTransactionID,
                ShardID const&                     shardID,
                rest::HttpRequest::HttpRequestType reqtype,
                string const&                      path,
                char const*                        body,
                size_t const                       bodyLength,
                map<string, string> const&         headerFields,
                ClusterCommTimeout                 timeout);

////////////////////////////////////////////////////////////////////////////////
/// @brief forward an HTTP request we got from the client on to a shard 
/// asynchronously. 
/// 
/// This behaves as @ref asyncRequest except that the actual request is
/// taken from `req`. We have to add a few headers and can use callback
/// and timeout. The caller has to delete the result. The library takes
/// ownerships of the pointers `headerFields` and `callback` and
/// releases the memory when the operation has been finished. Note that
/// ClusterComm creates copy of relevant parts of the HTTP request
/// object `req`, simply because it can neither delete nor not delete
/// `req` and its children itself.

////////////////////////////////////////////////////////////////////////////////

        ClusterCommResult* asyncDelegate (
                rest::HttpRequest const&   req,
                CoordTransactionID const   coordTransactionID,
                ShardID const              shardID,
                string const               path,
                map<string, string> const* headerFields,
                ClusterCommCallback*       callback,
                ClusterCommTimeout         timeout);

////////////////////////////////////////////////////////////////////////////////
/// @brief forward an HTTP request we got from the client on to a shard 
/// synchronously. 
/// 
/// This behaves as @ref syncRequest except that the actual request is
/// taken from `req`. We have to add a few headers and can use callback
/// and timeout. The caller has to delete the result.
////////////////////////////////////////////////////////////////////////////////

        ClusterCommResult* syncDelegate (
                      rest::HttpRequest const&      req,
                      CoordTransactionID const      coordTransactionID,
                      ShardID const&                shardID,
                      const string&                 path,
                      const map<string, string>&    headerFields,
                      ClusterCommTimeout            timeout);

////////////////////////////////////////////////////////////////////////////////
/// @brief check on the status of an operation
///
/// This call never blocks and returns information about a specific operation
/// given by `operationID`. Note that if the `status` is >= `CL_COMM_SENT`, 
/// then the `result` field in the returned object is set, if the `status`
/// is `CL_COMM_RECEIVED`, then `answer` is set. However, in both cases
/// the ClusterComm library retains the operation in its queues! Therefore,
/// you have to use @ref wait or @ref drop to dequeue. Do not delete
/// `result` and `answer` before doing this! However, you have to delete
/// the ClusterCommResult pointer you get, it will automatically refrain
/// from deleting `result` and `answer`.
////////////////////////////////////////////////////////////////////////////////

        ClusterCommResult const* enquire (OperationID const operationID);

////////////////////////////////////////////////////////////////////////////////
/// @brief wait for one answer matching the criteria
///
/// If clientTransactionID is empty, then any answer with any 
/// clientTransactionID matches. If coordTransactionID is 0, then
/// any answer with any coordTransactionID matches. If shardID is
/// empty, then any answer from any ShardID matches. If operationID
/// is 0, then any answer with any operationID matches. 
/// This function returns 0 if noIf `timeout`
/// is given, the result can be 0 indicating that no matching answer
/// was available until the timeout was hit. The caller has to delete
/// the result, if it is not 0.
////////////////////////////////////////////////////////////////////////////////

        ClusterCommResult* wait (
                ClientTransactionID const& clientTransactionID,
                CoordTransactionID const   coordTransactionID,
                OperationID const          operationID,
                ShardID const&             shardID,
                ClusterCommTimeout         timeout = 0.0);

////////////////////////////////////////////////////////////////////////////////
/// @brief ignore and drop current and future answers matching
///
/// If clientTransactionID is empty, then any answer with any 
/// clientTransactionID matches. If coordTransactionID is 0, then
/// any answer with any coordTransactionID matches. If shardID is
/// empty, then any answer from any ShardID matches. If operationID
/// is 0, then any answer with any operationID matches. If there
/// is already an answer for a matching operation, it is dropped and
/// freed. If not, any future answer coming in is automatically dropped.
/// This function can be used to automatically delete all information about an
/// operation, for which @ref enquire reported successful completion.
////////////////////////////////////////////////////////////////////////////////

        void drop (ClientTransactionID const& clientTransactionID,
                   CoordTransactionID const   coordTransactionID,
                   OperationID const          operationID,
                   ShardID const&             shardID);

////////////////////////////////////////////////////////////////////////////////
/// @brief process an answer coming in on the HTTP socket which is actually
/// an answer to one of our earlier requests, return value of "" means OK
/// and nonempty is an error. This is only called in a coordinator node
/// and not in a DBServer node.
////////////////////////////////////////////////////////////////////////////////
                
        string processAnswer(string& coordinatorHeader,
                             rest::HttpRequest* answer);
                 
////////////////////////////////////////////////////////////////////////////////
/// @brief send an answer HTTP request to a coordinator, which contains
/// in its body a HttpResponse that we already have. This is only called in
/// a DBServer node and never in a coordinator node.
////////////////////////////////////////////////////////////////////////////////
                
        void asyncAnswer (string& coordinatorHeader,
                          rest::HttpResponse* responseToSend);
                     
// -----------------------------------------------------------------------------
// --SECTION--                                         private methods and data
// -----------------------------------------------------------------------------

      private: 

////////////////////////////////////////////////////////////////////////////////
/// @brief the pointer to the singleton instance
////////////////////////////////////////////////////////////////////////////////

        static ClusterComm* _theinstance;
         
////////////////////////////////////////////////////////////////////////////////
/// @brief global options for connections
////////////////////////////////////////////////////////////////////////////////

        static ClusterCommOptions _globalConnectionOptions;

////////////////////////////////////////////////////////////////////////////////
/// @brief produces an operation ID which is unique in this process
////////////////////////////////////////////////////////////////////////////////
      
        static OperationID getOperationID ();

////////////////////////////////////////////////////////////////////////////////
/// @brief get timestamp
////////////////////////////////////////////////////////////////////////////////

        static double now () {
          struct timeval tv;
          gettimeofday(&tv, 0);

          double sec = (double) tv.tv_sec; // seconds
          double usc = (double) tv.tv_usec; // microseconds

          return sec + usc / 1000000.0;
        }

        static int const maxConnectionsPerServer = 2;

        struct SingleServerConnection {
          httpclient::GeneralClientConnection* connection;
          rest::Endpoint* endpoint;
          time_t lastUsed;
          ServerID serverID;

          SingleServerConnection (httpclient::GeneralClientConnection* c,
                                  rest::Endpoint* e,
                                  ServerID s)
              : connection(c), endpoint(e), lastUsed(0), serverID(s) {}
          ~SingleServerConnection ();
        };

        struct ServerConnections {
          vector<SingleServerConnection*> connections;
          list<SingleServerConnection*> unused;
          triagens::basics::ReadWriteLock lock;

          ServerConnections () {}
          ~ServerConnections ();   // closes all connections
        };

        // We keep connections to servers open but do not care
        // if they are closed. The key is the server ID.
        map<ServerID,ServerConnections*> allConnections;
        triagens::basics::ReadWriteLock allLock;

        SingleServerConnection* getConnection(ServerID& serverID);
        void returnConnection(SingleServerConnection* singleConnection);
        void brokenConnection(SingleServerConnection* singleConnection);
        void closeUnusedConnections();

        // The data structures for our internal queues:

        // Sending questions:
        list<ClusterCommOperation*> toSend;
        map<OperationID,list<ClusterCommOperation*>::iterator> toSendByOpID;
        triagens::basics::ConditionVariable somethingToSend;

        // Receiving answers:
        list<ClusterCommOperation*> received;
        map<OperationID,list<ClusterCommOperation*>::iterator> receivedByOpID;
        triagens::basics::ConditionVariable somethingReceived;

        // Note: If you really have to lock both `somethingToSend`
        // and `somethingReceived` at the same time (usually you should
        // not have to!), then: first lock `somethingToReceive`, then
        // lock `somethingtoSend` in this order!

        // We frequently need the following lengthy types:
        typedef list<ClusterCommOperation*>::iterator QueueIterator;
        typedef map<OperationID, QueueIterator>::iterator IndexIterator;

        // An internal function to match an operation:
        bool match (ClientTransactionID const& clientTransactionID,
                    CoordTransactionID const   coordTransactionID,
                    ShardID const&             shardID,
                    ClusterCommOperation* op);

        // Move an operation from the send to the receive queue:
        bool moveFromSendToReceived (OperationID operationID);

        // Finally, our background communications thread:
        ClusterCommThread *_backgroundThread;
    };  // end of class ClusterComm

// -----------------------------------------------------------------------------
// --SECTION--                                                ClusterCommThread
// -----------------------------------------------------------------------------

    class ClusterCommThread : public basics::Thread {

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------
      
      private:
        ClusterCommThread (ClusterCommThread const&);
        ClusterCommThread& operator= (ClusterCommThread const&);

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs the ClusterCommThread
////////////////////////////////////////////////////////////////////////////////

        ClusterCommThread ();

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys the ClusterCommThread
////////////////////////////////////////////////////////////////////////////////

        ~ClusterCommThread ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------
      
      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises the ClusterCommThread
////////////////////////////////////////////////////////////////////////////////

        bool init ();

////////////////////////////////////////////////////////////////////////////////
/// @brief stops the ClusterCommThread
////////////////////////////////////////////////////////////////////////////////

        void stop () {
          if (_stop > 0) {
            return;
          }
          
          LOG_TRACE("stopping ClusterCommThread");

          _stop = 1;
          _condition.signal();

          while (_stop != 2) {
            usleep(1000);
          }
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                    Thread methods
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief ClusterCommThread main loop
////////////////////////////////////////////////////////////////////////////////

        void run ();

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------
      
      private:

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief AgencyComm instance
////////////////////////////////////////////////////////////////////////////////

        AgencyComm _agency;

////////////////////////////////////////////////////////////////////////////////
/// @brief condition variable for ClusterCommThread
////////////////////////////////////////////////////////////////////////////////

        triagens::basics::ConditionVariable _condition;

////////////////////////////////////////////////////////////////////////////////
/// @brief stop flag
////////////////////////////////////////////////////////////////////////////////
        
        volatile sig_atomic_t _stop;

    };
  }  // namespace arango
}  // namespace triagens

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


