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
#include "SimpleHttpClient/GeneralClientConnection.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::httpclient;
using namespace triagens::rest;

namespace triagens {
  namespace v8client {

    struct BenchmarkRequest {
      BenchmarkRequest (const char* url, 
                        map<string, string> params, 
                        char* (*genFunc)(),
                        SimpleHttpClient::http_method type) :
        url(url),
        params(params),
        genFunc(genFunc),
        type(type),
        ptr(0) {
      };

      ~BenchmarkRequest () {
        if (ptr) {
          TRI_Free(TRI_UNKNOWN_MEM_ZONE, ptr);
        }
      }

      void createString () {
        if (genFunc == NULL) {
          cerr << "invalid call to createString" << endl;
          exit(EXIT_FAILURE);
        }
        ptr = (void*) genFunc();
      }
      
      char* getString () {
        return (char*) ptr;
      }
      
      size_t getStringLength () {
        return strlen((char*) ptr);
      }

      string url;
      map<string, string> params;
      char* (*genFunc)();
      SimpleHttpClient::http_method type;
      void* ptr;
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
            _connection(0),
            _time(0.0) {
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
  
          {
            ConditionLocker guard(_startCondition);
            guard.wait();
          }

          while (1) {
            unsigned long numOps = _operationsCounter->next(_batchSize);

            if (numOps == 0) {
              break;
            }

            if (_batchSize == 1) {
              executeRequest();
            } 
            else {
              executeRequest(numOps);
            }
          }
        }

        void executeRequest (const unsigned long numOperations) {
          /*
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
            
            if (_useJson) {
              r.createJson(blob);
              blob->set_contenttype(PB_JSON_CONTENT);
            }
            else {
              r.createString(blob);
              blob->set_contenttype(PB_NO_CONTENT);
            }
            
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

          Timing timer(Timing::TI_WALLCLOCK);
          SimpleHttpResult* result = _client->request(SimpleHttpClient::POST, "/_api/batch", message, (size_t) messageSize, headerFields);
          _time += ((double) timer.time()) / 1000000.0;
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
          */
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

          r.createString();

          map<string, string> headerFields;
          Timing timer(Timing::TI_WALLCLOCK);

          SimpleHttpResult* result = _client->request(r.type, url, r.getString(), r.getStringLength(), headerFields);
          _time += ((double) timer.time()) / 1000000.0;

          if (result == 0) {
            _operationsCounter->incFailures();
            return;
          }

          if (result->getHttpReturnCode() >= 400) { 
            _operationsCounter->incFailures();
          }
          delete result;
        }


      public:

        double getTime () const {
          return _time;
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

////////////////////////////////////////////////////////////////////////////////
/// @brief time
////////////////////////////////////////////////////////////////////////////////

        double _time;

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
