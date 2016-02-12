////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#ifndef LIB_REST_ARANGO_TASK_H
#define LIB_REST_ARANGO_TASK_H 1

#include "Scheduler/SocketTask.h"

#include "Basics/Mutex.h"
#include "Basics/StringBuffer.h"
#include "Basics/WorkItem.h"

#include <deque>

namespace arangodb{
  namespace rest{
    class GeneralHandler;
    class GeneralRequest;
    class GeneralResponse;
    class GeneralServer;
    class GeneralServerJob;
    class ArangoTask: public SocketTask, public RequestStatisticsAgent
     {

    public:   
      //////////////////////////////////////////////////////////////////////////////
      /// @brief create the arangotask
      //////////////////////////////////////////////////////////////////////////////
      ArangoTask(GeneralServer* server, TRI_socket_t socket, ConnectionInfo const& info, 
                                        double keepAliveTimeout, GeneralRequest::ProtocolVersion version
                                        , GeneralRequest::RequestType type, std::string typeCommTask) : 
                arangodb::rest::Task(typeCommTask),
                SocketTask(socket, keepAliveTimeout),
                _connectionInfo(info),
                _server(server),
                _writeBuffers(),
                _writeBuffersStats(), 
                _bodyLength(0),
                _requestPending(false),
                _closeRequested(false),
                _readRequestBody(false),
                _denyCredentials(false),
                _acceptDeflate(false),
                _newRequest(true),
                _isChunked(false),
                _request(nullptr),
                _protocolVersion(version),
                _requestType(type),
                _fullUrl(),
                _origin(),
                _startPosition(0),
                _sinceCompactification(0),
                _originalBodyLength(0),
                _setupDone(false){}

        virtual ~ArangoTask() {}

    private:
      ArangoTask(const ArangoTask&);
      ArangoTask& operator=(const ArangoTask&);

    public:
        
      virtual void handleResponse(GeneralResponse* response) = 0; 
    
      virtual bool processRead() = 0;

      virtual void sendChunk(basics::StringBuffer* buffer) = 0;

      virtual void finishedChunked() = 0;
      
      ////////////////////////////////////////////////////////////////////////////////
      /// @brief task set up complete
      ////////////////////////////////////////////////////////////////////////////////
      
      void setupDone() {
        _setupDone.store(true, std::memory_order_relaxed);
      }

      virtual void addResponse(GeneralResponse* response) = 0;

      void fillWriteBuffer() {
        if (!hasWriteBuffer() && !_writeBuffers.empty()) {
          basics::StringBuffer* buffer = _writeBuffers.front();
          _writeBuffers.pop_front();

          TRI_ASSERT(buffer != nullptr);

          TRI_request_statistics_t* statistics = _writeBuffersStats.front();
          _writeBuffersStats.pop_front();

          setWriteBuffer(buffer, statistics);
        }
      }

      virtual void processCorsOptions(uint32_t compatibility) = 0;

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief clears the request object
      ////////////////////////////////////////////////////////////////////////////////

      void clearRequest() {
        delete _request;
        _request = nullptr;
      }

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief decides whether or not we should send back a www-authenticate header
      ////////////////////////////////////////////////////////////////////////////////

      bool sendWwwAuthenticateHeader() const {
        bool found;
        _request->header("x-omit-www-authenticate", found);

        return !found;
      }

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief get request compatibility
      ////////////////////////////////////////////////////////////////////////////////

      int32_t getCompatibility() const {
        if (_request != nullptr) {
          return _request->compatibility();
        }

        return GeneralRequest::MinCompatibility;
      }

      bool setup(Scheduler* scheduler, EventLoop loop) {
        bool ok = SocketTask::setup(scheduler, loop);

        if (!ok) {
          return false;
        }

        _scheduler = scheduler;
        _loop = loop;
        
        setupDone();

        return true;
      }

      void cleanup() { SocketTask::cleanup(); }

      void signalTask(TaskData* data) {
        // data response
        if (data->_type == TaskData::TASK_DATA_RESPONSE) {
          data->transfer(this);
          handleResponse(data->_response.get());
          processRead();
        }

        // data chunk
        else if (data->_type == TaskData::TASK_DATA_CHUNK) {
          size_t len = data->_data.size();

          if (0 == len) {
            finishedChunked();
          } else {
            basics::StringBuffer* buffer = new basics::StringBuffer(TRI_UNKNOWN_MEM_ZONE, len);

            TRI_ASSERT(buffer != nullptr);

            buffer->appendHex(len);
            buffer->appendText(TRI_CHAR_LENGTH_PAIR("\r\n"));
            buffer->appendText(data->_data.c_str(), len);
            buffer->appendText(TRI_CHAR_LENGTH_PAIR("\r\n"));

            sendChunk(buffer);
          }
        }

        // do not know, what to do - give up
        else {
          _scheduler->destroyTask(this);
        }
      }

      void handleTimeout() {
        _clientClosed = true;
        _server->handleCommunicationClosed(this);
      }

     protected:
      //////////////////////////////////////////////////////////////////////////////
      /// @brief connection info
      //////////////////////////////////////////////////////////////////////////////

      ConnectionInfo _connectionInfo;

      //////////////////////////////////////////////////////////////////////////////
      /// @brief the underlying server
      //////////////////////////////////////////////////////////////////////////////

      GeneralServer* const _server;  

    private:
        //////////////////////////////////////////////////////////////////////////////
        /// @brief write buffers
        //////////////////////////////////////////////////////////////////////////////

        std::deque<basics::StringBuffer*> _writeBuffers;

        //////////////////////////////////////////////////////////////////////////////
        /// @brief statistics buffers
        //////////////////////////////////////////////////////////////////////////////

        std::deque<TRI_request_statistics_t*> _writeBuffersStats;

        //////////////////////////////////////////////////////////////////////////////
        /// @brief start of the body position
        //////////////////////////////////////////////////////////////////////////////

        size_t _bodyPosition;

        //////////////////////////////////////////////////////////////////////////////
        /// @brief body length
        //////////////////////////////////////////////////////////////////////////////

        size_t _bodyLength;

        //////////////////////////////////////////////////////////////////////////////
        /// @brief true if request is complete but not handled
        //////////////////////////////////////////////////////////////////////////////

        bool _requestPending;

        //////////////////////////////////////////////////////////////////////////////
        /// @brief true if a close has been requested by the client
        //////////////////////////////////////////////////////////////////////////////

        bool _closeRequested;

        //////////////////////////////////////////////////////////////////////////////
        /// @brief true if reading the request body
        //////////////////////////////////////////////////////////////////////////////

        bool _readRequestBody;

        //////////////////////////////////////////////////////////////////////////////
        /// @brief whether or not to allow credentialed requests
        ///
        /// this is only used for CORS
        //////////////////////////////////////////////////////////////////////////////

        bool _denyCredentials;

        //////////////////////////////////////////////////////////////////////////////
        /// @brief whether the client accepts deflate algorithm
        //////////////////////////////////////////////////////////////////////////////

        bool _acceptDeflate;

        //////////////////////////////////////////////////////////////////////////////
        /// @brief new request started
        //////////////////////////////////////////////////////////////////////////////

        bool _newRequest;

        //////////////////////////////////////////////////////////////////////////////
        /// @brief true if within a chunked response
        //////////////////////////////////////////////////////////////////////////////

        bool _isChunked;

        //////////////////////////////////////////////////////////////////////////////
        /// @brief the request with possible incomplete body
        //////////////////////////////////////////////////////////////////////////////

        GeneralRequest* _request;

        //////////////////////////////////////////////////////////////////////////////
        /// @brief velocystream version number used
        //////////////////////////////////////////////////////////////////////////////

        GeneralRequest::ProtocolVersion _vstreamVersion;

        //////////////////////////////////////////////////////////////////////////////
        /// @brief type of request (GET, POST, ...)
        //////////////////////////////////////////////////////////////////////////////

        GeneralRequest::RequestType _requestType;

        //////////////////////////////////////////////////////////////////////////////
        /// @brief value of requested URL
        //////////////////////////////////////////////////////////////////////////////

        std::string _fullUrl;

        //////////////////////////////////////////////////////////////////////////////
        /// @brief value of the VelocyStream origin header the client sent (if any).
        ///
        /// this is only used for CORS
        //////////////////////////////////////////////////////////////////////////////

        std::string _origin;

        //////////////////////////////////////////////////////////////////////////////
        /// @brief number of requests since last compactification
        //////////////////////////////////////////////////////////////////////////////

        size_t _sinceCompactification;

        //////////////////////////////////////////////////////////////////////////////
        /// @brief original body length
        //////////////////////////////////////////////////////////////////////////////

        size_t _originalBodyLength;

        //////////////////////////////////////////////////////////////////////////////
        /// @brief task ready
        //////////////////////////////////////////////////////////////////////////////

        std::atomic<bool> _setupDone;

        //////////////////////////////////////////////////////////////////////////////
        /// @brief the maximal header size
        //////////////////////////////////////////////////////////////////////////////

        static size_t const MaximalHeaderSize;

        GeneralRequest::ProtocolVersion _protocolVersion;
        
        //////////////////////////////////////////////////////////////////////////////
        /// @brief start position of current request
        //////////////////////////////////////////////////////////////////////////////

        size_t _startPosition;

        // //////////////////////////////////////////////////////////////////////////////
        // /// @brief the maximal body size
        // //////////////////////////////////////////////////////////////////////////////

        // static size_t const MaximalBodySize;

        // //////////////////////////////////////////////////////////////////////////////
        // /// @brief the maximal pipeline size
        // //////////////////////////////////////////////////////////////////////////////

        // static size_t const MaximalPipelineSize;
    };
  }
}
#endif


