////////////////////////////////////////////////////////////////////////////////
/// @brief task for https communication
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Achim Brandt
/// @author Copyright 2010-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_FYN_HTTP_SERVER_HTTPS_ASYNC_COMM_TASK_H
#define TRIAGENS_FYN_HTTP_SERVER_HTTPS_ASYNC_COMM_TASK_H 1

#include "GeneralServer/GeneralAsyncCommTask.h"

#include <openssl/ssl.h>

#include <Rest/HttpHandlerFactory.h>

#include "HttpServer/HttpCommTask.h"

namespace triagens {
  namespace rest {
    class HttpServerImpl;
    class HttpCommTask;

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief task for https communication
    ////////////////////////////////////////////////////////////////////////////////

    class HttpsAsyncCommTask : public GeneralAsyncCommTask<HttpServerImpl, HttpHandlerFactory, HttpCommTask> {
      private:
        static size_t const READ_BLOCK_SIZE = 10000;

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief constructs a new task with a given socket
        ////////////////////////////////////////////////////////////////////////////////

        HttpsAsyncCommTask (HttpServerImpl*, socket_t, ConnectionInfo const&, BIO*);

      protected:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief destructs a task
        ////////////////////////////////////////////////////////////////////////////////

        ~HttpsAsyncCommTask ();

        ////////////////////////////////////////////////////////////////////////////////
        /// {@inheritDoc}
        ////////////////////////////////////////////////////////////////////////////////

        bool handleEvent (EventToken token, EventType event);

        ////////////////////////////////////////////////////////////////////////////////
        /// {@inheritDoc}
        ////////////////////////////////////////////////////////////////////////////////

        bool fillReadBuffer (bool& closed);

        ////////////////////////////////////////////////////////////////////////////////
        /// {@inheritDoc}
        ////////////////////////////////////////////////////////////////////////////////

        bool handleWrite (bool& closed, bool noWrite);

      private:
        bool trySSLAccept ();
        bool trySSLRead (bool& closed);
        bool trySSLWrite (bool& closed, bool noWrite);

      private:
        bool accepted;
        bool readBlocked;
        bool readBlockedOnWrite;
        bool writeBlockedOnRead;

        char * tmpReadBuffer;

        SSL* ssl;
        BIO* bio;
    };
  }
}

#endif
