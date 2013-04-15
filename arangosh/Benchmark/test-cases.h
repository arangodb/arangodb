////////////////////////////////////////////////////////////////////////////////
/// @brief arango benchmark test cases
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

using namespace std;
using namespace triagens::httpclient;

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

static bool DeleteCollection (SimpleClient*, const string&);

static bool CreateCollection (SimpleClient*, const string&, const int);

static bool CreateDocument (SimpleClient*, const string&, const string&);

static bool CreateIndex (SimpleClient*, const string&, const string&, const string&);

// -----------------------------------------------------------------------------
// --SECTION--                                              benchmark test cases
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                            version retrieval test
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Benchmark
/// @{
////////////////////////////////////////////////////////////////////////////////

struct VersionTest : public BenchmarkOperation {
  VersionTest ()
    : BenchmarkOperation () {
  }

  ~VersionTest () {
  }

  bool setUp (SimpleClient* client) {
    return true;
  }

  void tearDown () {
  }

  string url (const int threadNumber, const size_t threadCounter, const size_t globalCounter) {
    static string url = "/_api/version";

    return url;
  }

  HttpRequest::HttpRequestType type (const int threadNumber, const size_t threadCounter, const size_t globalCounter) {
    return HttpRequest::HTTP_REQUEST_GET;
  }

  const char* payload (size_t* length, const int threadNumber, const size_t threadCounter, const size_t globalCounter, bool* mustFree) {
    static const char* payload = "";

    *mustFree = false;
    *length = 0;
    return payload;
  }

  const map<string, string>& headers () {
    static const map<string, string> headers;
    return headers;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                         document CRUD append test
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Benchmark
/// @{
////////////////////////////////////////////////////////////////////////////////

struct DocumentCrudAppendTest : public BenchmarkOperation {
  DocumentCrudAppendTest ()
    : BenchmarkOperation () {
  }

  ~DocumentCrudAppendTest () {
  }

  bool setUp (SimpleClient* client) {
    return DeleteCollection(client, Collection) &&
           CreateCollection(client, Collection, 2);
  }

  void tearDown () {
  }

  string url (const int threadNumber, const size_t threadCounter, const size_t globalCounter) {
    const size_t mod = globalCounter % 4;

    if (mod == 0) {
      return string("/_api/document?collection=" + Collection);
    }
    else {
      size_t keyId = (size_t) (globalCounter / 4);
      const string key = "testkey" + StringUtils::itoa(keyId);

      return string("/_api/document/" + Collection + "/" + key);
    }
  }

  HttpRequest::HttpRequestType type (const int threadNumber, const size_t threadCounter, const size_t globalCounter) {
    const size_t mod = globalCounter % 4;

    if (mod == 0) {
      return HttpRequest::HTTP_REQUEST_POST;
    }
    else if (mod == 1) {
      return HttpRequest::HTTP_REQUEST_GET;
    }
    else if (mod == 2) {
      return HttpRequest::HTTP_REQUEST_PATCH;
    }
    else if (mod == 3) {
      return HttpRequest::HTTP_REQUEST_GET;
    }
    else {
      assert(false);
      return HttpRequest::HTTP_REQUEST_GET;
    }
  }

  const char* payload (size_t* length, const int threadNumber, const size_t threadCounter, const size_t globalCounter, bool* mustFree) {
    const size_t mod = globalCounter % 4;

    if (mod == 0 || mod == 2) {
      const size_t n = Complexity;
      TRI_string_buffer_t* buffer;

      buffer = TRI_CreateSizedStringBuffer(TRI_UNKNOWN_MEM_ZONE, 256);
      TRI_AppendStringStringBuffer(buffer, "{\"_key\":\"");

      size_t keyId = (size_t) (globalCounter / 4);
      const string key = "testkey" + StringUtils::itoa(keyId);
      TRI_AppendStringStringBuffer(buffer, key.c_str());
      TRI_AppendStringStringBuffer(buffer, "\"");

      for (size_t i = 1; i <= n; ++i) {
        TRI_AppendStringStringBuffer(buffer, ",\"value");
        TRI_AppendUInt32StringBuffer(buffer, (uint32_t) i);
        if (mod == 0) {
          TRI_AppendStringStringBuffer(buffer, "\":true");
        }
        else {
          TRI_AppendStringStringBuffer(buffer, "\":false");
        }
      }

      TRI_AppendCharStringBuffer(buffer, '}');

      *length = TRI_LengthStringBuffer(buffer);
      *mustFree = true;
      char* ptr = buffer->_buffer;
      buffer->_buffer = NULL;

      TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);

      return (const char*) ptr;
    }
    else if (mod == 1 || mod == 3) {
      *length = 0;
      *mustFree = false;
      return (const char*) 0;
    }
    else {
      assert(false);
      return 0;
    }
  }

  const map<string, string>& headers () {
    static const map<string, string> headers;
    return headers;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                document CRUD test
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Benchmark
/// @{
////////////////////////////////////////////////////////////////////////////////

struct DocumentCrudTest : public BenchmarkOperation {
  DocumentCrudTest ()
    : BenchmarkOperation () {
  }

  ~DocumentCrudTest () {
  }

  bool setUp (SimpleClient* client) {
    return DeleteCollection(client, Collection) &&
           CreateCollection(client, Collection, 2);
  }

  void tearDown () {
  }

  string url (const int threadNumber, const size_t threadCounter, const size_t globalCounter) {
    const size_t mod = globalCounter % 5;

    if (mod == 0) {
      return string("/_api/document?collection=" + Collection);
    }
    else {
      size_t keyId = (size_t) (globalCounter / 5);
      const string key = "testkey" + StringUtils::itoa(keyId);

      return string("/_api/document/" + Collection + "/" + key);
    }
  }

  HttpRequest::HttpRequestType type (const int threadNumber, const size_t threadCounter, const size_t globalCounter) {
    const size_t mod = globalCounter % 5;

    if (mod == 0) {
      return HttpRequest::HTTP_REQUEST_POST;
    }
    else if (mod == 1) {
      return HttpRequest::HTTP_REQUEST_GET;
    }
    else if (mod == 2) {
      return HttpRequest::HTTP_REQUEST_PATCH;
    }
    else if (mod == 3) {
      return HttpRequest::HTTP_REQUEST_GET;
    }
    else if (mod == 4) {
      return HttpRequest::HTTP_REQUEST_DELETE;
    }
    else {
      assert(false);
      return HttpRequest::HTTP_REQUEST_GET;
    }
  }

  const char* payload (size_t* length, const int threadNumber, const size_t threadCounter, const size_t globalCounter, bool* mustFree) {
    const size_t mod = globalCounter % 5;

    if (mod == 0 || mod == 2) {
      const size_t n = Complexity;
      TRI_string_buffer_t* buffer;

      buffer = TRI_CreateSizedStringBuffer(TRI_UNKNOWN_MEM_ZONE, 256);
      TRI_AppendStringStringBuffer(buffer, "{\"_key\":\"");

      size_t keyId = (size_t) (globalCounter / 5);
      const string key = "testkey" + StringUtils::itoa(keyId);
      TRI_AppendStringStringBuffer(buffer, key.c_str());
      TRI_AppendStringStringBuffer(buffer, "\"");

      for (size_t i = 1; i <= n; ++i) {
        TRI_AppendStringStringBuffer(buffer, ",\"value");
        TRI_AppendUInt32StringBuffer(buffer, (uint32_t) i);
        if (mod == 0) {
          TRI_AppendStringStringBuffer(buffer, "\":true");
        }
        else {
          TRI_AppendStringStringBuffer(buffer, "\":false");
        }
      }

      TRI_AppendCharStringBuffer(buffer, '}');

      *length = TRI_LengthStringBuffer(buffer);
      *mustFree = true;
      char* ptr = buffer->_buffer;
      buffer->_buffer = NULL;

      TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);

      return (const char*) ptr;
    }
    else if (mod == 1 || mod == 3 || mod == 4) {
      *length = 0;
      *mustFree = false;
      return (const char*) 0;
    }
    else {
      assert(false);
      return 0;
    }
  }

  const map<string, string>& headers () {
    static const map<string, string> headers;
    return headers;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    edge CRUD test
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Benchmark
/// @{
////////////////////////////////////////////////////////////////////////////////

struct EdgeCrudTest : public BenchmarkOperation {
  EdgeCrudTest ()
    : BenchmarkOperation () {
  }

  ~EdgeCrudTest () {
  }

  bool setUp (SimpleClient* client) {
    return DeleteCollection(client, Collection) &&
           CreateCollection(client, Collection, 3);
  }

  void tearDown () {
  }

  string url (const int threadNumber, const size_t threadCounter, const size_t globalCounter) {
    const size_t mod = globalCounter % 4;

    if (mod == 0) {
      return string("/_api/edge?collection=" + Collection + "&from=" + Collection.c_str() + "%2Ftestfrom" + StringUtils::itoa(globalCounter) + "&to=" + Collection.c_str() + "%2Ftestto" + StringUtils::itoa(globalCounter));
    }
    else {
      size_t keyId = (size_t) (globalCounter / 4);
      const string key = "testkey" + StringUtils::itoa(keyId);

      return string("/_api/edge/" + Collection + "/" + key);
    }
  }

  HttpRequest::HttpRequestType type (const int threadNumber, const size_t threadCounter, const size_t globalCounter) {
    const size_t mod = globalCounter % 4;

    if (mod == 0) {
      return HttpRequest::HTTP_REQUEST_POST;
    }
    else if (mod == 1) {
      return HttpRequest::HTTP_REQUEST_GET;
    }
    else if (mod == 2) {
      return HttpRequest::HTTP_REQUEST_PATCH;
    }
    else if (mod == 3) {
      return HttpRequest::HTTP_REQUEST_GET;
    }
    /*
    else if (mod == 4) {
      return HttpRequest::HTTP_REQUEST_DELETE;
    }
    */
    else {
      assert(false);
      return HttpRequest::HTTP_REQUEST_GET;
    }
  }

  const char* payload (size_t* length, const int threadNumber, const size_t threadCounter, const size_t globalCounter, bool* mustFree) {
    const size_t mod = globalCounter % 4;

    if (mod == 0 || mod == 2) {
      const size_t n = Complexity;
      TRI_string_buffer_t* buffer;

      buffer = TRI_CreateSizedStringBuffer(TRI_UNKNOWN_MEM_ZONE, 256);
      TRI_AppendStringStringBuffer(buffer, "{\"_key\":\"");

      size_t keyId = (size_t) (globalCounter / 4);
      const string key = "testkey" + StringUtils::itoa(keyId);
      TRI_AppendStringStringBuffer(buffer, key.c_str());
      TRI_AppendStringStringBuffer(buffer, "\"");

      for (size_t i = 1; i <= n; ++i) {
        TRI_AppendStringStringBuffer(buffer, ",\"value");
        TRI_AppendUInt32StringBuffer(buffer, (uint32_t) i);
        if (mod == 0) {
          TRI_AppendStringStringBuffer(buffer, "\":true");
        }
        else {
          TRI_AppendStringStringBuffer(buffer, "\":false");
        }
      }

      TRI_AppendCharStringBuffer(buffer, '}');

      *length = TRI_LengthStringBuffer(buffer);
      *mustFree = true;
      char* ptr = buffer->_buffer;
      buffer->_buffer = NULL;

      TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);

      return (const char*) ptr;
    }
    else if (mod == 1 || mod == 3 || mod == 4) {
      *length = 0;
      *mustFree = false;
      return (const char*) 0;
    }
    else {
      assert(false);
      return 0;
    }
  }

  const map<string, string>& headers () {
    static const map<string, string> headers;
    return headers;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                     skiplist test
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Benchmark
/// @{
////////////////////////////////////////////////////////////////////////////////

struct SkiplistTest : public BenchmarkOperation {
  SkiplistTest ()
    : BenchmarkOperation () {
  }

  ~SkiplistTest () {
  }

  bool setUp (SimpleClient* client) {
    return DeleteCollection(client, Collection) &&
           CreateCollection(client, Collection, 2) &&
           CreateIndex(client, Collection, "skiplist", "[\"value\"]");
  }

  void tearDown () {
  }

  string url (const int threadNumber, const size_t threadCounter, const size_t globalCounter) {
    const size_t mod = globalCounter % 4;

    if (mod == 0) {
      return string("/_api/document?collection=" + Collection);
    }
    else {
      size_t keyId = (size_t) (globalCounter / 4);
      const string key = "testkey" + StringUtils::itoa(keyId);

      return string("/_api/document/" + Collection + "/" + key);
    }
  }

  HttpRequest::HttpRequestType type (const int threadNumber, const size_t threadCounter, const size_t globalCounter) {
    const size_t mod = globalCounter % 4;

    if (mod == 0) {
      return HttpRequest::HTTP_REQUEST_POST;
    }
    else if (mod == 1) {
      return HttpRequest::HTTP_REQUEST_GET;
    }
    else if (mod == 2) {
      return HttpRequest::HTTP_REQUEST_PATCH;
    }
    else if (mod == 3) {
      return HttpRequest::HTTP_REQUEST_GET;
    }
    else {
      assert(false);
      return HttpRequest::HTTP_REQUEST_GET;
    }
  }

  const char* payload (size_t* length, const int threadNumber, const size_t threadCounter, const size_t globalCounter, bool* mustFree) {
    const size_t mod = globalCounter % 4;

    if (mod == 0 || mod == 2) {
      TRI_string_buffer_t* buffer;

      buffer = TRI_CreateSizedStringBuffer(TRI_UNKNOWN_MEM_ZONE, 256);
      TRI_AppendStringStringBuffer(buffer, "{\"_key\":\"");

      size_t keyId = (size_t) (globalCounter / 4);
      const string key = "testkey" + StringUtils::itoa(keyId);
      TRI_AppendStringStringBuffer(buffer, key.c_str());
      TRI_AppendStringStringBuffer(buffer, "\"");

      TRI_AppendStringStringBuffer(buffer, ",\"value\":");
      TRI_AppendUInt32StringBuffer(buffer, (uint32_t) threadCounter);
      TRI_AppendCharStringBuffer(buffer, '}');

      *length = TRI_LengthStringBuffer(buffer);
      *mustFree = true;
      char* ptr = buffer->_buffer;
      buffer->_buffer = NULL;

      TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);

      return (const char*) ptr;
    }
    else if (mod == 1 || mod == 3 || mod == 4) {
      *length = 0;
      *mustFree = false;
      return (const char*) 0;
    }
    else {
      assert(false);
      return 0;
    }
  }

  const map<string, string>& headers () {
    static const map<string, string> headers;
    return headers;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                         hash test
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Benchmark
/// @{
////////////////////////////////////////////////////////////////////////////////

struct HashTest : public BenchmarkOperation {
  HashTest ()
    : BenchmarkOperation () {
  }

  ~HashTest () {
  }

  bool setUp (SimpleClient* client) {
    return DeleteCollection(client, Collection) &&
           CreateCollection(client, Collection, 2) &&
           CreateIndex(client, Collection, "hash", "[\"value\"]");
  }

  void tearDown () {
  }

  string url (const int threadNumber, const size_t threadCounter, const size_t globalCounter) {
    const size_t mod = globalCounter % 4;

    if (mod == 0) {
      return string("/_api/document?collection=" + Collection);
    }
    else {
      size_t keyId = (size_t) (globalCounter / 4);
      const string key = "testkey" + StringUtils::itoa(keyId);

      return string("/_api/document/" + Collection + "/" + key);
    }
  }

  HttpRequest::HttpRequestType type (const int threadNumber, const size_t threadCounter, const size_t globalCounter) {
    const size_t mod = globalCounter % 4;

    if (mod == 0) {
      return HttpRequest::HTTP_REQUEST_POST;
    }
    else if (mod == 1) {
      return HttpRequest::HTTP_REQUEST_GET;
    }
    else if (mod == 2) {
      return HttpRequest::HTTP_REQUEST_PATCH;
    }
    else if (mod == 3) {
      return HttpRequest::HTTP_REQUEST_GET;
    }
    else {
      assert(false);
      return HttpRequest::HTTP_REQUEST_GET;
    }
  }

  const char* payload (size_t* length, const int threadNumber, const size_t threadCounter, const size_t globalCounter, bool* mustFree) {
    const size_t mod = globalCounter % 4;

    if (mod == 0 || mod == 2) {
      TRI_string_buffer_t* buffer;

      buffer = TRI_CreateSizedStringBuffer(TRI_UNKNOWN_MEM_ZONE, 256);
      TRI_AppendStringStringBuffer(buffer, "{\"_key\":\"");

      size_t keyId = (size_t) (globalCounter / 4);
      const string key = "testkey" + StringUtils::itoa(keyId);
      TRI_AppendStringStringBuffer(buffer, key.c_str());
      TRI_AppendStringStringBuffer(buffer, "\"");

      TRI_AppendStringStringBuffer(buffer, ",\"value\":");
      TRI_AppendUInt32StringBuffer(buffer, (uint32_t) threadCounter);
      TRI_AppendCharStringBuffer(buffer, '}');

      *length = TRI_LengthStringBuffer(buffer);
      *mustFree = true;
      char* ptr = buffer->_buffer;
      buffer->_buffer = NULL;

      TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);

      return (const char*) ptr;
    }
    else if (mod == 1 || mod == 3 || mod == 4) {
      *length = 0;
      *mustFree = false;
      return (const char*) 0;
    }
    else {
      assert(false);
      return 0;
    }
  }

  const map<string, string>& headers () {
    static const map<string, string> headers;
    return headers;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                            document creation test
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Benchmark
/// @{
////////////////////////////////////////////////////////////////////////////////

struct DocumentCreationTest : public BenchmarkOperation {
  DocumentCreationTest ()
    : BenchmarkOperation (),
      _url(),
      _buffer(0) {
    _url = "/_api/document?collection=" + Collection;

    const size_t n = Complexity;

    _buffer = TRI_CreateSizedStringBuffer(TRI_UNKNOWN_MEM_ZONE, 4096);
    TRI_AppendCharStringBuffer(_buffer, '{');

    for (size_t i = 1; i <= n; ++i) {
      TRI_AppendStringStringBuffer(_buffer, "\"test");
      TRI_AppendUInt32StringBuffer(_buffer, (uint32_t) i);
      TRI_AppendStringStringBuffer(_buffer, "\":\"some test value\"");
      if (i != n) {
        TRI_AppendCharStringBuffer(_buffer, ',');
      }
    }

    TRI_AppendCharStringBuffer(_buffer, '}');

    _length = TRI_LengthStringBuffer(_buffer);
  }

  ~DocumentCreationTest () {
    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, _buffer);
  }

  bool setUp (SimpleClient* client) {
    return DeleteCollection(client, Collection) &&
           CreateCollection(client, Collection, 2);
  }

  void tearDown () {
  }

  string url (const int threadNumber, const size_t threadCounter, const size_t globalCounter) {
    return _url;
  }

  HttpRequest::HttpRequestType type (const int threadNumber, const size_t threadCounter, const size_t globalCounter) {
    return HttpRequest::HTTP_REQUEST_POST;
  }

  const char* payload (size_t* length, const int threadNumber, const size_t threadCounter, const size_t globalCounter, bool* mustFree) {
    *mustFree = false;
    *length = _length;
    return (const char*) _buffer->_buffer;
  }

  const map<string, string>& headers () {
    static const map<string, string> headers;
    return headers;
  }

  string _url;

  TRI_string_buffer_t* _buffer;

  size_t _length;
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                            document creation test
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Benchmark
/// @{
////////////////////////////////////////////////////////////////////////////////

struct CollectionCreationTest : public BenchmarkOperation {
  CollectionCreationTest ()
    : BenchmarkOperation (),
      _url() {
    _url = "/_api/collection";

  }

  ~CollectionCreationTest () {
  }

  BenchmarkCounter<uint64_t>* getSharedCounter () {
    if (_counter == 0) {
      _counter = new BenchmarkCounter<uint64_t>(0, 1024 * 1024);
    }

    return _counter;
  }

  bool setUp (SimpleClient* client) {
    return true;
  }

  void tearDown () {
  }

  string url (const int threadNumber, const size_t threadCounter, const size_t globalCounter) {
    return _url;
  }

  HttpRequest::HttpRequestType type (const int threadNumber, const size_t threadCounter, const size_t globalCounter) {
    return HttpRequest::HTTP_REQUEST_POST;
  }

  const char* payload (size_t* length, const int threadNumber, const size_t threadCounter, const size_t globalCounter, bool* mustFree) {
    BenchmarkCounter<uint64_t>* ctr = getSharedCounter();
    TRI_string_buffer_t* buffer;
    char* data;

    ctr->next(1);
    buffer = TRI_CreateSizedStringBuffer(TRI_UNKNOWN_MEM_ZONE, 64);
    if (buffer == 0) {
      return 0;
    }
    TRI_AppendStringStringBuffer(buffer, "{\"name\":\"");
    TRI_AppendStringStringBuffer(buffer, Collection.c_str());
    TRI_AppendUInt64StringBuffer(buffer, ctr->getValue());
    TRI_AppendStringStringBuffer(buffer, "\"}");

    *length = TRI_LengthStringBuffer(buffer);

    // this will free the string buffer frame, but not the string
    data = buffer->_buffer;
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, buffer);

    *mustFree = true;
    return (const char*) data;
  }

  const map<string, string>& headers () {
    static const map<string, string> headers;
    return headers;
  }

  static BenchmarkCounter<uint64_t>* _counter;

  string _url;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                  transaction test
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Benchmark
/// @{
////////////////////////////////////////////////////////////////////////////////

struct TransactionAqlTest : public BenchmarkOperation {
  TransactionAqlTest ()
    : BenchmarkOperation () {
  }

  ~TransactionAqlTest () {
  }

  bool setUp (SimpleClient* client) {
    _c1 = string(Collection + "1");
    _c2 = string(Collection + "2");
    _c3 = string(Collection + "3");

    return DeleteCollection(client, _c1) &&
           DeleteCollection(client, _c2) &&
           DeleteCollection(client, _c3) &&
           CreateCollection(client, _c1, 2) &&
           CreateCollection(client, _c2, 2) &&
           CreateCollection(client, _c3, 2);
  }

  void tearDown () {
  }

  string url (const int threadNumber, const size_t threadCounter, const size_t globalCounter) {
    return string("/_api/cursor");
  }

  HttpRequest::HttpRequestType type (const int threadNumber, const size_t threadCounter, const size_t globalCounter) {
    return HttpRequest::HTTP_REQUEST_POST;
  }

  const char* payload (size_t* length, const int threadNumber, const size_t threadCounter, const size_t globalCounter, bool* mustFree) {
    const size_t mod = globalCounter % 8;
    TRI_string_buffer_t* buffer;
    buffer = TRI_CreateSizedStringBuffer(TRI_UNKNOWN_MEM_ZONE, 256);

    if (mod == 0) {
      TRI_AppendStringStringBuffer(buffer, "{\"query\":\"FOR c IN ");
      TRI_AppendStringStringBuffer(buffer, _c1.c_str());
      TRI_AppendStringStringBuffer(buffer, " RETURN 1\"}");
    }
    else if (mod == 1) {
      TRI_AppendStringStringBuffer(buffer, "{\"query\":\"FOR c IN ");
      TRI_AppendStringStringBuffer(buffer, _c2.c_str());
      TRI_AppendStringStringBuffer(buffer, " RETURN 1\"}");
    }
    else if (mod == 2) {
      TRI_AppendStringStringBuffer(buffer, "{\"query\":\"FOR c IN ");
      TRI_AppendStringStringBuffer(buffer, _c3.c_str());
      TRI_AppendStringStringBuffer(buffer, " RETURN 1\"}");
    }
    else if (mod == 3) {
      TRI_AppendStringStringBuffer(buffer, "{\"query\":\"FOR c1 IN ");
      TRI_AppendStringStringBuffer(buffer, _c1.c_str());
      TRI_AppendStringStringBuffer(buffer, " FOR c2 IN ");
      TRI_AppendStringStringBuffer(buffer, _c2.c_str());
      TRI_AppendStringStringBuffer(buffer, " RETURN 1\"}");
    }
    else if (mod == 4) {
      TRI_AppendStringStringBuffer(buffer, "{\"query\":\"FOR c2 IN ");
      TRI_AppendStringStringBuffer(buffer, _c2.c_str());
      TRI_AppendStringStringBuffer(buffer, " FOR c1 IN ");
      TRI_AppendStringStringBuffer(buffer, _c1.c_str());
      TRI_AppendStringStringBuffer(buffer, " RETURN 1\"}");
    }
    else if (mod == 5) {
      TRI_AppendStringStringBuffer(buffer, "{\"query\":\"FOR c3 IN ");
      TRI_AppendStringStringBuffer(buffer, _c3.c_str());
      TRI_AppendStringStringBuffer(buffer, " FOR c1 IN ");
      TRI_AppendStringStringBuffer(buffer, _c1.c_str());
      TRI_AppendStringStringBuffer(buffer, " RETURN 1\"}");
    }
    else if (mod == 6) {
      TRI_AppendStringStringBuffer(buffer, "{\"query\":\"FOR c2 IN ");
      TRI_AppendStringStringBuffer(buffer, _c2.c_str());
      TRI_AppendStringStringBuffer(buffer, " FOR c3 IN ");
      TRI_AppendStringStringBuffer(buffer, _c3.c_str());
      TRI_AppendStringStringBuffer(buffer, " RETURN 1\"}");
    }
    else if (mod == 7) {
      TRI_AppendStringStringBuffer(buffer, "{\"query\":\"FOR c1 IN ");
      TRI_AppendStringStringBuffer(buffer, _c1.c_str());
      TRI_AppendStringStringBuffer(buffer, " FOR c2 IN ");
      TRI_AppendStringStringBuffer(buffer, _c2.c_str());
      TRI_AppendStringStringBuffer(buffer, " FOR c3 IN ");
      TRI_AppendStringStringBuffer(buffer, _c3.c_str());
      TRI_AppendStringStringBuffer(buffer, " RETURN 1\"}");
    }

    *length = TRI_LengthStringBuffer(buffer);
    *mustFree = true;
    char* ptr = buffer->_buffer;
    buffer->_buffer = NULL;

    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);

    return (const char*) ptr;
  }

  const map<string, string>& headers () {
    static const map<string, string> headers;
    return headers;
  }

  string _c1;
  string _c2;
  string _c3;
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  transaction test
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Benchmark
/// @{
////////////////////////////////////////////////////////////////////////////////

struct TransactionCountTest : public BenchmarkOperation {
  TransactionCountTest ()
    : BenchmarkOperation () {
  }

  ~TransactionCountTest () {
  }

  bool setUp (SimpleClient* client) {
    return DeleteCollection(client, Collection) &&
           CreateCollection(client, Collection, 2);
  }

  void tearDown () {
  }

  string url (const int threadNumber, const size_t threadCounter, const size_t globalCounter) {
    return string("/_api/transaction");
  }

  HttpRequest::HttpRequestType type (const int threadNumber, const size_t threadCounter, const size_t globalCounter) {
    return HttpRequest::HTTP_REQUEST_POST;
  }

  const char* payload (size_t* length, const int threadNumber, const size_t threadCounter, const size_t globalCounter, bool* mustFree) {
    TRI_string_buffer_t* buffer;
    buffer = TRI_CreateSizedStringBuffer(TRI_UNKNOWN_MEM_ZONE, 256);

    TRI_AppendStringStringBuffer(buffer, "{ \"collections\": { \"write\": \"");
    TRI_AppendStringStringBuffer(buffer, Collection.c_str());
    TRI_AppendStringStringBuffer(buffer, "\" }, \"action\": \"function () { var c = require(\\\"internal\\\").db._collection(\\\""); 
    TRI_AppendStringStringBuffer(buffer, Collection.c_str());
    TRI_AppendStringStringBuffer(buffer, "\\\"); var startcount = c.count(); for (var i = 0; i < 50; ++i) { if (startcount + i !== c.count()) { throw \\\"error\\\"; } c.save({ }); } }\" }");

    *length = TRI_LengthStringBuffer(buffer);
    *mustFree = true;
    char* ptr = buffer->_buffer;
    buffer->_buffer = NULL;

    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);

    return (const char*) ptr;
  }

  const map<string, string>& headers () {
    static const map<string, string> headers;
    return headers;
  }

};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  transaction test
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Benchmark
/// @{
////////////////////////////////////////////////////////////////////////////////

struct TransactionMultiTest : public BenchmarkOperation {
  TransactionMultiTest ()
    : BenchmarkOperation () {
  }

  ~TransactionMultiTest () {
  }

  bool setUp (SimpleClient* client) {
    _c1 = string(Collection + "1");
    _c2 = string(Collection + "2");

    return DeleteCollection(client, _c1) &&
           DeleteCollection(client, _c2) &&
           CreateCollection(client, _c1, 2) &&
           CreateCollection(client, _c2, 2) && 
           CreateDocument(client, _c2, "{ \"_key\": \"sum\", \"count\": 0 }");
  }

  void tearDown () {
  }

  string url (const int threadNumber, const size_t threadCounter, const size_t globalCounter) {
    return string("/_api/transaction");
  }

  HttpRequest::HttpRequestType type (const int threadNumber, const size_t threadCounter, const size_t globalCounter) {
    return HttpRequest::HTTP_REQUEST_POST;
  }

  const char* payload (size_t* length, const int threadNumber, const size_t threadCounter, const size_t globalCounter, bool* mustFree) {
    const size_t mod = globalCounter % 2;
    TRI_string_buffer_t* buffer;
    buffer = TRI_CreateSizedStringBuffer(TRI_UNKNOWN_MEM_ZONE, 256);

    TRI_AppendStringStringBuffer(buffer, "{ \"collections\": { ");
    
    if (mod == 0) {
      TRI_AppendStringStringBuffer(buffer, "\"write\": [ \"");
    }
    else {
      TRI_AppendStringStringBuffer(buffer, "\"read\": [ \"");
    }

    TRI_AppendStringStringBuffer(buffer, _c1.c_str());
    TRI_AppendStringStringBuffer(buffer, "\", \"");
    TRI_AppendStringStringBuffer(buffer, _c2.c_str());
    TRI_AppendStringStringBuffer(buffer, "\" ] }, \"action\": \"function () { ");
    TRI_AppendStringStringBuffer(buffer, "var c1 = require(\\\"internal\\\").db._collection(\\\"");
    TRI_AppendStringStringBuffer(buffer, _c1.c_str());
    TRI_AppendStringStringBuffer(buffer, "\\\"); var c2 = require(\\\"internal\\\").db._collection(\\\"");
    TRI_AppendStringStringBuffer(buffer, _c2.c_str());
    TRI_AppendStringStringBuffer(buffer, "\\\"); ");
  
    if (mod == 0) {
      TRI_AppendStringStringBuffer(buffer, "var n = Math.floor(Math.random() * 25) + 1; c1.save({ count: n }); var d = c2.document(\\\"sum\\\"); c2.update(d, { count: d.count + n });");
    }
    else {
      TRI_AppendStringStringBuffer(buffer, "var r1 = 0; c1.toArray().forEach(function (d) { r1 += d.count }); var r2 = c2.document(\\\"sum\\\").count; if (r1 !== r2) { throw \\\"error\\\"; }");
    }
    
    TRI_AppendStringStringBuffer(buffer, " }\" }");

    *length = TRI_LengthStringBuffer(buffer);
    *mustFree = true;
    char* ptr = buffer->_buffer;
    buffer->_buffer = NULL;

    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);

    return (const char*) ptr;
  }

  const map<string, string>& headers () {
    static const map<string, string> headers;
    return headers;
  }

  string _c1;
  string _c2;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Benchmark
/// @{
////////////////////////////////////////////////////////////////////////////////

BenchmarkCounter<uint64_t>* CollectionCreationTest::_counter = 0;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Benchmark
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a collection
////////////////////////////////////////////////////////////////////////////////

static bool DeleteCollection (SimpleClient* client, const string& name) {
  map<string, string> headerFields;
  SimpleHttpResult* result = 0;

  result = client->request(HttpRequest::HTTP_REQUEST_DELETE,
                           "/_api/collection/" + name,
                           "",
                           0,
                           headerFields);

  bool failed = true;
  if (result != 0) {
    if (result->getHttpReturnCode() == 200 || result->getHttpReturnCode() == 404) {
      failed = false;
    }

    delete result;
  }

  return ! failed;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a collection
////////////////////////////////////////////////////////////////////////////////

static bool CreateCollection (SimpleClient* client,
                              const string& name,
                              const int type) {
  map<string, string> headerFields;
  SimpleHttpResult* result = 0;

  string payload = "{\"name\":\"" + name + "\",\"type\":" + StringUtils::itoa(type) + "}";
  result = client->request(HttpRequest::HTTP_REQUEST_POST,
                           "/_api/collection",
                           payload.c_str(),
                           payload.size(),
                           headerFields);

  bool failed = true;

  if (result != 0) {
    if (result->getHttpReturnCode() == 200 || result->getHttpReturnCode() == 201) {
      failed = false;
    }

    delete result;
  }

  return ! failed;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an index
////////////////////////////////////////////////////////////////////////////////

static bool CreateIndex (SimpleClient* client,
                         const string& name,
                         const string& type,
                         const string& fields) {
  map<string, string> headerFields;
  SimpleHttpResult* result = 0;

  string payload = "{\"type\":\"" + type + "\",\"fields\":" + fields + ",\"unique\":false}";
  result = client->request(HttpRequest::HTTP_REQUEST_POST,
                           "/_api/index?collection=" + name,
                           payload.c_str(),
                           payload.size(),
                           headerFields);

  bool failed = true;

  if (result != 0) {
    if (result->getHttpReturnCode() == 200 || result->getHttpReturnCode() == 201) {
      failed = false;
    }

    delete result;
  }

  return ! failed;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a document
////////////////////////////////////////////////////////////////////////////////

static bool CreateDocument (SimpleClient* client,
                            const string& collection,
                            const string& payload) {
  map<string, string> headerFields;
  SimpleHttpResult* result = 0;

  result = client->request(HttpRequest::HTTP_REQUEST_POST,
                           "/_api/document?collection=" + collection,
                           payload.c_str(),
                           payload.size(),
                           headerFields);

  bool failed = true;

  if (result != 0) {
    if (result->getHttpReturnCode() == 200 || 
        result->getHttpReturnCode() == 201 || 
        result->getHttpReturnCode() == 202) {
      failed = false;
    }

    delete result;
  }

  return ! failed;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the test case for a name
////////////////////////////////////////////////////////////////////////////////

static BenchmarkOperation* GetTestCase (const string& name) {
  if (name == "version") {
    return new VersionTest();
  }
  if (name == "document") {
    return new DocumentCreationTest();
  }
  if (name == "collection") {
    return new CollectionCreationTest();
  }
  if (name == "hash") {
    return new HashTest();
  }
  if (name == "skiplist") {
    return new SkiplistTest();
  }
  if (name == "edge") {
    return new EdgeCrudTest();
  }
  if (name == "crud") {
    return new DocumentCrudTest();
  }
  if (name == "crud-append") {
    return new DocumentCrudAppendTest();
  }
  if (name == "aqltrx") {
    return new TransactionAqlTest();
  }
  if (name == "counttrx") {
    return new TransactionCountTest();
  }
  if (name == "multitrx") {
    return new TransactionMultiTest();
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
