////////////////////////////////////////////////////////////////////////////////
/// @brief statistics agents
///
/// @file
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
/// @author Dr. Frank Celler
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_STATISTICS_STATISTICS_AGENT_H
#define ARANGODB_STATISTICS_STATISTICS_AGENT_H 1

#include "Basics/Common.h"

#include "Statistics/statistics.h"

// -----------------------------------------------------------------------------
// --SECTION--                                             class StatisticsAgent
// -----------------------------------------------------------------------------

namespace triagens {
  namespace rest {

////////////////////////////////////////////////////////////////////////////////
/// @brief request statistics agent
////////////////////////////////////////////////////////////////////////////////

    template<typename STAT, typename FUNC>
    class StatisticsAgent {
      private:
        StatisticsAgent (StatisticsAgent const&);
        StatisticsAgent& operator= (StatisticsAgent const&);

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new agent
////////////////////////////////////////////////////////////////////////////////

        StatisticsAgent ()
          : _statistics(nullptr),
            _lastReadStart(0.0) {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructs an agent
////////////////////////////////////////////////////////////////////////////////

        ~StatisticsAgent () {
          if (_statistics != nullptr) {
            FUNC::release(_statistics);
          }
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief acquires a new statistics block
////////////////////////////////////////////////////////////////////////////////

        STAT* acquire () {
          if (_statistics != nullptr) {
            return _statistics;
          }

          _lastReadStart = 0.0;
          return _statistics = FUNC::acquire();
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a statistics block
////////////////////////////////////////////////////////////////////////////////

        void release () {
          if (_statistics != nullptr) {
            FUNC::release(_statistics);
            _statistics = nullptr;
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief transfers statistics information to another agent
////////////////////////////////////////////////////////////////////////////////

        void transfer (StatisticsAgent* agent) {
          agent->replace(_statistics);
          _statistics = nullptr;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief transfers statistics information
////////////////////////////////////////////////////////////////////////////////

        STAT* transfer () {
          STAT* statistics = _statistics;
          _statistics = nullptr;
          
          return statistics;
        }

        double elapsedSinceReadStart () {
          if (_lastReadStart != 0.0) {
            return TRI_StatisticsTime() - _lastReadStart;
          }

          return 0.0;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                  public variables
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief statistics
////////////////////////////////////////////////////////////////////////////////

        STAT* _statistics;

        double _lastReadStart;

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces a statistics block
////////////////////////////////////////////////////////////////////////////////

        virtual void replace (STAT* statistics) {
          if (_statistics != nullptr) {
            FUNC::release(_statistics);
          }

          _statistics = statistics;
        }
    };
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                      class RequestStatisticsAgent
// -----------------------------------------------------------------------------

namespace triagens {
  namespace rest {

////////////////////////////////////////////////////////////////////////////////
/// @brief request statistics description
////////////////////////////////////////////////////////////////////////////////

    struct RequestStatisticsAgentDesc {
      static TRI_request_statistics_t* acquire () {
        return TRI_AcquireRequestStatistics();
      }

      static void release (TRI_request_statistics_t* stat) {
        TRI_ReleaseRequestStatistics(stat);
      }
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief request statistics agent
////////////////////////////////////////////////////////////////////////////////

    typedef StatisticsAgent<TRI_request_statistics_t, RequestStatisticsAgentDesc> RequestStatisticsAgent;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the request type
////////////////////////////////////////////////////////////////////////////////

#define RequestStatisticsAgentSetRequestType(a,b)                                     \
  do {                                                                                \
    if (TRI_ENABLE_STATISTICS) {                                                      \
      if ((a)->RequestStatisticsAgent::_statistics != nullptr) {                      \
        (a)->RequestStatisticsAgent::_statistics->_requestType = b;                   \
      }                                                                               \
    }                                                                                 \
  }                                                                                   \
  while (0)

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the async flag
////////////////////////////////////////////////////////////////////////////////

#define RequestStatisticsAgentSetAsync(a)                                             \
  do {                                                                                \
    if (TRI_ENABLE_STATISTICS) {                                                      \
      if ((a)->RequestStatisticsAgent::_statistics != nullptr) {                      \
        (a)->RequestStatisticsAgent::_statistics->_async = true;                      \
      }                                                                               \
    }                                                                                 \
  }                                                                                   \
  while (0)

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the read start
////////////////////////////////////////////////////////////////////////////////

#define RequestStatisticsAgentSetReadStart(a)                                        \
  do {                                                                               \
    if (TRI_ENABLE_STATISTICS) {                                                     \
      if ((a)->RequestStatisticsAgent::_statistics != nullptr) {                     \
        (a)->RequestStatisticsAgent::_statistics->_readStart = TRI_StatisticsTime(); \
        (a)->RequestStatisticsAgent::_lastReadStart =                                \
          (a)->RequestStatisticsAgent::_statistics->_readStart;                      \
      }                                                                              \
    }                                                                                \
  }                                                                                  \
  while (0)

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the read end
////////////////////////////////////////////////////////////////////////////////

#define RequestStatisticsAgentSetReadEnd(a)                                        \
  do {                                                                             \
    if (TRI_ENABLE_STATISTICS) {                                                   \
      if ((a)->RequestStatisticsAgent::_statistics != nullptr) {                   \
        (a)->RequestStatisticsAgent::_statistics->_readEnd = TRI_StatisticsTime(); \
      }                                                                            \
    }                                                                              \
  }                                                                                \
  while (0)

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the write start
////////////////////////////////////////////////////////////////////////////////

#define RequestStatisticsAgentSetWriteStart(a)                                        \
  do {                                                                                \
    if (TRI_ENABLE_STATISTICS) {                                                      \
      if ((a)->RequestStatisticsAgent::_statistics != nullptr) {                      \
        (a)->RequestStatisticsAgent::_statistics->_writeStart = TRI_StatisticsTime(); \
      }                                                                               \
    }                                                                                 \
  }                                                                                   \
  while (0)

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the write end
////////////////////////////////////////////////////////////////////////////////

#define RequestStatisticsAgentSetWriteEnd(a)                                        \
  do {                                                                              \
    if (TRI_ENABLE_STATISTICS) {                                                    \
      if ((a)->RequestStatisticsAgent::_statistics != nullptr) {                    \
        (a)->RequestStatisticsAgent::_statistics->_writeEnd = TRI_StatisticsTime(); \
      }                                                                             \
    }                                                                               \
  }                                                                                 \
    while (0)

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the queue start
////////////////////////////////////////////////////////////////////////////////

#define RequestStatisticsAgentSetQueueStart(a)                                        \
  do {                                                                                \
    if (TRI_ENABLE_STATISTICS) {                                                      \
      if ((a)->RequestStatisticsAgent::_statistics != nullptr) {                      \
        (a)->RequestStatisticsAgent::_statistics->_queueStart = TRI_StatisticsTime(); \
      }                                                                               \
    }                                                                                 \
  }                                                                                   \
  while (0)

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the queue end
////////////////////////////////////////////////////////////////////////////////

#define RequestStatisticsAgentSetQueueEnd(a)                                        \
  do {                                                                              \
    if (TRI_ENABLE_STATISTICS) {                                                    \
      if ((a)->RequestStatisticsAgent::_statistics != nullptr) {                    \
        (a)->RequestStatisticsAgent::_statistics->_queueEnd = TRI_StatisticsTime(); \
      }                                                                             \
    }                                                                               \
  }                                                                                 \
  while (0)

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the request start
////////////////////////////////////////////////////////////////////////////////

#define RequestStatisticsAgentSetRequestStart(a)                                        \
  do {                                                                                  \
    if (TRI_ENABLE_STATISTICS) {                                                        \
      if ((a)->RequestStatisticsAgent::_statistics != nullptr) {                        \
        (a)->RequestStatisticsAgent::_statistics->_requestStart = TRI_StatisticsTime(); \
      }                                                                                 \
    }                                                                                   \
  }                                                                                     \
  while (0)

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the request end
////////////////////////////////////////////////////////////////////////////////

#define RequestStatisticsAgentSetRequestEnd(a)                                        \
  do {                                                                                \
    if (TRI_ENABLE_STATISTICS) {                                                      \
      if ((a)->RequestStatisticsAgent::_statistics != nullptr) {                      \
        (a)->RequestStatisticsAgent::_statistics->_requestEnd = TRI_StatisticsTime(); \
      }                                                                               \
    }                                                                                 \
  }                                                                                   \
  while (0)

////////////////////////////////////////////////////////////////////////////////
/// @brief sets execution error
////////////////////////////////////////////////////////////////////////////////

#define RequestStatisticsAgentSetExecuteError(a)                         \
  do {                                                                   \
    if (TRI_ENABLE_STATISTICS) {                                         \
      if ((a)->RequestStatisticsAgent::_statistics != nullptr) {         \
        (a)->RequestStatisticsAgent::_statistics->_executeError = true;  \
      }                                                                  \
    }                                                                    \
  }                                                                      \
  while (0)

////////////////////////////////////////////////////////////////////////////////
/// @brief sets ignore flag
////////////////////////////////////////////////////////////////////////////////

#define RequestStatisticsAgentSetIgnore(a)                               \
  do {                                                                   \
    if (TRI_ENABLE_STATISTICS) {                                         \
      if ((a)->RequestStatisticsAgent::_statistics != nullptr) {         \
        (a)->RequestStatisticsAgent::_statistics->_ignore = true;        \
      }                                                                  \
    }                                                                    \
  }                                                                      \
  while (0)

////////////////////////////////////////////////////////////////////////////////
/// @brief adds bytes received
////////////////////////////////////////////////////////////////////////////////

#define RequestStatisticsAgentAddReceivedBytes(a,b)                      \
  do {                                                                   \
    if (TRI_ENABLE_STATISTICS) {                                         \
      if ((a)->RequestStatisticsAgent::_statistics != nullptr) {         \
        (a)->RequestStatisticsAgent::_statistics->_receivedBytes += (b); \
      }                                                                  \
    }                                                                    \
  }                                                                      \
  while (0)

////////////////////////////////////////////////////////////////////////////////
/// @brief adds bytes sent
////////////////////////////////////////////////////////////////////////////////

#define RequestStatisticsAgentAddSentBytes(a,b)                      \
  do {                                                               \
    if (TRI_ENABLE_STATISTICS) {                                     \
      if ((a)->RequestStatisticsAgent::_statistics != nullptr) {     \
        (a)->RequestStatisticsAgent::_statistics->_sentBytes += (b); \
      }                                                              \
    }                                                                \
  }                                                                  \
  while (0)

// -----------------------------------------------------------------------------
// --SECTION--                                   class ConnectionStatisticsAgent
// -----------------------------------------------------------------------------

namespace triagens {
  namespace rest {

////////////////////////////////////////////////////////////////////////////////
/// @brief connection statistics description
////////////////////////////////////////////////////////////////////////////////

    struct ConnectionStatisticsAgentDesc {
      static TRI_connection_statistics_t* acquire () {
        return TRI_AcquireConnectionStatistics();
      }

      static void release (TRI_connection_statistics_t* stat) {
        TRI_ReleaseConnectionStatistics(stat);
      }
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief connection statistics agent
////////////////////////////////////////////////////////////////////////////////

    typedef StatisticsAgent<TRI_connection_statistics_t, ConnectionStatisticsAgentDesc> ConnectionStatisticsAgent;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the connection type
////////////////////////////////////////////////////////////////////////////////

#define ConnectionStatisticsAgentSetHttp(a)                             \
  do {                                                                  \
    if (TRI_ENABLE_STATISTICS) {                                        \
      if ((a)->ConnectionStatisticsAgent::_statistics != nullptr) {     \
        (a)->ConnectionStatisticsAgent::_statistics->_http = true;      \
      }                                                                 \
    }                                                                   \
  }                                                                     \
  while (0)

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the connection start
////////////////////////////////////////////////////////////////////////////////

#define ConnectionStatisticsAgentSetStart(a)                                            \
  do {                                                                                  \
    if (TRI_ENABLE_STATISTICS) {                                                        \
      if ((a)->ConnectionStatisticsAgent::_statistics != nullptr) {                     \
        (a)->ConnectionStatisticsAgent::_statistics->_connStart = TRI_StatisticsTime(); \
      }                                                                                 \
    }                                                                                   \
  }                                                                                     \
  while (0)

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the connection end
////////////////////////////////////////////////////////////////////////////////

#define ConnectionStatisticsAgentSetEnd(a)                                            \
  do {                                                                                \
    if (TRI_ENABLE_STATISTICS) {                                                      \
      if ((a)->ConnectionStatisticsAgent::_statistics != nullptr) {                   \
        (a)->ConnectionStatisticsAgent::_statistics->_connEnd = TRI_StatisticsTime(); \
      }                                                                               \
    }                                                                                 \
  }                                                                                   \
  while (0)

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
