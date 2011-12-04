////////////////////////////////////////////////////////////////////////////////
/// @brief task for communications
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
/// @author Copyright 2009-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_GENERAL_SERVER_GENERAL_COMM_TASK_H
#define TRIAGENS_GENERAL_SERVER_GENERAL_COMM_TASK_H 1

#include <Basics/Common.h>

#include <Basics/Logger.h>
#include <Basics/StringBuffer.h>
#include <Rest/SocketTask.h>
#include <Rest/HttpRequest.h>

namespace triagens {
  namespace rest {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief task for general communication
    ////////////////////////////////////////////////////////////////////////////////

    template<typename S, typename HF>
    class GeneralCommTask : public SocketTask {
      GeneralCommTask (GeneralCommTask const&);
      GeneralCommTask const& operator= (GeneralCommTask const&);

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief constructs a new task
        ////////////////////////////////////////////////////////////////////////////////

        GeneralCommTask (S* server, socket_t fd, ConnectionInfo const& info)
          : Task("GeneralCommTask"),
            SocketTask(fd),
            server(server),
            connectionInfo(info),
            readPosition(0),
            bodyPosition(0),
            bodyLength(0),
            requestPending(false),
            closeRequested(false),
            request(0),
            readRequestBody(false) {
          LOGGER_TRACE << "connection established, client " << fd
                       << ", server ip " << connectionInfo.serverAddress
                       << ", server port " << connectionInfo.serverPort
                       << ", client ip " <<  connectionInfo.clientAddress
                       << ", client port " <<  connectionInfo.clientPort;

          pair<size_t, size_t> p = server->sizeRestrictions();

          maximalHeaderSize = p.first;
          maximalBodySize = p.second;
        }

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief handles response
        ////////////////////////////////////////////////////////////////////////////////

        void handleResponse (typename HF::GeneralResponse * response)  {
          requestPending = false;

          addResponse(response);
        }

      protected:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief destructs a task
        ////////////////////////////////////////////////////////////////////////////////

        ~GeneralCommTask () {
          LOGGER_TRACE << "connection closed, client " << commSocket;

          // free write buffers
          for (deque<basics::StringBuffer*>::iterator i = writeBuffers.begin();  i != writeBuffers.end();  i++) {
            basics::StringBuffer * buffer = *i;

            buffer->free();

            delete buffer;
          }

          writeBuffers.clear();

          // free http request
          if (request != 0) {
            delete request;
          }
        }

      protected:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief reads data from the socket
        ////////////////////////////////////////////////////////////////////////////////

        virtual bool processRead () = 0;

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief reads data from the socket
        ////////////////////////////////////////////////////////////////////////////////

        virtual void addResponse (typename HF::GeneralResponse * response) = 0;

      protected:

        ////////////////////////////////////////////////////////////////////////////////
        /// {@inheritDoc}
        ////////////////////////////////////////////////////////////////////////////////

        bool handleRead (bool& closed)  {
          bool res = fillReadBuffer(closed);

          if (res) {
            if (request == 0 || readRequestBody) {
              res = processRead();
            }
          }

          if (closed) {
            res = false;
            server->handleCommunicationClosed(this);
          }
          else if (! res) {
            server->handleCommunicationFailure(this);
          }

          return res;
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// {@inheritDoc}
        ////////////////////////////////////////////////////////////////////////////////

        void completedWriteBuffer (bool& closed) {
          fillWriteBuffer();

          if (closeRequested && ! hasWriteBuffer() && writeBuffers.empty()) {
            closed = true;
            server->handleCommunicationClosed(this);
          }
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief fills the write buffer
        ////////////////////////////////////////////////////////////////////////////////

        void fillWriteBuffer () {
          if (! hasWriteBuffer() && ! writeBuffers.empty()) {
            basics::StringBuffer * buffer = writeBuffers.front();
            writeBuffers.pop_front();

            setWriteBuffer(buffer);
          }
        }

      protected:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief the underlying server
        ////////////////////////////////////////////////////////////////////////////////

        S * const server;

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief connection info
        ////////////////////////////////////////////////////////////////////////////////

        ConnectionInfo const connectionInfo;

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief write buffers
        ////////////////////////////////////////////////////////////////////////////////

        deque<basics::StringBuffer*> writeBuffers;

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief current read position
        ////////////////////////////////////////////////////////////////////////////////

        size_t readPosition;

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief start of the body position
        ////////////////////////////////////////////////////////////////////////////////

        size_t bodyPosition;

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief body length
        ////////////////////////////////////////////////////////////////////////////////

        size_t bodyLength;

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief true if request is complete but not handled
        ////////////////////////////////////////////////////////////////////////////////

        bool requestPending;

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief true if a close has been requested by the client
        ////////////////////////////////////////////////////////////////////////////////

        bool closeRequested;

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief the request with possible incomplete body
        ////////////////////////////////////////////////////////////////////////////////

        typename HF::GeneralRequest * request;

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief true if reading the request body
        ////////////////////////////////////////////////////////////////////////////////

        bool readRequestBody;

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief the maximal header size
        ////////////////////////////////////////////////////////////////////////////////

        size_t maximalHeaderSize;

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief the maximal body size
        ////////////////////////////////////////////////////////////////////////////////

        size_t maximalBodySize;
    };
  }
}

#endif
