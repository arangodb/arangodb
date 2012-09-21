////////////////////////////////////////////////////////////////////////////////
/// @brief benchmark thread
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_V8_CLIENT_BENCHMARK_THREAD_H
#define TRIAGENS_V8_CLIENT_BENCHMARK_THREAD_H 1

#include "Basics/Common.h"

#include "BasicsC/hashes.h"

#include "Basics/ConditionLocker.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Thread.h"
#include "V8Client/SharedCounter.h"
#include "SimpleHttpClient/SimpleClient.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleBinaryClient.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "BinaryServer/BinaryMessage.h"
#include "ProtocolBuffers/arangodb.pb.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::httpclient;
using namespace triagens::rest;

namespace triagens {
  namespace v8client {

    struct BenchmarkRequest {
      BenchmarkRequest (const char* url, 
                        map<string, string> params, 
                        const char* payload, 
                        PB_ArangoMessageContentType contentType, 
                        SimpleHttpClient::http_method type) :
        url(url),
        params(params),
        payload(payload),
        contentType(contentType),
        type(type) {
      };

      string url;
      map<string, string> params;
      string payload;
      PB_ArangoMessageContentType contentType;
      SimpleHttpClient::http_method type;
    };

// -----------------------------------------------------------------------------
// --SECTION--                                             class BenchmarkThread
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Threading
/// @{
////////////////////////////////////////////////////////////////////////////////

    class BenchmarkThread : public Thread {
      public:
        typedef BenchmarkRequest (*GenFunc)();

        BenchmarkThread (GenFunc generate,
                         ConditionVariable* condition, 
                         const unsigned long batchSize,
                         SharedCounter<unsigned long>* operationsCounter,
                         Endpoint* endpoint, 
                         const string& username, 
                         const string& password) 
          : Thread("arangob"), 
            _generate(generate),
            _startCondition(condition),
            _batchSize(batchSize),
            _operationsCounter(operationsCounter),
            _endpoint(endpoint),
            _username(username),
            _password(password),
            _client(0),
            _connection(0) {
        }
        
        ~BenchmarkThread () {
          if (_client != 0) {
            delete _client;
          }
          
          if (_connection != 0) {
            delete _connection;
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                         virtual protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Threading
/// @{
////////////////////////////////////////////////////////////////////////////////

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief the thread program
////////////////////////////////////////////////////////////////////////////////

        virtual void run () {
          allowAsynchronousCancelation();

          _connection = GeneralClientConnection::factory(_endpoint, 5.0, 10.0, 3);
          if (_connection == 0) {
            cerr << "out of memory" << endl;
            exit(EXIT_FAILURE);
          }

          bool isBinary = _endpoint->isBinary();

          if (isBinary) {
            _client = new SimpleBinaryClient(_connection, 10.0, true);
          }
          else {
            _client = new SimpleHttpClient(_connection, 10.0, true);
            _client->setUserNamePassword("/", _username, _password);
            map<string, string> headerFields;
            SimpleHttpResult* result = _client->request(SimpleHttpClient::GET, "/_api/version", 0, 0, headerFields);
  
            if (! result || ! result->isComplete()) {
              if (result) {
                delete result;
              }
              cerr << "could not connect to server" << endl;
              exit(EXIT_FAILURE);
            }

            delete result;
          }
  
          {
            ConditionLocker guard(_startCondition);
            guard.wait();
          }

          while (1) {
            unsigned long numOps = _operationsCounter->next(_batchSize);

            if (numOps == 0) {
              break;
            }

            if (_batchSize == 1 && ! isBinary) {
              executeRequest();
            } 
            else {
              executeRequest(numOps);
            }
          }
        }


        PB_ArangoRequestType getRequestType (const SimpleHttpClient::http_method type) const {
          if (type == SimpleHttpClient::GET) {
            return PB_REQUEST_TYPE_GET;
          }
          if (type == SimpleHttpClient::POST) {
            return PB_REQUEST_TYPE_POST;
          }
          if (type == SimpleHttpClient::PUT) {
            return PB_REQUEST_TYPE_PUT;
          }
          if (type == SimpleHttpClient::DELETE) {
            return PB_REQUEST_TYPE_DELETE;
          }
          cerr << "invalid request type" << endl;
          exit(EXIT_FAILURE);
        }

        
        void executeRequest (const unsigned long numOperations) {
          PB_ArangoMessage messages;
          PB_ArangoBatchMessage* batch;
          PB_ArangoBlobRequest* blob;
          PB_ArangoKeyValue* kv;

          for (unsigned long i = 0; i < numOperations; ++i) {
            BenchmarkRequest r = _generate();

            batch = messages.add_messages();
            batch->set_type(PB_BLOB_REQUEST);
            blob = batch->mutable_blobrequest();
      
            blob->set_requesttype(getRequestType(r.type));
            blob->set_url(r.url);
            blob->set_contenttype(r.contentType);
            blob->set_content(r.payload);
      
            for (map<string, string>::const_iterator it = r.params.begin(); it != r.params.end(); ++it) {
              kv = blob->add_values();
              kv->set_key((*it).first);
              kv->set_value((*it).second);
            }
          }
    
          size_t messageSize = messages.ByteSize();
          char* message = new char[messageSize];

          if (message == 0) {
            cerr << "out of memory" << endl;
            exit(EXIT_FAILURE);
          }

          if (! messages.SerializeToArray(message, messageSize)) {
            cerr << "out of memory" << endl;
            exit(EXIT_FAILURE);
          }

          map<string, string> headerFields;
          headerFields["Content-Type"] = BinaryMessage::getContentType();
            
          //std::cout << "body length: " << messageSize << ", hash: " << TRI_FnvHashPointer(message, (size_t) messageSize) << "\n";

          SimpleHttpResult* result = _client->request(SimpleHttpClient::POST, "/_api/batch", message, (size_t) messageSize, headerFields);
          delete[] message;

          if (result == 0) {
            _operationsCounter->incFailures();
            return;
          }
          
          if (_endpoint->isBinary()) {
            PB_ArangoMessage returnMessage;

            if (! returnMessage.ParseFromArray(result->getBody().str().c_str(), result->getContentLength())) {
              _operationsCounter->incFailures();
            }
            else {
              for (int i = 0; i < returnMessage.messages_size(); ++i) {
                if (returnMessage.messages(i).blobresponse().status() >= 400) {
                  _operationsCounter->incFailures();
                } 
              }
            }
          }
          else {
            if (result->getHttpReturnCode() >= 400) { 
              _operationsCounter->incFailures();
            }
          }
          delete result;
        }


        void executeRequest () {
          BenchmarkRequest r = _generate();
          string url = r.url;

          bool found = false;
          for (map<string, string>::const_iterator i = r.params.begin(); i != r.params.end(); ++i) {
            if (! found) {
              url.append("?");
              found = true;
            }
            else {
              url.append("&");
            }

            url.append((*i).first);
            url.append("=");
            url.append((*i).second);
          }

          map<string, string> headerFields;
          SimpleHttpResult* result = _client->request(r.type, url, r.payload.c_str(), r.payload.size(), headerFields);
          if (result == 0) {
            _operationsCounter->incFailures();
            return;
          }

          if (result->getHttpReturnCode() >= 400) { 
            _operationsCounter->incFailures();
          }
          delete result;
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Threading
/// @{
////////////////////////////////////////////////////////////////////////////////

      private:

        GenFunc _generate;

////////////////////////////////////////////////////////////////////////////////
/// @brief condition variable
////////////////////////////////////////////////////////////////////////////////

        ConditionVariable* _startCondition;

////////////////////////////////////////////////////////////////////////////////
/// @brief batch size
////////////////////////////////////////////////////////////////////////////////

        const unsigned long _batchSize;

////////////////////////////////////////////////////////////////////////////////
/// @brief shared operations counter
////////////////////////////////////////////////////////////////////////////////

        SharedCounter<unsigned long>* _operationsCounter;

////////////////////////////////////////////////////////////////////////////////
/// @brief endpoint to use
////////////////////////////////////////////////////////////////////////////////

        Endpoint* _endpoint;

////////////////////////////////////////////////////////////////////////////////
/// @brief HTTP username
////////////////////////////////////////////////////////////////////////////////

        const string _username;

////////////////////////////////////////////////////////////////////////////////
/// @brief HTTP password
////////////////////////////////////////////////////////////////////////////////

        const string _password;

////////////////////////////////////////////////////////////////////////////////
/// @brief underlying client
////////////////////////////////////////////////////////////////////////////////

        triagens::httpclient::SimpleClient* _client;

////////////////////////////////////////////////////////////////////////////////
/// @brief connection to the server
////////////////////////////////////////////////////////////////////////////////

        triagens::httpclient::GeneralClientConnection* _connection;

    };
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
