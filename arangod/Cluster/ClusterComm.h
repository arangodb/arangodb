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

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"
#include "Rest/HttpRequest.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "VocBase/voc-types.h"

#include "Cluster/ClusterState.h"

#ifdef __cplusplus
extern "C" {
#endif

namespace triagens {
  namespace arango {

// -----------------------------------------------------------------------------
// --SECTION--                                       some types for ClusterComm
// -----------------------------------------------------------------------------

    typedef string ClientTransactionID;         // Transaction ID from client
    typedef TRI_voc_tick_t TransactionID;       // Coordinator transaction ID
    typedef TRI_voc_tick_t OperationID;         // Coordinator operation ID

    struct ClusterCommResult {
      ClientTransactionID clientTransactionID;
      TransactionID       transactionID;
      OperationID         operationID;
      ShardID             shardID;
      ServerID            serverID;   // the actual server ID of the sender
      httpclient::SimpleHttpResult*   result;

      ClusterCommResult () {}
      ~ClusterCommResult () {
        if (0 != result) {
          delete result;
        }
      }
    };

    class ClusterCommCallback {
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

    struct ClusterCommOptions {
      double _connectTimeout;
      double _requestTimeout;
      size_t _connectRetries;
      double _singleRequestTimeout;
    };

// -----------------------------------------------------------------------------
// --SECTION--                                                      ClusterComm
// -----------------------------------------------------------------------------

    class ClusterComm {
      
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
/// @brief produces an operation ID which is unique in this process
////////////////////////////////////////////////////////////////////////////////
      
        static OperationID getOperationID ( );

////////////////////////////////////////////////////////////////////////////////
/// @brief submit an HTTP request to a shard asynchronously.
///
/// Actually, it does one synchronous HTTP request but also creates
/// an entry in a list of expected answers. One either has to use a
/// callback for the answer, or poll for it, or drop it to prevent
/// memory leaks. The result of this call is what happened in the
/// original request, which is usually just a "202 Accepted". The actual
/// answer is then delivered either in the callback or via poll. The
/// caller has to call delete on the resulting ClusterCommResult*.
////////////////////////////////////////////////////////////////////////////////

        ClusterCommResult* asyncRequest (
                ClientTransactionID const&          clientTransactionID,
                TransactionID const                 coordTransactionID,
                ShardID const&                      shardID,
                string const&                       replyToPath,
                rest::HttpRequest::HttpRequestType  reqtype,
                string const&                       path,
                char const *                        body,
                size_t const                        bodyLength,
                map<string, string> const&          headerFields,
                ClusterCommCallback*                callback,
                ClusterCommTimeout                  timeout);

////////////////////////////////////////////////////////////////////////////////
/// @brief submit a single HTTP request to a shard synchronously.
////////////////////////////////////////////////////////////////////////////////

        ClusterCommResult* syncRequest (
                ClientTransactionID const&         clientTransactionID,
                TransactionID const                coordTransactionID,
                ShardID const&                     shardID,
                rest::HttpRequest::HttpRequestType reqtype,
                string const&                      path,
                char const *                       body,
                size_t const                       bodyLength,
                map<string, string> const&         headerFields);

////////////////////////////////////////////////////////////////////////////////
/// @brief forward an HTTP request we got from the client on to a shard 
/// asynchronously. 
/// 
/// We have to add a few headers and can use callback and timeout. This
/// creates an entry in a list of expected answers. One either has to
/// use a callback for the answer, or poll for it, or drop it to prevent
/// memory leaks. The result is what happened in the original request,
/// which is usually just a "202 Accepted". The actual answer is then
/// delivered either in the callback or via poll. The caller has to
/// delete the result eventually.
////////////////////////////////////////////////////////////////////////////////

        ClusterCommResult* asyncDelegate (
                rest::HttpRequest const&   req,
                TransactionID const        coordTransactionID,
                ShardID const&             shardID,
                string const&                    replyToPath,
                const string&              path,
                const map<string, string>& headerFields,
                ClusterCommCallback*       callback,
                ClusterCommTimeout         timeout);

////////////////////////////////////////////////////////////////////////////////
/// @brief forward an HTTP request we got from the client on to a shard 
/// synchronously. 
////////////////////////////////////////////////////////////////////////////////

        ClusterCommResult* syncDelegate (
                      rest::HttpRequest const&      req,
                      TransactionID const           coordTransactionID,
                      ShardID const&                shardID,
                      const string&                 path,
                      const map<string, string>&    headerFields);

////////////////////////////////////////////////////////////////////////////////
/// @brief poll one answer matching the criteria
///
/// If clientTransactionID is empty, then any answer with any 
/// clientTransactionID matches. If coordTransactionID is 0, then
/// any answer with any coordTransactionID matches. If shardID is
/// empty, then any answer from any ShardID matches. If operationID
/// is 0, then any answer with any operationID matches. If the answer
/// is not 0, then the caller has to call delete on it.
////////////////////////////////////////////////////////////////////////////////

        ClusterCommResult* poll (
                ClientTransactionID const& clientTransactionID,
                TransactionID const        coordTransactionID,
                OperationID const          operationID,
                ShardID const&             shardID,
                bool                       blocking = false, 
                ClusterCommTimeout         timeout = 0.0);

////////////////////////////////////////////////////////////////////////////////
/// @brief poll many answers matching the criteria
///
/// If clientTransactionID is empty, then any answer with any 
/// clientTransactionID matches. If coordTransactionID is 0, then
/// any answer with any coordTransactionID matches. If shardID is
/// empty, then any answer from any ShardID matches. If operationID
/// is 0, then any answer with any operationID matches. At most maxAnswers
/// results are returned. If the answer is not 0, then the caller has to
/// call delete on it.
////////////////////////////////////////////////////////////////////////////////

        vector<ClusterCommResult*> multipoll (
                ClientTransactionID const& clientTransactionID,
                TransactionID const        coordTransactionID,
                OperationID const          operationID,
                ShardID const&             shardID,
                int const                  maxAnswers = 0,
                bool                       blocking = false,
                ClusterCommTimeout         timeout = 0);

////////////////////////////////////////////////////////////////////////////////
/// @brief ignore and drop current and future answers matching
///
/// If clientTransactionID is empty, then any answer with any 
/// clientTransactionID matches. If coordTransactionID is 0, then
/// any answer with any coordTransactionID matches. If shardID is
/// empty, then any answer from any ShardID matches. If operationID
/// is 0, then any answer with any operationID matches. At most maxAnswers
/// results are returned. If the answer is not 0, then the caller has to
/// call delete on it.
////////////////////////////////////////////////////////////////////////////////

        void drop (ClientTransactionID const& clientTransactionID,
                   TransactionID const        coordTransactionID,
                   OperationID const          operationID,
                   ShardID const&             shardID);

////////////////////////////////////////////////////////////////////////////////
/// @brief send an answer HTTP request to a coordinator, which contains
/// in its body a HttpResponse that we already have.
////////////////////////////////////////////////////////////////////////////////
                
        httpclient::SimpleHttpResult* asyncAnswer (
                       rest::HttpRequest& origRequest,
                       rest::HttpResponse& responseToSend);
                     
////////////////////////////////////////////////////////////////////////////////
/// @brief process an answer coming in on the HTTP socket which is actually
/// an answer to one of our earlier requests, return value of 0 means OK
/// and nonzero is an error.
////////////////////////////////////////////////////////////////////////////////
                
        int processAnswer(rest::HttpRequest& answer);
                 
// -----------------------------------------------------------------------------
// --SECTION--                                         private methods and data
// -----------------------------------------------------------------------------

       private: 

////////////////////////////////////////////////////////////////////////////////
/// @brief the pointer to the singleton instance
////////////////////////////////////////////////////////////////////////////////

         static ClusterComm* _theinstance;
         
         static ClusterCommOptions _globalConnectionOptions;

         static int const maxConnectionsPerServer = 10;

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

    };  // end of class ClusterComm

  }  // namespace arango
}  // namespace triagens

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


