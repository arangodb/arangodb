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

#ifdef __cplusplus
extern "C" {
#endif

namespace triagens {
  namespace arango {

    typedef string ClientTID;
    typedef string CoordTID;
    typedef int32_t OpID;
    typedef string ShardID;
    typedef string ServerID;

    class ClusterCommResult {
      OpID opID;
      // All the stuff from the HTTP request result

    };

    class ClusterCommCallback {
      // The idea is that one inherits from this class and implements
      // the callback.

      ClusterCommCallback();
      ~ClusterCommCallback();

      // Result indicates whether or not the returned result shall be queued
      virtual bool operator() (ClusterCommResult *);
    };

    typedef uint64_t ClusterCommTimeout;    // in microseconds

// -----------------------------------------------------------------------------
// --SECTION--                                                      ClusterComm
// -----------------------------------------------------------------------------

    class ClusterComm {
      
// -----------------------------------------------------------------------------
// --SECTION--                                     constructors and destructors
// -----------------------------------------------------------------------------
      
      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises library
////////////////////////////////////////////////////////////////////////////////
      
        ClusterComm ( );

////////////////////////////////////////////////////////////////////////////////
/// @brief shuts down library
////////////////////////////////////////////////////////////////////////////////
      
        ~ClusterComm ( );

// -----------------------------------------------------------------------------
// --SECTION--                                                   public methods
// -----------------------------------------------------------------------------
      
////////////////////////////////////////////////////////////////////////////////
/// @brief produces an operation ID which is unique in this process
////////////////////////////////////////////////////////////////////////////////
      
        OpID getOpID ( );

////////////////////////////////////////////////////////////////////////////////
/// @brief submit an HTTP request to a shard synchronously
////////////////////////////////////////////////////////////////////////////////

        int request (const ClientTID& clientTID,
                     const CoordTID&  coordTID,
                     const OpID& opID,
                     const ShardID& shardID,
                     rest::HttpRequest::HttpRequestType reqtype,
                     const string& path,
                     const char* body,
                     size_t bodyLength,
                     const map<string, string>& headerFields,
                     ClusterCommCallback* callback,
                     ClusterCommTimeout& timeout);

////////////////////////////////////////////////////////////////////////////////
/// @brief poll one answer for a given opID
////////////////////////////////////////////////////////////////////////////////

        ClusterCommResult* poll (const ClientTID& clientTID,
                                 const CoordTID&  coordTID,
                                 const OpID&      opID, 
                                 const ShardID&   shardID,
                                 bool blocking = false, 
                                 ClusterCommTimeout timeout = 0);

////////////////////////////////////////////////////////////////////////////////
/// @brief poll many answers for some given opIDs
////////////////////////////////////////////////////////////////////////////////

        vector<ClusterCommResult*> multipoll (
                const ClientTID& clientTID,
                const CoordTID&  coordTID,
                const OpID&      opID, 
                const ShardID&   shardID,
                int maxanswers = 0,
                bool blocking = false,
                ClusterCommTimeout timeout = 0);

////////////////////////////////////////////////////////////////////////////////
/// @brief ignore and drop current and future answers
////////////////////////////////////////////////////////////////////////////////

        void drop (const ClientTID& clientTID,
                   const CoordTID&  coordTID,
                   const OpID&      opID, 
                   const ShardID&   shardID);

////////////////////////////////////////////////////////////////////////////////
/// @brief send an answer HTTP request to a coordinator
////////////////////////////////////////////////////////////////////////////////
                
        int answer (rest::HttpRequest& request,
                    rest::HttpRequest::HttpRequestType reqtype,
                    const string& path,
                    const char* body,
                    size_t bodyLength,
                    const map<string, string>& headerFields,
                    ClusterCommTimeout& timeout);
                 
// -----------------------------------------------------------------------------
// --SECTION--                                                     private data
// -----------------------------------------------------------------------------

       private: 

////////////////////////////////////////////////////////////////////////////////
/// @brief process global last used operation ID
////////////////////////////////////////////////////////////////////////////////

         static OpID _lastUsedOpID;

////////////////////////////////////////////////////////////////////////////////
/// @brief global lock to protect _lastUsedOpID
////////////////////////////////////////////////////////////////////////////////

         static triagens::basics::ReadWriteLock _lock;

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


