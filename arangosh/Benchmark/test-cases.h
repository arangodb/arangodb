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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

static bool DeleteCollection(SimpleHttpClient*, std::string const&);

static bool CreateCollection(SimpleHttpClient*, std::string const&, int const);

static bool CreateDocument(SimpleHttpClient*, std::string const&,
                           std::string const&);

static bool CreateIndex(SimpleHttpClient*, std::string const&,
                        std::string const&, std::string const&);

struct VersionTest : public BenchmarkOperation {
  VersionTest() : BenchmarkOperation() { _url = "/_api/version"; }

  ~VersionTest() {}

  bool setUp(SimpleHttpClient* client) { return true; }

  void tearDown() {}

  std::string url(int const threadNumber, size_t const threadCounter,
                  size_t const globalCounter) {
    return _url;
  }

  HttpRequest::HttpRequestType type(int const threadNumber,
                                    size_t const threadCounter,
                                    size_t const globalCounter) {
    return HttpRequest::HTTP_REQUEST_GET;
  }

  char const* payload(size_t* length, int const threadNumber,
                      size_t const threadCounter, size_t const globalCounter,
                      bool* mustFree) {
    static char const* payload = "";

    *mustFree = false;
    *length = 0;
    return payload;
  }

  std::string _url;
};

struct DocumentCrudAppendTest : public BenchmarkOperation {
  DocumentCrudAppendTest() : BenchmarkOperation() {}

  ~DocumentCrudAppendTest() {}

  bool setUp(SimpleHttpClient* client) {
    return DeleteCollection(client, Collection) &&
           CreateCollection(client, Collection, 2);
  }

  void tearDown() {}

  std::string url(int const threadNumber, size_t const threadCounter,
                  size_t const globalCounter) {
    size_t const mod = globalCounter % 4;

    if (mod == 0) {
      return std::string("/_api/document?collection=" + Collection);
    } else {
      size_t keyId = (size_t)(globalCounter / 4);
      std::string const key = "testkey" + StringUtils::itoa(keyId);

      return std::string("/_api/document/" + Collection + "/" + key);
    }
  }

  HttpRequest::HttpRequestType type(int const threadNumber,
                                    size_t const threadCounter,
                                    size_t const globalCounter) {
    size_t const mod = globalCounter % 4;

    if (mod == 0) {
      return HttpRequest::HTTP_REQUEST_POST;
    } else if (mod == 1) {
      return HttpRequest::HTTP_REQUEST_GET;
    } else if (mod == 2) {
      return HttpRequest::HTTP_REQUEST_PATCH;
    } else if (mod == 3) {
      return HttpRequest::HTTP_REQUEST_GET;
    } else {
      TRI_ASSERT(false);
      return HttpRequest::HTTP_REQUEST_GET;
    }
  }

  char const* payload(size_t* length, int const threadNumber,
                      size_t const threadCounter, size_t const globalCounter,
                      bool* mustFree) {
    size_t const mod = globalCounter % 4;

    if (mod == 0 || mod == 2) {
      uint64_t const n = Complexity;
      TRI_string_buffer_t* buffer;

      buffer = TRI_CreateSizedStringBuffer(TRI_UNKNOWN_MEM_ZONE, 256);
      TRI_AppendStringStringBuffer(buffer, "{\"_key\":\"");

      size_t keyId = (size_t)(globalCounter / 4);
      std::string const key = "testkey" + StringUtils::itoa(keyId);
      TRI_AppendStringStringBuffer(buffer, key.c_str());
      TRI_AppendStringStringBuffer(buffer, "\"");

      for (uint64_t i = 1; i <= n; ++i) {
        TRI_AppendStringStringBuffer(buffer, ",\"value");
        TRI_AppendUInt64StringBuffer(buffer, i);
        if (mod == 0) {
          TRI_AppendStringStringBuffer(buffer, "\":true");
        } else {
          TRI_AppendStringStringBuffer(buffer, "\":false");
        }
      }

      TRI_AppendCharStringBuffer(buffer, '}');

      *length = TRI_LengthStringBuffer(buffer);
      *mustFree = true;
      char* ptr = TRI_StealStringBuffer(buffer);
      TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);

      return (char const*)ptr;
    } else if (mod == 1 || mod == 3) {
      *length = 0;
      *mustFree = false;
      return (char const*)nullptr;
    } else {
      TRI_ASSERT(false);
      return 0;
    }
  }
};

struct DocumentCrudWriteReadTest : public BenchmarkOperation {
  DocumentCrudWriteReadTest() : BenchmarkOperation() {}

  ~DocumentCrudWriteReadTest() {}

  bool setUp(SimpleHttpClient* client) {
    return DeleteCollection(client, Collection) &&
           CreateCollection(client, Collection, 2);
  }

  void tearDown() {}

  std::string url(int const threadNumber, size_t const threadCounter,
                  size_t const globalCounter) {
    size_t const mod = globalCounter % 2;

    if (mod == 0) {
      return std::string("/_api/document?collection=" + Collection);
    } else {
      size_t keyId = (size_t)(globalCounter / 2);
      std::string const key = "testkey" + StringUtils::itoa(keyId);

      return std::string("/_api/document/" + Collection + "/" + key);
    }
  }

  HttpRequest::HttpRequestType type(int const threadNumber,
                                    size_t const threadCounter,
                                    size_t const globalCounter) {
    size_t const mod = globalCounter % 2;

    if (mod == 0) {
      return HttpRequest::HTTP_REQUEST_POST;
    } else {
      return HttpRequest::HTTP_REQUEST_GET;
    }
  }

  char const* payload(size_t* length, int const threadNumber,
                      size_t const threadCounter, size_t const globalCounter,
                      bool* mustFree) {
    size_t const mod = globalCounter % 2;

    if (mod == 0) {
      uint64_t const n = Complexity;
      TRI_string_buffer_t* buffer;

      buffer = TRI_CreateSizedStringBuffer(TRI_UNKNOWN_MEM_ZONE, 256);
      TRI_AppendStringStringBuffer(buffer, "{\"_key\":\"");

      size_t keyId = (size_t)(globalCounter / 2);
      std::string const key = "testkey" + StringUtils::itoa(keyId);
      TRI_AppendStringStringBuffer(buffer, key.c_str());
      TRI_AppendStringStringBuffer(buffer, "\"");

      for (uint64_t i = 1; i <= n; ++i) {
        TRI_AppendStringStringBuffer(buffer, ",\"value");
        TRI_AppendUInt64StringBuffer(buffer, i);
        if (mod == 0) {
          TRI_AppendStringStringBuffer(buffer, "\":true");
        } else {
          TRI_AppendStringStringBuffer(buffer, "\":false");
        }
      }

      TRI_AppendCharStringBuffer(buffer, '}');

      *length = TRI_LengthStringBuffer(buffer);
      *mustFree = true;
      char* ptr = TRI_StealStringBuffer(buffer);
      TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);

      return (char const*)ptr;
    } else {
      *length = 0;
      *mustFree = false;
      return (char const*)nullptr;
    }
  }
};

struct ShapesTest : public BenchmarkOperation {
  ShapesTest() : BenchmarkOperation() {}

  ~ShapesTest() {}

  bool setUp(SimpleHttpClient* client) {
    return DeleteCollection(client, Collection) &&
           CreateCollection(client, Collection, 2);
  }

  void tearDown() {}

  std::string url(int const threadNumber, size_t const threadCounter,
                  size_t const globalCounter) {
    size_t const mod = globalCounter % 3;

    if (mod == 0) {
      return std::string("/_api/document?collection=" + Collection);
    } else {
      size_t keyId = (size_t)(globalCounter / 3);
      std::string const key = "testkey" + StringUtils::itoa(keyId);

      return std::string("/_api/document/" + Collection + "/" + key);
    }
  }

  HttpRequest::HttpRequestType type(int const threadNumber,
                                    size_t const threadCounter,
                                    size_t const globalCounter) {
    size_t const mod = globalCounter % 3;

    if (mod == 0) {
      return HttpRequest::HTTP_REQUEST_POST;
    } else if (mod == 1) {
      return HttpRequest::HTTP_REQUEST_GET;
    } else {
      return HttpRequest::HTTP_REQUEST_DELETE;
    }
  }

  char const* payload(size_t* length, int const threadNumber,
                      size_t const threadCounter, size_t const globalCounter,
                      bool* mustFree) {
    size_t const mod = globalCounter % 3;

    if (mod == 0) {
      uint64_t const n = Complexity;
      TRI_string_buffer_t* buffer;

      buffer = TRI_CreateSizedStringBuffer(TRI_UNKNOWN_MEM_ZONE, 256);
      TRI_AppendStringStringBuffer(buffer, "{\"_key\":\"");

      size_t keyId = (size_t)(globalCounter / 3);
      std::string const key = "testkey" + StringUtils::itoa(keyId);
      TRI_AppendString2StringBuffer(buffer, key.c_str(), key.size());
      TRI_AppendStringStringBuffer(buffer, "\"");

      for (uint64_t i = 1; i <= n; ++i) {
        uint64_t mod = Operations / 10;
        if (mod < 100) {
          mod = 100;
        }
        TRI_AppendStringStringBuffer(buffer, ",\"value");
        TRI_AppendUInt64StringBuffer(buffer,
                                     (uint64_t)((globalCounter + i) % mod));
        TRI_AppendStringStringBuffer(
            buffer,
            "\":\"some bogus string value to fill up the datafile...\"");
      }

      TRI_AppendStringStringBuffer(buffer, "}");

      *length = TRI_LengthStringBuffer(buffer);
      *mustFree = true;
      char* ptr = TRI_StealStringBuffer(buffer);
      TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);

      return (char const*)ptr;
    } else {
      *length = 0;
      *mustFree = false;
      return (char const*)nullptr;
    }
  }
};

struct ShapesAppendTest : public BenchmarkOperation {
  ShapesAppendTest() : BenchmarkOperation() {}

  ~ShapesAppendTest() {}

  bool setUp(SimpleHttpClient* client) {
    return DeleteCollection(client, Collection) &&
           CreateCollection(client, Collection, 2);
  }

  void tearDown() {}

  std::string url(int const threadNumber, size_t const threadCounter,
                  size_t const globalCounter) {
    size_t const mod = globalCounter % 2;

    if (mod == 0) {
      return std::string("/_api/document?collection=" + Collection);
    } else {
      size_t keyId = (size_t)(globalCounter / 2);
      std::string const key = "testkey" + StringUtils::itoa(keyId);

      return std::string("/_api/document/" + Collection + "/" + key);
    }
  }

  HttpRequest::HttpRequestType type(int const threadNumber,
                                    size_t const threadCounter,
                                    size_t const globalCounter) {
    size_t const mod = globalCounter % 2;

    if (mod == 0) {
      return HttpRequest::HTTP_REQUEST_POST;
    }
    return HttpRequest::HTTP_REQUEST_GET;
  }

  char const* payload(size_t* length, int const threadNumber,
                      size_t const threadCounter, size_t const globalCounter,
                      bool* mustFree) {
    size_t const mod = globalCounter % 2;

    if (mod == 0) {
      uint64_t const n = Complexity;
      TRI_string_buffer_t* buffer;

      buffer = TRI_CreateSizedStringBuffer(TRI_UNKNOWN_MEM_ZONE, 256);
      TRI_AppendStringStringBuffer(buffer, "{\"_key\":\"");

      size_t keyId = (size_t)(globalCounter / 2);
      std::string const key = "testkey" + StringUtils::itoa(keyId);
      TRI_AppendString2StringBuffer(buffer, key.c_str(), key.size());
      TRI_AppendStringStringBuffer(buffer, "\"");

      for (uint64_t i = 1; i <= n; ++i) {
        uint64_t mod = Operations / 10;
        if (mod < 100) {
          mod = 100;
        }
        TRI_AppendStringStringBuffer(buffer, ",\"value");
        TRI_AppendUInt64StringBuffer(buffer,
                                     (uint64_t)((globalCounter + i) % mod));
        TRI_AppendStringStringBuffer(
            buffer,
            "\":\"some bogus string value to fill up the datafile...\"");
      }

      TRI_AppendStringStringBuffer(buffer, "}");

      *length = TRI_LengthStringBuffer(buffer);
      *mustFree = true;
      char* ptr = TRI_StealStringBuffer(buffer);
      TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);

      return (char const*)ptr;
    } else {
      *length = 0;
      *mustFree = false;
      return (char const*)nullptr;
    }
  }
};

struct RandomShapesTest : public BenchmarkOperation {
  RandomShapesTest() : BenchmarkOperation() {
    _randomValue = TRI_UInt32Random();
  }

  ~RandomShapesTest() {}

  bool setUp(SimpleHttpClient* client) {
    return DeleteCollection(client, Collection) &&
           CreateCollection(client, Collection, 2);
  }

  void tearDown() {}

  std::string url(int const threadNumber, size_t const threadCounter,
                  size_t const globalCounter) {
    size_t const mod = globalCounter % 3;

    if (mod == 0) {
      return std::string("/_api/document?collection=") + Collection;
    } else {
      size_t keyId = (size_t)(globalCounter / 3);
      std::string const key = "testkey" + StringUtils::itoa(keyId);

      return std::string("/_api/document/") + Collection + std::string("/") +
             key;
    }
  }

  HttpRequest::HttpRequestType type(int const threadNumber,
                                    size_t const threadCounter,
                                    size_t const globalCounter) {
    size_t const mod = globalCounter % 3;

    if (mod == 0) {
      return HttpRequest::HTTP_REQUEST_POST;
    } else if (mod == 1) {
      return HttpRequest::HTTP_REQUEST_GET;
    } else {
      return HttpRequest::HTTP_REQUEST_DELETE;
    }
  }

  char const* payload(size_t* length, int const threadNumber,
                      size_t const threadCounter, size_t const globalCounter,
                      bool* mustFree) {
    size_t const mod = globalCounter % 3;

    if (mod == 0) {
      uint64_t const n = Complexity;
      TRI_string_buffer_t* buffer;

      buffer = TRI_CreateSizedStringBuffer(TRI_UNKNOWN_MEM_ZONE, 256);
      TRI_AppendStringStringBuffer(buffer, "{\"_key\":\"");

      size_t keyId = (size_t)(globalCounter / 3);
      std::string const key = "testkey" + StringUtils::itoa(keyId);
      TRI_AppendString2StringBuffer(buffer, key.c_str(), key.size());
      TRI_AppendStringStringBuffer(buffer, "\"");

      uint32_t const t =
          _randomValue % (uint32_t)(globalCounter + threadNumber + 1);

      for (uint64_t i = 1; i <= n; ++i) {
        TRI_AppendStringStringBuffer(buffer, ",\"value");
        TRI_AppendUInt64StringBuffer(buffer, (uint64_t)(globalCounter + i));
        if (t % 3 == 0) {
          TRI_AppendStringStringBuffer(buffer, "\":true");
        } else if (t % 3 == 1) {
          TRI_AppendStringStringBuffer(buffer, "\":null");
        } else
          TRI_AppendStringStringBuffer(
              buffer,
              "\":\"some bogus string value to fill up the datafile...\"");
      }

      TRI_AppendStringStringBuffer(buffer, "}");

      *length = TRI_LengthStringBuffer(buffer);
      *mustFree = true;
      char* ptr = TRI_StealStringBuffer(buffer);
      TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);

      return (char const*)ptr;
    } else {
      *length = 0;
      *mustFree = false;
      return (char const*)nullptr;
    }
  }

  uint32_t _randomValue;
};

struct DocumentCrudTest : public BenchmarkOperation {
  DocumentCrudTest() : BenchmarkOperation() {}

  ~DocumentCrudTest() {}

  bool setUp(SimpleHttpClient* client) {
    return DeleteCollection(client, Collection) &&
           CreateCollection(client, Collection, 2);
  }

  void tearDown() {}

  std::string url(int const threadNumber, size_t const threadCounter,
                  size_t const globalCounter) {
    size_t const mod = globalCounter % 5;

    if (mod == 0) {
      return std::string("/_api/document?collection=" + Collection);
    } else {
      size_t keyId = (size_t)(globalCounter / 5);
      std::string const key = "testkey" + StringUtils::itoa(keyId);

      return std::string("/_api/document/" + Collection + "/" + key);
    }
  }

  HttpRequest::HttpRequestType type(int const threadNumber,
                                    size_t const threadCounter,
                                    size_t const globalCounter) {
    size_t const mod = globalCounter % 5;

    if (mod == 0) {
      return HttpRequest::HTTP_REQUEST_POST;
    } else if (mod == 1) {
      return HttpRequest::HTTP_REQUEST_GET;
    } else if (mod == 2) {
      return HttpRequest::HTTP_REQUEST_PATCH;
    } else if (mod == 3) {
      return HttpRequest::HTTP_REQUEST_GET;
    } else if (mod == 4) {
      return HttpRequest::HTTP_REQUEST_DELETE;
    } else {
      TRI_ASSERT(false);
      return HttpRequest::HTTP_REQUEST_GET;
    }
  }

  char const* payload(size_t* length, int const threadNumber,
                      size_t const threadCounter, size_t const globalCounter,
                      bool* mustFree) {
    size_t const mod = globalCounter % 5;

    if (mod == 0 || mod == 2) {
      uint64_t const n = Complexity;
      TRI_string_buffer_t* buffer;

      buffer = TRI_CreateSizedStringBuffer(TRI_UNKNOWN_MEM_ZONE, 256);
      TRI_AppendStringStringBuffer(buffer, "{\"_key\":\"");

      size_t keyId = (size_t)(globalCounter / 5);
      std::string const key = "testkey" + StringUtils::itoa(keyId);
      TRI_AppendStringStringBuffer(buffer, key.c_str());
      TRI_AppendStringStringBuffer(buffer, "\"");

      for (uint64_t i = 1; i <= n; ++i) {
        TRI_AppendStringStringBuffer(buffer, ",\"value");
        TRI_AppendUInt64StringBuffer(buffer, i);
        if (mod == 0) {
          TRI_AppendStringStringBuffer(buffer, "\":true");
        } else {
          TRI_AppendStringStringBuffer(buffer, "\":false");
        }
      }

      TRI_AppendCharStringBuffer(buffer, '}');

      *length = TRI_LengthStringBuffer(buffer);
      *mustFree = true;
      char* ptr = TRI_StealStringBuffer(buffer);
      TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);

      return (char const*)ptr;
    } else if (mod == 1 || mod == 3 || mod == 4) {
      *length = 0;
      *mustFree = false;
      return (char const*)nullptr;
    } else {
      TRI_ASSERT(false);
      return 0;
    }
  }
};

struct EdgeCrudTest : public BenchmarkOperation {
  EdgeCrudTest() : BenchmarkOperation() {}

  ~EdgeCrudTest() {}

  bool setUp(SimpleHttpClient* client) {
    return DeleteCollection(client, Collection) &&
           CreateCollection(client, Collection, 3);
  }

  void tearDown() {}

  std::string url(int const threadNumber, size_t const threadCounter,
                  size_t const globalCounter) {
    size_t const mod = globalCounter % 4;

    if (mod == 0) {
      return std::string("/_api/edge?collection=" + Collection + "&from=" +
                         Collection.c_str() + "%2Ftestfrom" +
                         StringUtils::itoa(globalCounter) + "&to=" +
                         Collection.c_str() + "%2Ftestto" +
                         StringUtils::itoa(globalCounter));
    } else {
      size_t keyId = (size_t)(globalCounter / 4);
      std::string const key = "testkey" + StringUtils::itoa(keyId);

      return std::string("/_api/edge/" + Collection + "/" + key);
    }
  }

  HttpRequest::HttpRequestType type(int const threadNumber,
                                    size_t const threadCounter,
                                    size_t const globalCounter) {
    size_t const mod = globalCounter % 4;

    if (mod == 0) {
      return HttpRequest::HTTP_REQUEST_POST;
    } else if (mod == 1) {
      return HttpRequest::HTTP_REQUEST_GET;
    } else if (mod == 2) {
      return HttpRequest::HTTP_REQUEST_PATCH;
    } else if (mod == 3) {
      return HttpRequest::HTTP_REQUEST_GET;
    }
    /*
    else if (mod == 4) {
      return HttpRequest::HTTP_REQUEST_DELETE;
    }
    */
    else {
      TRI_ASSERT(false);
      return HttpRequest::HTTP_REQUEST_GET;
    }
  }

  char const* payload(size_t* length, int const threadNumber,
                      size_t const threadCounter, size_t const globalCounter,
                      bool* mustFree) {
    size_t const mod = globalCounter % 4;

    if (mod == 0 || mod == 2) {
      uint64_t const n = Complexity;
      TRI_string_buffer_t* buffer;

      buffer = TRI_CreateSizedStringBuffer(TRI_UNKNOWN_MEM_ZONE, 256);
      TRI_AppendStringStringBuffer(buffer, "{\"_key\":\"");

      size_t keyId = (size_t)(globalCounter / 4);
      std::string const key = "testkey" + StringUtils::itoa(keyId);
      TRI_AppendStringStringBuffer(buffer, key.c_str());
      TRI_AppendStringStringBuffer(buffer, "\"");

      for (uint64_t i = 1; i <= n; ++i) {
        TRI_AppendStringStringBuffer(buffer, ",\"value");
        TRI_AppendUInt64StringBuffer(buffer, i);
        if (mod == 0) {
          TRI_AppendStringStringBuffer(buffer, "\":true");
        } else {
          TRI_AppendStringStringBuffer(buffer, "\":false");
        }
      }

      TRI_AppendCharStringBuffer(buffer, '}');

      *length = TRI_LengthStringBuffer(buffer);
      *mustFree = true;
      char* ptr = TRI_StealStringBuffer(buffer);
      TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);

      return (char const*)ptr;
    } else if (mod == 1 || mod == 3 || mod == 4) {
      *length = 0;
      *mustFree = false;
      return (char const*)nullptr;
    } else {
      TRI_ASSERT(false);
      return 0;
    }
  }
};

struct SkiplistTest : public BenchmarkOperation {
  SkiplistTest() : BenchmarkOperation() {}

  ~SkiplistTest() {}

  bool setUp(SimpleHttpClient* client) {
    return DeleteCollection(client, Collection) &&
           CreateCollection(client, Collection, 2) &&
           CreateIndex(client, Collection, "skiplist", "[\"value\"]");
  }

  void tearDown() {}

  std::string url(int const threadNumber, size_t const threadCounter,
                  size_t const globalCounter) {
    size_t const mod = globalCounter % 4;

    if (mod == 0) {
      return std::string("/_api/document?collection=" + Collection);
    } else {
      size_t keyId = (size_t)(globalCounter / 4);
      std::string const key = "testkey" + StringUtils::itoa(keyId);

      return std::string("/_api/document/" + Collection + "/" + key);
    }
  }

  HttpRequest::HttpRequestType type(int const threadNumber,
                                    size_t const threadCounter,
                                    size_t const globalCounter) {
    size_t const mod = globalCounter % 4;

    if (mod == 0) {
      return HttpRequest::HTTP_REQUEST_POST;
    } else if (mod == 1) {
      return HttpRequest::HTTP_REQUEST_GET;
    } else if (mod == 2) {
      return HttpRequest::HTTP_REQUEST_PATCH;
    } else if (mod == 3) {
      return HttpRequest::HTTP_REQUEST_GET;
    } else {
      TRI_ASSERT(false);
      return HttpRequest::HTTP_REQUEST_GET;
    }
  }

  char const* payload(size_t* length, int const threadNumber,
                      size_t const threadCounter, size_t const globalCounter,
                      bool* mustFree) {
    size_t const mod = globalCounter % 4;

    if (mod == 0 || mod == 2) {
      TRI_string_buffer_t* buffer;

      buffer = TRI_CreateSizedStringBuffer(TRI_UNKNOWN_MEM_ZONE, 256);
      TRI_AppendStringStringBuffer(buffer, "{\"_key\":\"");

      size_t keyId = (size_t)(globalCounter / 4);
      std::string const key = "testkey" + StringUtils::itoa(keyId);
      TRI_AppendStringStringBuffer(buffer, key.c_str());
      TRI_AppendStringStringBuffer(buffer, "\"");

      TRI_AppendStringStringBuffer(buffer, ",\"value\":");
      TRI_AppendUInt32StringBuffer(buffer, (uint32_t)threadCounter);
      TRI_AppendCharStringBuffer(buffer, '}');

      *length = TRI_LengthStringBuffer(buffer);
      *mustFree = true;
      char* ptr = TRI_StealStringBuffer(buffer);
      TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);

      return (char const*)ptr;
    } else if (mod == 1 || mod == 3 || mod == 4) {
      *length = 0;
      *mustFree = false;
      return (char const*)nullptr;
    } else {
      TRI_ASSERT(false);
      return 0;
    }
  }
};

struct HashTest : public BenchmarkOperation {
  HashTest() : BenchmarkOperation() {}

  ~HashTest() {}

  bool setUp(SimpleHttpClient* client) {
    return DeleteCollection(client, Collection) &&
           CreateCollection(client, Collection, 2) &&
           CreateIndex(client, Collection, "hash", "[\"value\"]");
  }

  void tearDown() {}

  std::string url(int const threadNumber, size_t const threadCounter,
                  size_t const globalCounter) {
    size_t const mod = globalCounter % 4;

    if (mod == 0) {
      return std::string("/_api/document?collection=" + Collection);
    } else {
      size_t keyId = (size_t)(globalCounter / 4);
      std::string const key = "testkey" + StringUtils::itoa(keyId);

      return std::string("/_api/document/" + Collection + "/" + key);
    }
  }

  HttpRequest::HttpRequestType type(int const threadNumber,
                                    size_t const threadCounter,
                                    size_t const globalCounter) {
    size_t const mod = globalCounter % 4;

    if (mod == 0) {
      return HttpRequest::HTTP_REQUEST_POST;
    } else if (mod == 1) {
      return HttpRequest::HTTP_REQUEST_GET;
    } else if (mod == 2) {
      return HttpRequest::HTTP_REQUEST_PATCH;
    } else if (mod == 3) {
      return HttpRequest::HTTP_REQUEST_GET;
    } else {
      TRI_ASSERT(false);
      return HttpRequest::HTTP_REQUEST_GET;
    }
  }

  char const* payload(size_t* length, int const threadNumber,
                      size_t const threadCounter, size_t const globalCounter,
                      bool* mustFree) {
    size_t const mod = globalCounter % 4;

    if (mod == 0 || mod == 2) {
      TRI_string_buffer_t* buffer;

      buffer = TRI_CreateSizedStringBuffer(TRI_UNKNOWN_MEM_ZONE, 256);
      TRI_AppendStringStringBuffer(buffer, "{\"_key\":\"");

      size_t keyId = (size_t)(globalCounter / 4);
      std::string const key = "testkey" + StringUtils::itoa(keyId);
      TRI_AppendStringStringBuffer(buffer, key.c_str());
      TRI_AppendStringStringBuffer(buffer, "\"");

      TRI_AppendStringStringBuffer(buffer, ",\"value\":");
      TRI_AppendUInt32StringBuffer(buffer, (uint32_t)threadCounter);
      TRI_AppendCharStringBuffer(buffer, '}');

      *length = TRI_LengthStringBuffer(buffer);
      *mustFree = true;
      char* ptr = TRI_StealStringBuffer(buffer);
      TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);

      return (char const*)ptr;
    } else if (mod == 1 || mod == 3 || mod == 4) {
      *length = 0;
      *mustFree = false;
      return (char const*)nullptr;
    } else {
      TRI_ASSERT(false);
      return 0;
    }
  }
};

struct DocumentImportTest : public BenchmarkOperation {
  DocumentImportTest() : BenchmarkOperation(), _url(), _buffer(0) {
    _url = "/_api/import?collection=" + Collection + "&type=documents";

    uint64_t const n = Complexity;

    _buffer = TRI_CreateSizedStringBuffer(TRI_UNKNOWN_MEM_ZONE, 16384);
    for (uint64_t i = 0; i < n; ++i) {
      TRI_AppendStringStringBuffer(_buffer, "{\"key1\":\"");
      TRI_AppendUInt64StringBuffer(_buffer, i);
      TRI_AppendStringStringBuffer(_buffer, "\",\"key2\":");
      TRI_AppendUInt64StringBuffer(_buffer, i);
      TRI_AppendStringStringBuffer(_buffer, "}\n");
    }

    _length = TRI_LengthStringBuffer(_buffer);
  }

  ~DocumentImportTest() { TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, _buffer); }

  bool setUp(SimpleHttpClient* client) {
    return DeleteCollection(client, Collection) &&
           CreateCollection(client, Collection, 2);
  }

  void tearDown() {}

  std::string url(int const threadNumber, size_t const threadCounter,
                  size_t const globalCounter) {
    return _url;
  }

  HttpRequest::HttpRequestType type(int const threadNumber,
                                    size_t const threadCounter,
                                    size_t const globalCounter) {
    return HttpRequest::HTTP_REQUEST_POST;
  }

  char const* payload(size_t* length, int const threadNumber,
                      size_t const threadCounter, size_t const globalCounter,
                      bool* mustFree) {
    *mustFree = false;
    *length = _length;
    return (char const*)_buffer->_buffer;
  }

  std::string _url;

  TRI_string_buffer_t* _buffer;

  size_t _length;
};

struct DocumentCreationTest : public BenchmarkOperation {
  DocumentCreationTest() : BenchmarkOperation(), _url(), _buffer(0) {
    _url = "/_api/document?collection=" + Collection;

    uint64_t const n = Complexity;

    _buffer = TRI_CreateSizedStringBuffer(TRI_UNKNOWN_MEM_ZONE, 4096);
    TRI_AppendCharStringBuffer(_buffer, '{');

    for (uint64_t i = 1; i <= n; ++i) {
      TRI_AppendStringStringBuffer(_buffer, "\"test");
      TRI_AppendUInt64StringBuffer(_buffer, i);
      TRI_AppendStringStringBuffer(_buffer, "\":\"some test value\"");
      if (i != n) {
        TRI_AppendCharStringBuffer(_buffer, ',');
      }
    }

    TRI_AppendCharStringBuffer(_buffer, '}');

    _length = TRI_LengthStringBuffer(_buffer);
  }

  ~DocumentCreationTest() {
    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, _buffer);
  }

  bool setUp(SimpleHttpClient* client) {
    return DeleteCollection(client, Collection) &&
           CreateCollection(client, Collection, 2);
  }

  void tearDown() {}

  std::string url(int const threadNumber, size_t const threadCounter,
                  size_t const globalCounter) {
    return _url;
  }

  HttpRequest::HttpRequestType type(int const threadNumber,
                                    size_t const threadCounter,
                                    size_t const globalCounter) {
    return HttpRequest::HTTP_REQUEST_POST;
  }

  char const* payload(size_t* length, int const threadNumber,
                      size_t const threadCounter, size_t const globalCounter,
                      bool* mustFree) {
    *mustFree = false;
    *length = _length;
    return (char const*)_buffer->_buffer;
  }

  std::string _url;

  TRI_string_buffer_t* _buffer;

  size_t _length;
};

struct CollectionCreationTest : public BenchmarkOperation {
  CollectionCreationTest() : BenchmarkOperation(), _url() {
    _url = "/_api/collection";
  }

  ~CollectionCreationTest() {}

  bool setUp(SimpleHttpClient* client) { return true; }

  void tearDown() {}

  std::string url(int const threadNumber, size_t const threadCounter,
                  size_t const globalCounter) {
    return _url;
  }

  HttpRequest::HttpRequestType type(int const threadNumber,
                                    size_t const threadCounter,
                                    size_t const globalCounter) {
    return HttpRequest::HTTP_REQUEST_POST;
  }

  char const* payload(size_t* length, int const threadNumber,
                      size_t const threadCounter, size_t const globalCounter,
                      bool* mustFree) {
    TRI_string_buffer_t* buffer;
    char* data;

    buffer = TRI_CreateSizedStringBuffer(TRI_UNKNOWN_MEM_ZONE, 64);
    if (buffer == 0) {
      return 0;
    }
    TRI_AppendStringStringBuffer(buffer, "{\"name\":\"");
    TRI_AppendStringStringBuffer(buffer, Collection.c_str());
    TRI_AppendUInt64StringBuffer(buffer, ++_counter);
    TRI_AppendStringStringBuffer(buffer, "\"}");

    *length = TRI_LengthStringBuffer(buffer);

    // this will free the string buffer frame, but not the string
    data = TRI_StealStringBuffer(buffer);
    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);

    *mustFree = true;
    return (char const*)data;
  }

  static std::atomic<uint64_t> _counter;

  std::string _url;
};

std::atomic<uint64_t> CollectionCreationTest::_counter(0);

struct TransactionAqlTest : public BenchmarkOperation {
  TransactionAqlTest() : BenchmarkOperation() {}

  ~TransactionAqlTest() {}

  bool setUp(SimpleHttpClient* client) {
    _c1 = std::string(Collection + "1");
    _c2 = std::string(Collection + "2");
    _c3 = std::string(Collection + "3");

    return DeleteCollection(client, _c1) && DeleteCollection(client, _c2) &&
           DeleteCollection(client, _c3) && CreateCollection(client, _c1, 2) &&
           CreateCollection(client, _c2, 2) && CreateCollection(client, _c3, 2);
  }

  void tearDown() {}

  std::string url(int const threadNumber, size_t const threadCounter,
                  size_t const globalCounter) {
    return std::string("/_api/cursor");
  }

  HttpRequest::HttpRequestType type(int const threadNumber,
                                    size_t const threadCounter,
                                    size_t const globalCounter) {
    return HttpRequest::HTTP_REQUEST_POST;
  }

  char const* payload(size_t* length, int const threadNumber,
                      size_t const threadCounter, size_t const globalCounter,
                      bool* mustFree) {
    size_t const mod = globalCounter % 8;
    TRI_string_buffer_t* buffer;
    buffer = TRI_CreateSizedStringBuffer(TRI_UNKNOWN_MEM_ZONE, 256);

    if (mod == 0) {
      TRI_AppendStringStringBuffer(buffer, "{\"query\":\"FOR c IN ");
      TRI_AppendStringStringBuffer(buffer, _c1.c_str());
      TRI_AppendStringStringBuffer(buffer, " RETURN 1\"}");
    } else if (mod == 1) {
      TRI_AppendStringStringBuffer(buffer, "{\"query\":\"FOR c IN ");
      TRI_AppendStringStringBuffer(buffer, _c2.c_str());
      TRI_AppendStringStringBuffer(buffer, " RETURN 1\"}");
    } else if (mod == 2) {
      TRI_AppendStringStringBuffer(buffer, "{\"query\":\"FOR c IN ");
      TRI_AppendStringStringBuffer(buffer, _c3.c_str());
      TRI_AppendStringStringBuffer(buffer, " RETURN 1\"}");
    } else if (mod == 3) {
      TRI_AppendStringStringBuffer(buffer, "{\"query\":\"FOR c1 IN ");
      TRI_AppendStringStringBuffer(buffer, _c1.c_str());
      TRI_AppendStringStringBuffer(buffer, " FOR c2 IN ");
      TRI_AppendStringStringBuffer(buffer, _c2.c_str());
      TRI_AppendStringStringBuffer(buffer, " RETURN 1\"}");
    } else if (mod == 4) {
      TRI_AppendStringStringBuffer(buffer, "{\"query\":\"FOR c2 IN ");
      TRI_AppendStringStringBuffer(buffer, _c2.c_str());
      TRI_AppendStringStringBuffer(buffer, " FOR c1 IN ");
      TRI_AppendStringStringBuffer(buffer, _c1.c_str());
      TRI_AppendStringStringBuffer(buffer, " RETURN 1\"}");
    } else if (mod == 5) {
      TRI_AppendStringStringBuffer(buffer, "{\"query\":\"FOR c3 IN ");
      TRI_AppendStringStringBuffer(buffer, _c3.c_str());
      TRI_AppendStringStringBuffer(buffer, " FOR c1 IN ");
      TRI_AppendStringStringBuffer(buffer, _c1.c_str());
      TRI_AppendStringStringBuffer(buffer, " RETURN 1\"}");
    } else if (mod == 6) {
      TRI_AppendStringStringBuffer(buffer, "{\"query\":\"FOR c2 IN ");
      TRI_AppendStringStringBuffer(buffer, _c2.c_str());
      TRI_AppendStringStringBuffer(buffer, " FOR c3 IN ");
      TRI_AppendStringStringBuffer(buffer, _c3.c_str());
      TRI_AppendStringStringBuffer(buffer, " RETURN 1\"}");
    } else if (mod == 7) {
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
    char* ptr = TRI_StealStringBuffer(buffer);
    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);

    return (char const*)ptr;
  }

  std::string _c1;
  std::string _c2;
  std::string _c3;
};

struct TransactionCountTest : public BenchmarkOperation {
  TransactionCountTest() : BenchmarkOperation() {}

  ~TransactionCountTest() {}

  bool setUp(SimpleHttpClient* client) {
    return DeleteCollection(client, Collection) &&
           CreateCollection(client, Collection, 2);
  }

  void tearDown() {}

  std::string url(int const threadNumber, size_t const threadCounter,
                  size_t const globalCounter) {
    return std::string("/_api/transaction");
  }

  HttpRequest::HttpRequestType type(int const threadNumber,
                                    size_t const threadCounter,
                                    size_t const globalCounter) {
    return HttpRequest::HTTP_REQUEST_POST;
  }

  char const* payload(size_t* length, int const threadNumber,
                      size_t const threadCounter, size_t const globalCounter,
                      bool* mustFree) {
    TRI_string_buffer_t* buffer;
    buffer = TRI_CreateSizedStringBuffer(TRI_UNKNOWN_MEM_ZONE, 256);

    TRI_AppendStringStringBuffer(buffer, "{ \"collections\": { \"write\": \"");
    TRI_AppendStringStringBuffer(buffer, Collection.c_str());
    TRI_AppendStringStringBuffer(buffer,
                                 "\" }, \"action\": \"function () { var c = "
                                 "require(\\\"internal\\\").db[\\\"");
    TRI_AppendStringStringBuffer(buffer, Collection.c_str());
    TRI_AppendStringStringBuffer(buffer,
                                 "\\\"]; var startcount = c.count(); for (var "
                                 "i = 0; i < 50; ++i) { if (startcount + i !== "
                                 "c.count()) { throw \\\"error\\\"; } c.save({ "
                                 "}); } }\" }");

    *length = TRI_LengthStringBuffer(buffer);
    *mustFree = true;
    char* ptr = TRI_StealStringBuffer(buffer);
    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);

    return (char const*)ptr;
  }
};

struct TransactionDeadlockTest : public BenchmarkOperation {
  TransactionDeadlockTest() : BenchmarkOperation() {}

  ~TransactionDeadlockTest() {}

  bool setUp(SimpleHttpClient* client) {
    _c1 = std::string(Collection + "1");
    _c2 = std::string(Collection + "2");

    return DeleteCollection(client, _c1) && DeleteCollection(client, _c2) &&
           CreateCollection(client, _c1, 2) &&
           CreateCollection(client, _c2, 2) &&
           CreateDocument(client, _c2, "{ \"_key\": \"sum\", \"count\": 0 }");
  }

  void tearDown() {}

  std::string url(int const threadNumber, size_t const threadCounter,
                  size_t const globalCounter) {
    return std::string("/_api/transaction");
  }

  HttpRequest::HttpRequestType type(int const threadNumber,
                                    size_t const threadCounter,
                                    size_t const globalCounter) {
    return HttpRequest::HTTP_REQUEST_POST;
  }

  char const* payload(size_t* length, int const threadNumber,
                      size_t const threadCounter, size_t const globalCounter,
                      bool* mustFree) {
    size_t const mod = globalCounter % 2;
    TRI_string_buffer_t* buffer;
    buffer = TRI_CreateSizedStringBuffer(TRI_UNKNOWN_MEM_ZONE, 256);

    TRI_AppendStringStringBuffer(buffer, "{ \"collections\": { ");
    TRI_AppendStringStringBuffer(buffer, "\"write\": [ \"");

    if (mod == 0) {
      TRI_AppendStringStringBuffer(buffer, _c1.c_str());
    } else {
      TRI_AppendStringStringBuffer(buffer, _c2.c_str());
    }

    TRI_AppendStringStringBuffer(buffer,
                                 "\" ] }, \"action\": \"function () { ");
    TRI_AppendStringStringBuffer(buffer,
                                 "var c = require(\\\"internal\\\").db[\\\"");
    if (mod == 0) {
      TRI_AppendStringStringBuffer(buffer, _c2.c_str());
    } else {
      TRI_AppendStringStringBuffer(buffer, _c1.c_str());
    }
    TRI_AppendStringStringBuffer(buffer, "\\\"]; c.any();");

    TRI_AppendStringStringBuffer(buffer, " }\" }");

    *length = TRI_LengthStringBuffer(buffer);
    *mustFree = true;
    char* ptr = TRI_StealStringBuffer(buffer);
    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);

    return (char const*)ptr;
  }

  std::string _c1;
  std::string _c2;
};

struct TransactionMultiTest : public BenchmarkOperation {
  TransactionMultiTest() : BenchmarkOperation() {}

  ~TransactionMultiTest() {}

  bool setUp(SimpleHttpClient* client) {
    _c1 = std::string(Collection + "1");
    _c2 = std::string(Collection + "2");

    return DeleteCollection(client, _c1) && DeleteCollection(client, _c2) &&
           CreateCollection(client, _c1, 2) &&
           CreateCollection(client, _c2, 2) &&
           CreateDocument(client, _c2, "{ \"_key\": \"sum\", \"count\": 0 }");
  }

  void tearDown() {}

  std::string url(int const threadNumber, size_t const threadCounter,
                  size_t const globalCounter) {
    return std::string("/_api/transaction");
  }

  HttpRequest::HttpRequestType type(int const threadNumber,
                                    size_t const threadCounter,
                                    size_t const globalCounter) {
    return HttpRequest::HTTP_REQUEST_POST;
  }

  char const* payload(size_t* length, int const threadNumber,
                      size_t const threadCounter, size_t const globalCounter,
                      bool* mustFree) {
    size_t const mod = globalCounter % 2;
    TRI_string_buffer_t* buffer;
    buffer = TRI_CreateSizedStringBuffer(TRI_UNKNOWN_MEM_ZONE, 256);

    TRI_AppendStringStringBuffer(buffer, "{ \"collections\": { ");

    if (mod == 0) {
      TRI_AppendStringStringBuffer(buffer, "\"write\": [ \"");
    } else {
      TRI_AppendStringStringBuffer(buffer, "\"read\": [ \"");
    }

    TRI_AppendStringStringBuffer(buffer, _c1.c_str());
    TRI_AppendStringStringBuffer(buffer, "\", \"");
    TRI_AppendStringStringBuffer(buffer, _c2.c_str());
    TRI_AppendStringStringBuffer(buffer,
                                 "\" ] }, \"action\": \"function () { ");
    TRI_AppendStringStringBuffer(buffer,
                                 "var c1 = require(\\\"internal\\\").db[\\\"");
    TRI_AppendStringStringBuffer(buffer, _c1.c_str());
    TRI_AppendStringStringBuffer(
        buffer, "\\\"]; var c2 = require(\\\"internal\\\").db[\\\"");
    TRI_AppendStringStringBuffer(buffer, _c2.c_str());
    TRI_AppendStringStringBuffer(buffer, "\\\"]; ");

    if (mod == 0) {
      TRI_AppendStringStringBuffer(buffer,
                                   "var n = Math.floor(Math.random() * 25) + "
                                   "1; c1.save({ count: n }); var d = "
                                   "c2.document(\\\"sum\\\"); c2.update(d, { "
                                   "count: d.count + n });");
    } else {
      TRI_AppendStringStringBuffer(buffer,
                                   "var r1 = 0; c1.toArray().forEach(function "
                                   "(d) { r1 += d.count }); var r2 = "
                                   "c2.document(\\\"sum\\\").count; if (r1 !== "
                                   "r2) { throw \\\"error\\\"; }");
    }

    TRI_AppendStringStringBuffer(buffer, " }\" }");

    *length = TRI_LengthStringBuffer(buffer);
    *mustFree = true;
    char* ptr = TRI_StealStringBuffer(buffer);
    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);

    return (char const*)ptr;
  }

  std::string _c1;
  std::string _c2;
};

struct TransactionMultiCollectionTest : public BenchmarkOperation {
  TransactionMultiCollectionTest() : BenchmarkOperation() {}

  ~TransactionMultiCollectionTest() {}

  bool setUp(SimpleHttpClient* client) {
    _c1 = std::string(Collection + "1");
    _c2 = std::string(Collection + "2");

    return DeleteCollection(client, _c1) && DeleteCollection(client, _c2) &&
           CreateCollection(client, _c1, 2) && CreateCollection(client, _c2, 2);
  }

  void tearDown() {}

  std::string url(int const threadNumber, size_t const threadCounter,
                  size_t const globalCounter) {
    return std::string("/_api/transaction");
  }

  HttpRequest::HttpRequestType type(int const threadNumber,
                                    size_t const threadCounter,
                                    size_t const globalCounter) {
    return HttpRequest::HTTP_REQUEST_POST;
  }

  char const* payload(size_t* length, int const threadNumber,
                      size_t const threadCounter, size_t const globalCounter,
                      bool* mustFree) {
    TRI_string_buffer_t* buffer;
    buffer = TRI_CreateSizedStringBuffer(TRI_UNKNOWN_MEM_ZONE, 256);

    TRI_AppendStringStringBuffer(buffer, "{ \"collections\": { ");

    TRI_AppendStringStringBuffer(buffer, "\"write\": [ \"");
    TRI_AppendStringStringBuffer(buffer, _c1.c_str());
    TRI_AppendStringStringBuffer(buffer, "\", \"");
    TRI_AppendStringStringBuffer(buffer, _c2.c_str());
    TRI_AppendStringStringBuffer(buffer,
                                 "\" ] }, \"action\": \"function () { ");

    TRI_AppendStringStringBuffer(buffer,
                                 "var c1 = require(\\\"internal\\\").db[\\\"");
    TRI_AppendStringStringBuffer(buffer, _c1.c_str());
    TRI_AppendStringStringBuffer(
        buffer, "\\\"]; var c2 = require(\\\"internal\\\").db[\\\"");
    TRI_AppendStringStringBuffer(buffer, _c2.c_str());
    TRI_AppendStringStringBuffer(buffer, "\\\"]; ");

    TRI_AppendStringStringBuffer(buffer, "var doc = {");
    uint64_t const n = Complexity;
    for (uint64_t i = 0; i < n; ++i) {
      if (i > 0) {
        TRI_AppendStringStringBuffer(buffer, ", ");
      }
      TRI_AppendStringStringBuffer(buffer, "value");
      TRI_AppendUInt64StringBuffer(buffer, i);
      TRI_AppendStringStringBuffer(buffer, ": ");
      TRI_AppendUInt64StringBuffer(buffer, i);
    }
    TRI_AppendStringStringBuffer(buffer, " }; ");

    TRI_AppendStringStringBuffer(buffer, "c1.save(doc); c2.save(doc); }\" }");

    *length = TRI_LengthStringBuffer(buffer);
    *mustFree = true;
    char* ptr = TRI_StealStringBuffer(buffer);
    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);

    return (char const*)ptr;
  }

  std::string _c1;
  std::string _c2;
};

struct AqlInsertTest : public BenchmarkOperation {
  AqlInsertTest() : BenchmarkOperation() {}

  ~AqlInsertTest() {}

  bool setUp(SimpleHttpClient* client) {
    return DeleteCollection(client, Collection) &&
           CreateCollection(client, Collection, 2);
  }

  void tearDown() {}

  std::string url(int const threadNumber, size_t const threadCounter,
                  size_t const globalCounter) {
    return std::string("/_api/cursor");
  }

  HttpRequest::HttpRequestType type(int const threadNumber,
                                    size_t const threadCounter,
                                    size_t const globalCounter) {
    return HttpRequest::HTTP_REQUEST_POST;
  }

  char const* payload(size_t* length, int const threadNumber,
                      size_t const threadCounter, size_t const globalCounter,
                      bool* mustFree) {
    TRI_string_buffer_t* buffer;
    buffer = TRI_CreateSizedStringBuffer(TRI_UNKNOWN_MEM_ZONE, 256);

    TRI_AppendStringStringBuffer(buffer,
                                 "{\"query\":\"INSERT { _key: \\\"test");
    TRI_AppendInt64StringBuffer(buffer, (int64_t)globalCounter);
    TRI_AppendStringStringBuffer(buffer, "\\\"");

    uint64_t const n = Complexity;
    for (uint64_t i = 1; i <= n; ++i) {
      TRI_AppendStringStringBuffer(buffer, ",\\\"value");
      TRI_AppendUInt64StringBuffer(buffer, i);
      TRI_AppendStringStringBuffer(buffer, "\\\":true");
    }

    TRI_AppendStringStringBuffer(buffer, " } INTO ");
    TRI_AppendStringStringBuffer(buffer, Collection.c_str());
    TRI_AppendStringStringBuffer(buffer, "\"}");

    *length = TRI_LengthStringBuffer(buffer);
    *mustFree = true;
    char* ptr = TRI_StealStringBuffer(buffer);
    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);

    return (char const*)ptr;
  }
};

struct AqlV8Test : public BenchmarkOperation {
  AqlV8Test() : BenchmarkOperation() {}

  ~AqlV8Test() {}

  bool setUp(SimpleHttpClient* client) {
    return DeleteCollection(client, Collection) &&
           CreateCollection(client, Collection, 2);
  }

  void tearDown() {}

  std::string url(int const threadNumber, size_t const threadCounter,
                  size_t const globalCounter) {
    return std::string("/_api/cursor");
  }

  HttpRequest::HttpRequestType type(int const threadNumber,
                                    size_t const threadCounter,
                                    size_t const globalCounter) {
    return HttpRequest::HTTP_REQUEST_POST;
  }

  char const* payload(size_t* length, int const threadNumber,
                      size_t const threadCounter, size_t const globalCounter,
                      bool* mustFree) {
    TRI_string_buffer_t* buffer;
    buffer = TRI_CreateSizedStringBuffer(TRI_UNKNOWN_MEM_ZONE, 256);

    TRI_AppendStringStringBuffer(buffer,
                                 "{\"query\":\"INSERT { _key: \\\"test");
    TRI_AppendInt64StringBuffer(buffer, (int64_t)globalCounter);
    TRI_AppendStringStringBuffer(buffer, "\\\"");

    uint64_t const n = Complexity;
    for (uint64_t i = 1; i <= n; ++i) {
      TRI_AppendStringStringBuffer(buffer, ",\\\"value");
      TRI_AppendUInt64StringBuffer(buffer, i);
      TRI_AppendStringStringBuffer(buffer, "\\\":RAND(),\\\"test");
      TRI_AppendUInt64StringBuffer(buffer, i);
      TRI_AppendStringStringBuffer(buffer, "\\\":RANDOM_TOKEN(32)");
    }

    TRI_AppendStringStringBuffer(buffer, " } INTO ");
    TRI_AppendStringStringBuffer(buffer, Collection.c_str());
    TRI_AppendStringStringBuffer(buffer, "\"}");

    *length = TRI_LengthStringBuffer(buffer);
    *mustFree = true;
    char* ptr = TRI_StealStringBuffer(buffer);
    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);

    return (char const*)ptr;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a collection
////////////////////////////////////////////////////////////////////////////////

static bool DeleteCollection(SimpleHttpClient* client,
                             std::string const& name) {
  std::map<std::string, std::string> headerFields;
  SimpleHttpResult* result = nullptr;

  result = client->request(HttpRequest::HTTP_REQUEST_DELETE,
                           "/_api/collection/" + name, "", 0, headerFields);

  bool failed = true;
  if (result != nullptr) {
    int statusCode = result->getHttpReturnCode();
    if (statusCode == 200 || statusCode == 201 || statusCode == 202 ||
        statusCode == 404) {
      failed = false;
    }

    delete result;
  }

  return !failed;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a collection
////////////////////////////////////////////////////////////////////////////////

static bool CreateCollection(SimpleHttpClient* client, std::string const& name,
                             int const type) {
  std::map<std::string, std::string> headerFields;
  SimpleHttpResult* result = nullptr;

  std::string payload =
      "{\"name\":\"" + name + "\",\"type\":" + StringUtils::itoa(type) + "}";
  result = client->request(HttpRequest::HTTP_REQUEST_POST, "/_api/collection",
                           payload.c_str(), payload.size(), headerFields);

  bool failed = true;

  if (result != nullptr) {
    int statusCode = result->getHttpReturnCode();
    if (statusCode == 200 || statusCode == 201 || statusCode == 202) {
      failed = false;
    }

    delete result;
  }

  return !failed;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an index
////////////////////////////////////////////////////////////////////////////////

static bool CreateIndex(SimpleHttpClient* client, std::string const& name,
                        std::string const& type, std::string const& fields) {
  std::map<std::string, std::string> headerFields;
  SimpleHttpResult* result = nullptr;

  std::string payload =
      "{\"type\":\"" + type + "\",\"fields\":" + fields + ",\"unique\":false}";
  result = client->request(HttpRequest::HTTP_REQUEST_POST,
                           "/_api/index?collection=" + name, payload.c_str(),
                           payload.size(), headerFields);

  bool failed = true;

  if (result != nullptr) {
    if (result->getHttpReturnCode() == 200 ||
        result->getHttpReturnCode() == 201) {
      failed = false;
    }

    delete result;
  }

  return !failed;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a document
////////////////////////////////////////////////////////////////////////////////

static bool CreateDocument(SimpleHttpClient* client,
                           std::string const& collection,
                           std::string const& payload) {
  std::map<std::string, std::string> headerFields;
  SimpleHttpResult* result = nullptr;

  result = client->request(HttpRequest::HTTP_REQUEST_POST,
                           "/_api/document?collection=" + collection,
                           payload.c_str(), payload.size(), headerFields);

  bool failed = true;

  if (result != nullptr) {
    if (result->getHttpReturnCode() == 200 ||
        result->getHttpReturnCode() == 201 ||
        result->getHttpReturnCode() == 202) {
      failed = false;
    }

    delete result;
  }

  return !failed;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the test case for a name
////////////////////////////////////////////////////////////////////////////////

static BenchmarkOperation* GetTestCase(std::string const& name) {
  if (name == "version") {
    return new VersionTest();
  }
  if (name == "import-document") {
    return new DocumentImportTest();
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
  if (name == "shapes") {
    return new ShapesTest();
  }
  if (name == "shapes-append") {
    return new ShapesAppendTest();
  }
  if (name == "random-shapes") {
    return new RandomShapesTest();
  }
  if (name == "crud") {
    return new DocumentCrudTest();
  }
  if (name == "crud-append") {
    return new DocumentCrudAppendTest();
  }
  if (name == "crud-write-read") {
    return new DocumentCrudWriteReadTest();
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
  if (name == "deadlocktrx") {
    return new TransactionDeadlockTest();
  }
  if (name == "multi-collection") {
    return new TransactionMultiCollectionTest();
  }
  if (name == "aqlinsert") {
    return new AqlInsertTest();
  }
  if (name == "aqlv8") {
    return new AqlV8Test();
  }

  return nullptr;
}
