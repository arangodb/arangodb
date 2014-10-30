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

#ifdef TRI_ENABLE_FIGURES

        StatisticsAgent ()
          : _statistics(nullptr) {
        }

#else

        StatisticsAgent () {
        }

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief destructs an agent
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_FIGURES

        ~StatisticsAgent () {
          if (_statistics != nullptr) {
            FUNC::release(_statistics);
          }
        }

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief acquires a new statistics block
////////////////////////////////////////////////////////////////////////////////

        STAT* acquire () {
#ifdef TRI_ENABLE_FIGURES

          if (_statistics != nullptr) {
            return _statistics;
          }

          return _statistics = FUNC::acquire();

#else
          return 0;
#endif
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a statistics block
////////////////////////////////////////////////////////////////////////////////

        void release () {
#ifdef TRI_ENABLE_FIGURES

          if (_statistics != nullptr) {
            FUNC::release(_statistics);
            _statistics = 0;
          }

#endif
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief transfers statistics information to another agent
////////////////////////////////////////////////////////////////////////////////

        void transfer (StatisticsAgent* agent) {
#ifdef TRI_ENABLE_FIGURES

          agent->replace(_statistics);
          _statistics = 0;

#endif
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief transfers statistics information
////////////////////////////////////////////////////////////////////////////////

        STAT* transfer () {
#ifdef TRI_ENABLE_FIGURES

          STAT* statistics = _statistics;
          _statistics = 0;

          return statistics;

#else

          return 0;

#endif
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                  public variables
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief statistics
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_FIGURES
        STAT* _statistics;
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces a statistics block
////////////////////////////////////////////////////////////////////////////////

        virtual void replace (STAT* statistics) {
#ifdef TRI_ENABLE_FIGURES

          if (_statistics != nullptr) {
            FUNC::release(_statistics);
          }

          _statistics = statistics;

#endif
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

#ifdef TRI_ENABLE_FIGURES

#define RequestStatisticsAgentSetRequestType(a,b)                                     \
  do {                                                                                \
    if (TRI_ENABLE_STATISTICS) {                                                      \
      if ((a)->RequestStatisticsAgent::_statistics != nullptr) {                      \
        (a)->RequestStatisticsAgent::_statistics->_requestType = b;                   \
      }                                                                               \
    }                                                                                 \
  }                                                                                   \
  while (0)

#else

#define RequestStatisticsAgentSetRequestType(a,b) while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the async flag
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_FIGURES

#define RequestStatisticsAgentSetAsync(a)                                             \
  do {                                                                                \
    if (TRI_ENABLE_STATISTICS) {                                                      \
      if ((a)->RequestStatisticsAgent::_statistics != nullptr) {                      \
        (a)->RequestStatisticsAgent::_statistics->_async = true;                      \
      }                                                                               \
    }                                                                                 \
  }                                                                                   \
  while (0)

#else

#define RequestStatisticsAgentSetAsync(a) while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the read start
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_FIGURES

#define RequestStatisticsAgentSetReadStart(a)                                        \
  do {                                                                               \
    if (TRI_ENABLE_STATISTICS) {                                                     \
      if ((a)->RequestStatisticsAgent::_statistics != nullptr) {                     \
        (a)->RequestStatisticsAgent::_statistics->_readStart = TRI_StatisticsTime(); \
      }                                                                              \
    }                                                                                \
  }                                                                                  \
  while (0)

#else

#define RequestStatisticsAgentSetReadStart(a) while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the read end
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_FIGURES

#define RequestStatisticsAgentSetReadEnd(a)                                        \
  do {                                                                             \
    if (TRI_ENABLE_STATISTICS) {                                                   \
      if ((a)->RequestStatisticsAgent::_statistics != nullptr) {                   \
        (a)->RequestStatisticsAgent::_statistics->_readEnd = TRI_StatisticsTime(); \
      }                                                                            \
    }                                                                              \
  }                                                                                \
  while (0)

#else

#define RequestStatisticsAgentSetReadEnd(a) while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the write start
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_FIGURES

#define RequestStatisticsAgentSetWriteStart(a)                                        \
  do {                                                                                \
    if (TRI_ENABLE_STATISTICS) {                                                      \
      if ((a)->RequestStatisticsAgent::_statistics != nullptr) {                      \
        (a)->RequestStatisticsAgent::_statistics->_writeStart = TRI_StatisticsTime(); \
      }                                                                               \
    }                                                                                 \
  }                                                                                   \
  while (0)

#else

#define RequestStatisticsAgentSetWriteStart(a) while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the write end
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_FIGURES

#define RequestStatisticsAgentSetWriteEnd(a)                                        \
  do {                                                                              \
    if (TRI_ENABLE_STATISTICS) {                                                    \
      if ((a)->RequestStatisticsAgent::_statistics != nullptr) {                    \
        (a)->RequestStatisticsAgent::_statistics->_writeEnd = TRI_StatisticsTime(); \
      }                                                                             \
    }                                                                               \
  }                                                                                 \
    while (0)

#else

#define RequestStatisticsAgentSetWriteEnd(a) while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the queue start
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_FIGURES

#define RequestStatisticsAgentSetQueueStart(a)                                        \
  do {                                                                                \
    if (TRI_ENABLE_STATISTICS) {                                                      \
      if ((a)->RequestStatisticsAgent::_statistics != nullptr) {                      \
        (a)->RequestStatisticsAgent::_statistics->_queueStart = TRI_StatisticsTime(); \
      }                                                                               \
    }                                                                                 \
  }                                                                                   \
  while (0)

#else

#define RequestStatisticsAgentSetQueueStart(a) while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the queue end
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_FIGURES

#define RequestStatisticsAgentSetQueueEnd(a)                                        \
  do {                                                                              \
    if (TRI_ENABLE_STATISTICS) {                                                    \
      if ((a)->RequestStatisticsAgent::_statistics != nullptr) {                    \
        (a)->RequestStatisticsAgent::_statistics->_queueEnd = TRI_StatisticsTime(); \
      }                                                                             \
    }                                                                               \
  }                                                                                 \
  while (0)

#else

#define RequestStatisticsAgentSetQueueEnd(a) while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the request start
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_FIGURES

#define RequestStatisticsAgentSetRequestStart(a)                                        \
  do {                                                                                  \
    if (TRI_ENABLE_STATISTICS) {                                                        \
      if ((a)->RequestStatisticsAgent::_statistics != nullptr) {                        \
        (a)->RequestStatisticsAgent::_statistics->_requestStart = TRI_StatisticsTime(); \
      }                                                                                 \
    }                                                                                   \
  }                                                                                     \
  while (0)

#else

#define RequestStatisticsAgentSetRequestStart(a) while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the request end
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_FIGURES

#define RequestStatisticsAgentSetRequestEnd(a)                                        \
  do {                                                                                \
    if (TRI_ENABLE_STATISTICS) {                                                      \
      if ((a)->RequestStatisticsAgent::_statistics != nullptr) {                      \
        (a)->RequestStatisticsAgent::_statistics->_requestEnd = TRI_StatisticsTime(); \
      }                                                                               \
    }                                                                                 \
  }                                                                                   \
  while (0)

#else

#define RequestStatisticsAgentSetRequestEnd(a) while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief sets execution error
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_FIGURES

#define RequestStatisticsAgentSetExecuteError(a)                         \
  do {                                                                   \
    if (TRI_ENABLE_STATISTICS) {                                         \
      if ((a)->RequestStatisticsAgent::_statistics != nullptr) {         \
        (a)->RequestStatisticsAgent::_statistics->_executeError = true;  \
      }                                                                  \
    }                                                                    \
  }                                                                      \
  while (0)

#else

#define RequestStatisticsAgentSetExecuteError(a) while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief sets ignore flag
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_FIGURES

#define RequestStatisticsAgentSetIgnore(a)                               \
  do {                                                                   \
    if (TRI_ENABLE_STATISTICS) {                                         \
      if ((a)->RequestStatisticsAgent::_statistics != nullptr) {         \
        (a)->RequestStatisticsAgent::_statistics->_ignore = true;        \
      }                                                                  \
    }                                                                    \
  }                                                                      \
  while (0)

#else

#define RequestStatisticsAgentSetIgnore(a) while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief adds bytes received
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_FIGURES

#define RequestStatisticsAgentAddReceivedBytes(a,b)                      \
  do {                                                                   \
    if (TRI_ENABLE_STATISTICS) {                                         \
      if ((a)->RequestStatisticsAgent::_statistics != nullptr) {         \
        (a)->RequestStatisticsAgent::_statistics->_receivedBytes += (b); \
      }                                                                  \
    }                                                                    \
  }                                                                      \
  while (0)

#else

#define RequestStatisticsAgentAddReceivedBytes(a,b) while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief adds bytes sent
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_FIGURES

#define RequestStatisticsAgentAddSentBytes(a,b)                      \
  do {                                                               \
    if (TRI_ENABLE_STATISTICS) {                                     \
      if ((a)->RequestStatisticsAgent::_statistics != nullptr) {     \
        (a)->RequestStatisticsAgent::_statistics->_sentBytes += (b); \
      }                                                              \
    }                                                                \
  }                                                                  \
  while (0)

#else

#define RequestStatisticsAgentAddSentBytes(a,b) while (0)

#endif

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

#ifdef TRI_ENABLE_FIGURES

#define ConnectionStatisticsAgentSetHttp(a)                             \
  do {                                                                  \
    if (TRI_ENABLE_STATISTICS) {                                        \
      if ((a)->ConnectionStatisticsAgent::_statistics != nullptr) {     \
        (a)->ConnectionStatisticsAgent::_statistics->_http = true;      \
      }                                                                 \
    }                                                                   \
  }                                                                     \
  while (0)

#else

#define ConnectionStatisticsAgentSetHttp(a) while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the connection start
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_FIGURES

#define ConnectionStatisticsAgentSetStart(a)                                            \
  do {                                                                                  \
    if (TRI_ENABLE_STATISTICS) {                                                        \
      if ((a)->ConnectionStatisticsAgent::_statistics != nullptr) {                     \
        (a)->ConnectionStatisticsAgent::_statistics->_connStart = TRI_StatisticsTime(); \
      }                                                                                 \
    }                                                                                   \
  }                                                                                     \
  while (0)

#else

#define ConnectionStatisticsAgentSetStart(a) while (0)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the connection end
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_FIGURES

#define ConnectionStatisticsAgentSetEnd(a)                                            \
  do {                                                                                \
    if (TRI_ENABLE_STATISTICS) {                                                      \
      if ((a)->ConnectionStatisticsAgent::_statistics != nullptr) {                   \
        (a)->ConnectionStatisticsAgent::_statistics->_connEnd = TRI_StatisticsTime(); \
      }                                                                               \
    }                                                                                 \
  }                                                                                   \
  while (0)

#else

#define ConnectionStatisticsAgentSetEnd(a) while (0)

#endif

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
