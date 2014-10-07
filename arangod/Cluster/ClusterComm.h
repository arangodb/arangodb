////////////////////////////////////////////////////////////////////////////////
/// @brief Library for intra-cluster communications
///
/// @file ClusterComm.h
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2013, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_CLUSTER_CLUSTER_COMM_H
#define ARANGODB_CLUSTER_CLUSTER_COMM_H 1

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
#include "Cluster/ClusterInfo.h"
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

////////////////////////////////////////////////////////////////////////////////
/// @brief type of a client transaction ID
////////////////////////////////////////////////////////////////////////////////

    typedef string ClientTransactionID;

////////////////////////////////////////////////////////////////////////////////
/// @brief type of a coordinator transaction ID
////////////////////////////////////////////////////////////////////////////////

    typedef TRI_voc_tick_t CoordTransactionID;

////////////////////////////////////////////////////////////////////////////////
/// @brief trype of an operation ID
////////////////////////////////////////////////////////////////////////////////

    typedef TRI_voc_tick_t OperationID;

////////////////////////////////////////////////////////////////////////////////
/// @brief status of an (a-)synchronous cluster operation
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
/// @brief used to report the status, progress and possibly result of
/// an operation
////////////////////////////////////////////////////////////////////////////////

    struct ClusterCommResult {
      bool                _deleteOnDestruction;
      ClientTransactionID clientTransactionID;
      CoordTransactionID  coordTransactionID;
      OperationID         operationID;
      ShardID             shardID;
      ServerID            serverID;   // the actual server ID of the sender
      ClusterCommOpStatus status;
      bool                dropped; // this is set to true, if the operation
                                   // is dropped whilst in state CL_COMM_SENDING
                                   // it is then actually dropped when it has
                                   // been sent

      // The field result is != 0 ifs status is >= CL_COMM_SENT.
      // Note that if status is CL_COMM_TIMEOUT, then the result
      // field is a response object that only says "timeout"
      httpclient::SimpleHttpResult* result;
      // the field answer is != 0 if status is == CL_COMM_RECEIVED
      // answer_code is valid iff answer is != 0
      rest::HttpRequest* answer;
      rest::HttpResponse::HttpResponseCode answer_code;

      ClusterCommResult ()
        : _deleteOnDestruction(true), dropped(false), result(0), answer(0),
          answer_code( rest::HttpResponse::OK ) {}

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

////////////////////////////////////////////////////////////////////////////////
/// @brief type for a callback for a cluster operation
///
/// The idea is that one inherits from this class and implements
/// the callback. Note however that the callback is called whilst
/// holding the lock for the receiving (or indeed also the sending)
/// queue! Therefore the operation should be quick.
////////////////////////////////////////////////////////////////////////////////

    struct ClusterCommCallback {

      ClusterCommCallback () {}
      virtual ~ClusterCommCallback () {};

////////////////////////////////////////////////////////////////////////////////
/// @brief the actual callback function
///
/// Result indicates whether or not the returned result is already
/// fully processed. If so, it is removed from all queues. In this
/// case the object is automatically destructed, so that the
/// callback must not call delete in any case.
////////////////////////////////////////////////////////////////////////////////

      virtual bool operator() (ClusterCommResult*) = 0;
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief type of a timeout specification, is meant in seconds
////////////////////////////////////////////////////////////////////////////////

    typedef double ClusterCommTimeout;

////////////////////////////////////////////////////////////////////////////////
/// @brief used to store the status, progress and possibly result of
/// an operation
////////////////////////////////////////////////////////////////////////////////

    struct ClusterCommOperation : public ClusterCommResult {
      rest::HttpRequest::HttpRequestType reqtype;
      string path;
      string const* body;
      bool freeBody;
      map<string, string>* headerFields;
      ClusterCommCallback* callback;
      ClusterCommTimeout endTime;

      ClusterCommOperation () : body(0), headerFields(0), callback(0) {}
      virtual ~ClusterCommOperation () {
        if (_deleteOnDestruction && 0 != headerFields) {
          delete headerFields;
        }
        if (_deleteOnDestruction && 0 != callback) {
          delete callback;
        }
        if (_deleteOnDestruction && 0 != body && freeBody) {
          delete body;
        }
      }
    };


////////////////////////////////////////////////////////////////////////////////
/// @brief global callback for asynchronous REST handler
////////////////////////////////////////////////////////////////////////////////

void ClusterCommRestCallback(string& coordinator, rest::HttpResponse* response);


// -----------------------------------------------------------------------------
// --SECTION--                                                      ClusterComm
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief the class for the cluster communications library
////////////////////////////////////////////////////////////////////////////////

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

        static ClusterComm* instance ();

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise function to call once when still single-threaded
////////////////////////////////////////////////////////////////////////////////

        static void initialise ();

////////////////////////////////////////////////////////////////////////////////
/// @brief cleanup function to call once when shutting down
////////////////////////////////////////////////////////////////////////////////

        static void cleanup ();

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not connection errors should be logged as errors
////////////////////////////////////////////////////////////////////////////////

        bool enableConnectionErrorLogging (bool value = true) {
          bool result = _logConnectionErrors;

          _logConnectionErrors = value;

          return result;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not connection errors are logged as errors
////////////////////////////////////////////////////////////////////////////////

        bool logConnectionErrors () const {
          return _logConnectionErrors;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief start the communication background thread
////////////////////////////////////////////////////////////////////////////////

        void startBackgroundThread ();

////////////////////////////////////////////////////////////////////////////////
/// @brief submit an HTTP request to a shard asynchronously.
////////////////////////////////////////////////////////////////////////////////

        ClusterCommResult* asyncRequest (
                ClientTransactionID const           clientTransactionID,
                CoordTransactionID const            coordTransactionID,
                string const&                       destination,
                rest::HttpRequest::HttpRequestType  reqtype,
                string const                        path,
                string const*                       body,
                bool                                freeBody,
                map<string, string>*                headerFields,
                ClusterCommCallback*                callback,
                ClusterCommTimeout                  timeout);

////////////////////////////////////////////////////////////////////////////////
/// @brief submit a single HTTP request to a shard synchronously.
////////////////////////////////////////////////////////////////////////////////

        ClusterCommResult* syncRequest (
                ClientTransactionID const&         clientTransactionID,
                CoordTransactionID const           coordTransactionID,
                string const&                      destination,
                rest::HttpRequest::HttpRequestType reqtype,
                string const&                      path,
                string const&                      body,
                map<string, string> const&         headerFields,
                ClusterCommTimeout                 timeout);

////////////////////////////////////////////////////////////////////////////////
/// @brief check on the status of an operation
////////////////////////////////////////////////////////////////////////////////

        ClusterCommResult const* enquire (OperationID const operationID);

////////////////////////////////////////////////////////////////////////////////
/// @brief wait for one answer matching the criteria
////////////////////////////////////////////////////////////////////////////////

        ClusterCommResult* wait (
                ClientTransactionID const& clientTransactionID,
                CoordTransactionID const   coordTransactionID,
                OperationID const          operationID,
                ShardID const&             shardID,
                ClusterCommTimeout         timeout = 0.0);

////////////////////////////////////////////////////////////////////////////////
/// @brief ignore and drop current and future answers matching
////////////////////////////////////////////////////////////////////////////////

        void drop (ClientTransactionID const& clientTransactionID,
                   CoordTransactionID const   coordTransactionID,
                   OperationID const          operationID,
                   ShardID const&             shardID);

////////////////////////////////////////////////////////////////////////////////
/// @brief process an answer coming in on the HTTP socket
////////////////////////////////////////////////////////////////////////////////

        string processAnswer(string& coordinatorHeader,
                             rest::HttpRequest* answer);

////////////////////////////////////////////////////////////////////////////////
/// @brief send an answer HTTP request to a coordinator
////////////////////////////////////////////////////////////////////////////////

        void asyncAnswer (string& coordinatorHeader,
                          rest::HttpResponse* responseToSend);

// -----------------------------------------------------------------------------
// --SECTION--                                          private methods and data
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief the pointer to the singleton instance
////////////////////////////////////////////////////////////////////////////////

        static ClusterComm* _theinstance;

////////////////////////////////////////////////////////////////////////////////
/// @brief produces an operation ID which is unique in this process
////////////////////////////////////////////////////////////////////////////////

        static OperationID getOperationID ();

////////////////////////////////////////////////////////////////////////////////
/// @brief send queue with lock and index
////////////////////////////////////////////////////////////////////////////////

        list<ClusterCommOperation*> toSend;
        map<OperationID,list<ClusterCommOperation*>::iterator> toSendByOpID;
        triagens::basics::ConditionVariable somethingToSend;

////////////////////////////////////////////////////////////////////////////////
/// @brief received queue with lock and index
////////////////////////////////////////////////////////////////////////////////

        // Receiving answers:
        list<ClusterCommOperation*> received;
        map<OperationID,list<ClusterCommOperation*>::iterator> receivedByOpID;
        triagens::basics::ConditionVariable somethingReceived;

        // Note: If you really have to lock both `somethingToSend`
        // and `somethingReceived` at the same time (usually you should
        // not have to!), then: first lock `somethingToReceive`, then
        // lock `somethingtoSend` in this order!

////////////////////////////////////////////////////////////////////////////////
/// @brief iterator type which is frequently used
////////////////////////////////////////////////////////////////////////////////

        typedef list<ClusterCommOperation*>::iterator QueueIterator;

////////////////////////////////////////////////////////////////////////////////
/// @brief iterator type which is frequently used
////////////////////////////////////////////////////////////////////////////////

        typedef map<OperationID, QueueIterator>::iterator IndexIterator;

////////////////////////////////////////////////////////////////////////////////
/// @brief internal function to match an operation:
////////////////////////////////////////////////////////////////////////////////

        bool match (ClientTransactionID const& clientTransactionID,
                    CoordTransactionID const   coordTransactionID,
                    ShardID const&             shardID,
                    ClusterCommOperation* op);

////////////////////////////////////////////////////////////////////////////////
/// @brief move an operation from the send to the receive queue
////////////////////////////////////////////////////////////////////////////////

        bool moveFromSendToReceived (OperationID operationID);

////////////////////////////////////////////////////////////////////////////////
/// @brief cleanup all queues
////////////////////////////////////////////////////////////////////////////////

        void cleanupAllQueues();

////////////////////////////////////////////////////////////////////////////////
/// @brief our background communications thread
////////////////////////////////////////////////////////////////////////////////

        ClusterCommThread *_backgroundThread;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not connection errors should be logged as errors
////////////////////////////////////////////////////////////////////////////////

        bool _logConnectionErrors;

    };  // end of class ClusterComm

// -----------------------------------------------------------------------------
// --SECTION--                                                ClusterCommThread
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief our background communications thread
////////////////////////////////////////////////////////////////////////////////

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

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
