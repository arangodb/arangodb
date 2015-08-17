////////////////////////////////////////////////////////////////////////////////
/// @brief base class for input-output tasks from sockets
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
/// @author Achim Brandt
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2009-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_SCHEDULER_SOCKET_TASK_H
#define ARANGODB_SCHEDULER_SOCKET_TASK_H 1

#include "Basics/Common.h"

#include "Scheduler/Task.h"

#include "Basics/Mutex.h"
#include "Basics/Thread.h"
#include "Statistics/StatisticsAgent.h"

#ifdef _WIN32
#include "Basics/win-utils.h"
#endif

#include "Basics/socket-utils.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

namespace triagens {
  namespace basics {
    class StringBuffer;
  }

// -----------------------------------------------------------------------------
// --SECTION--                                                  class SocketTask
// -----------------------------------------------------------------------------

  namespace rest {

////////////////////////////////////////////////////////////////////////////////
/// @brief base class for input-output tasks from sockets
////////////////////////////////////////////////////////////////////////////////

    class SocketTask : virtual public Task,
                       public ConnectionStatisticsAgent {
      private:
        SocketTask (SocketTask const&);
        SocketTask& operator= (SocketTask const&);

      private:
        static size_t const READ_BLOCK_SIZE = 10000;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new task with a given socket
////////////////////////////////////////////////////////////////////////////////

      public:

        explicit
        SocketTask (TRI_socket_t, double);

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a socket task
///
/// This method will close the underlying socket.
////////////////////////////////////////////////////////////////////////////////

      protected:

        ~SocketTask ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// set a request timeout
////////////////////////////////////////////////////////////////////////////////

    public:

        void setKeepAliveTimeout (double);

// -----------------------------------------------------------------------------
// --SECTION--                                         protected virtual methods
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief fills the read buffer
///
/// The function should be called by the input task if the scheduler has
/// indicated that new data is available. It will return true, if data could
/// be read and false if the connection has been closed.
////////////////////////////////////////////////////////////////////////////////

        virtual bool fillReadBuffer ();

////////////////////////////////////////////////////////////////////////////////
/// @brief handles a read
////////////////////////////////////////////////////////////////////////////////

        virtual bool handleRead () = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief handles a write
////////////////////////////////////////////////////////////////////////////////

        virtual bool handleWrite ();

////////////////////////////////////////////////////////////////////////////////
/// @brief called if write buffer has been sent
///
/// This called is called if the current write buffer has been sent
/// completly to the client.
////////////////////////////////////////////////////////////////////////////////

        virtual void completedWriteBuffer () = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief handles a keep-alive timeout
////////////////////////////////////////////////////////////////////////////////

        virtual void handleTimeout () = 0;

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief sets an active write buffer
////////////////////////////////////////////////////////////////////////////////

        void setWriteBuffer (basics::StringBuffer*,
                             TRI_request_statistics_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief checks for presence of an active write buffer
////////////////////////////////////////////////////////////////////////////////

        bool hasWriteBuffer () const;

// -----------------------------------------------------------------------------
// --SECTION--                                                      Task methods
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        bool setup (Scheduler*, EventLoop) override;

////////////////////////////////////////////////////////////////////////////////
/// @brief cleans up the task by unregistering all watchers
////////////////////////////////////////////////////////////////////////////////

        void cleanup () override;

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        bool handleEvent (EventToken token, EventType) override;

// -----------------------------------------------------------------------------
// --SECTION--                                               protected variables
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief event for keep-alive timeout
////////////////////////////////////////////////////////////////////////////////

        EventToken _keepAliveWatcher;

////////////////////////////////////////////////////////////////////////////////
/// @brief event for read
////////////////////////////////////////////////////////////////////////////////

        EventToken _readWatcher;

////////////////////////////////////////////////////////////////////////////////
/// @brief event for write
////////////////////////////////////////////////////////////////////////////////

        EventToken _writeWatcher;

////////////////////////////////////////////////////////////////////////////////
/// @brief event for async
////////////////////////////////////////////////////////////////////////////////

        EventToken _asyncWatcher;

////////////////////////////////////////////////////////////////////////////////
/// @brief communication socket
////////////////////////////////////////////////////////////////////////////////

        TRI_socket_t _commSocket;

////////////////////////////////////////////////////////////////////////////////
/// @brief keep-alive timeout in seconds
////////////////////////////////////////////////////////////////////////////////

        double _keepAliveTimeout;

////////////////////////////////////////////////////////////////////////////////
/// @brief the current write buffer
////////////////////////////////////////////////////////////////////////////////

        basics::StringBuffer* _writeBuffer;

////////////////////////////////////////////////////////////////////////////////
/// @brief the current write buffer statistics
////////////////////////////////////////////////////////////////////////////////

        TRI_request_statistics_t* _writeBufferStatistics;

////////////////////////////////////////////////////////////////////////////////
/// @brief number of bytes already written
////////////////////////////////////////////////////////////////////////////////

        size_t _writeLength;

////////////////////////////////////////////////////////////////////////////////
/// @brief read buffer
///
/// The function fillReadBuffer stores the data in this buffer.
////////////////////////////////////////////////////////////////////////////////

        basics::StringBuffer* _readBuffer;

////////////////////////////////////////////////////////////////////////////////
/// @brief client has closed the connection
////////////////////////////////////////////////////////////////////////////////

        bool _clientClosed;

// -----------------------------------------------------------------------------
// --SECTION--                                               protected variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief current thread identifier
////////////////////////////////////////////////////////////////////////////////

        TRI_tid_t _tid;

    };
  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
