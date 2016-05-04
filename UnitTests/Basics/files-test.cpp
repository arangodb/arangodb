////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for files.c
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2012 triagens GmbH, Cologne, Germany
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

#include "Basics/Common.h"

#define BOOST_TEST_INCLUDED
#include <boost/test/unit_test.hpp>

#include "Basics/StringBuffer.h"
#include "Basics/files.h"
#include "Basics/json.h"
#include "Random/RandomGenerator.h"

using namespace arangodb::basics;

static bool Initialized = false;

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct CFilesSetup {
  CFilesSetup () : _directory(TRI_UNKNOWN_MEM_ZONE) {
    long systemError;
    std::string errorMessage;
    BOOST_TEST_MESSAGE("setup files");
    
    if (!Initialized) {
      Initialized = true;
      arangodb::RandomGenerator::initialize(arangodb::RandomGenerator::RandomType::MERSENNE);
    }

    _directory.appendText(TRI_GetTempPath());
    _directory.appendChar(TRI_DIR_SEPARATOR_CHAR);
    _directory.appendText("arangotest-");
    _directory.appendInteger(static_cast<uint64_t>(TRI_microtime()));
    _directory.appendInteger(arangodb::RandomGenerator::interval(UINT32_MAX));

    TRI_CreateDirectory(_directory.c_str(), systemError, errorMessage);
  }

  ~CFilesSetup () {
    BOOST_TEST_MESSAGE("tear-down files");
    
    // let's be sure we delete the right stuff
    assert(_directory.length() > 10);

    TRI_RemoveDirectory(_directory.c_str());
  }

  StringBuffer* writeFile (const char* blob) {
    static uint64_t counter = 0;

    StringBuffer* filename = new StringBuffer(TRI_UNKNOWN_MEM_ZONE);
    filename->appendText(_directory);
    filename->appendText("/tmp-");
    filename->appendInteger(++counter);
    filename->appendInteger(arangodb::RandomGenerator::interval(UINT32_MAX));

    FILE* fd = fopen(filename->c_str(), "wb");

    if (fd) {
      size_t numWritten = fwrite(blob, strlen(blob), 1, fd);
      (void) numWritten;
      fclose(fd);
    }
    else {
      BOOST_CHECK_EQUAL(false, true);
    }

    return filename;
  }

  StringBuffer _directory;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_SUITE(CFilesTest, CFilesSetup)

////////////////////////////////////////////////////////////////////////////////
/// @brief test file exists
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_existsfile) {
  StringBuffer* filename = writeFile("");
  BOOST_CHECK_EQUAL(true, TRI_ExistsFile(filename->c_str()));
  TRI_UnlinkFile(filename->c_str());

  delete filename;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test file size empty file
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_filesize_empty) {
  StringBuffer* filename = writeFile("");
  BOOST_CHECK_EQUAL(0, (int) TRI_SizeFile(filename->c_str()));

  TRI_UnlinkFile(filename->c_str());
  delete filename;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test file size
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_filesize_exists) {
  const char* buffer = "the quick brown fox";
  
  StringBuffer* filename = writeFile(buffer);
  BOOST_CHECK_EQUAL((int) strlen(buffer), (int) TRI_SizeFile(filename->c_str()));

  TRI_UnlinkFile(filename->c_str());
  delete filename;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test file size, non existing file
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_filesize_non) {
  BOOST_CHECK_EQUAL((int) -1, (int) TRI_SizeFile("h5uuuuui3unn645wejhdjhikjdsf"));
  BOOST_CHECK_EQUAL((int) -1, (int) TRI_SizeFile("dihnui8ngiu54"));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test absolute path
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_absolute_paths) {
  char* path;

#ifdef _WIN32
  path = TRI_GetAbsolutePath("the-fox", "\\tmp");

  BOOST_CHECK_EQUAL("\\tmp\\the-fox", path);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, path);

  path = TRI_GetAbsolutePath("the-fox.lol", "\\tmp");
  BOOST_CHECK_EQUAL("\\tmp\\the-fox.lol", path);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, path);
  
  path = TRI_GetAbsolutePath("the-fox.lol", "\\tmp\\the-fox");
  BOOST_CHECK_EQUAL("\\tmp\\the-fox\\the-fox.lol", path);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, path);
  
  path = TRI_GetAbsolutePath("file", "\\");
  BOOST_CHECK_EQUAL("\\file", path);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, path);
  
  path = TRI_GetAbsolutePath(".\\file", "\\");
  BOOST_CHECK_EQUAL("\\.\\file", path);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, path);
  
  path = TRI_GetAbsolutePath("\\file", "\\tmp");
  BOOST_CHECK_EQUAL("\\tmp\\file", path);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, path);
  
  path = TRI_GetAbsolutePath("\\file\\to\\file", "\\tmp");
  BOOST_CHECK_EQUAL("\\tmp\\file\\to\\file", path);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, path);
  
  path = TRI_GetAbsolutePath("file\\to\\file", "\\tmp");
  BOOST_CHECK_EQUAL("\\tmp\\file\\to\\file", path);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, path);
  
  path = TRI_GetAbsolutePath("c:\\file\\to\\file", "abc");
  BOOST_CHECK_EQUAL("c:\\file\\to\\file", path);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, path);
  
  path = TRI_GetAbsolutePath("c:\\file\\to\\file", "\\tmp");
  BOOST_CHECK_EQUAL("c:\\file\\to\\file", path);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, path);

#else

  path = TRI_GetAbsolutePath("the-fox", "/tmp");
  BOOST_CHECK_EQUAL("/tmp/the-fox", path);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, path);

  path = TRI_GetAbsolutePath("the-fox.lol", "/tmp");
  BOOST_CHECK_EQUAL("/tmp/the-fox.lol", path);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, path);
  
  path = TRI_GetAbsolutePath("the-fox.lol", "/tmp/the-fox");
  BOOST_CHECK_EQUAL("/tmp/the-fox/the-fox.lol", path);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, path);
  
  path = TRI_GetAbsolutePath("file", "/");
  BOOST_CHECK_EQUAL("/file", path);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, path);
  
  path = TRI_GetAbsolutePath("./file", "/");
  BOOST_CHECK_EQUAL("/./file", path);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, path);
  
  path = TRI_GetAbsolutePath("/file", "/tmp");
  BOOST_CHECK_EQUAL("/file", path);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, path);
  
  path = TRI_GetAbsolutePath("/file/to/file", "/tmp");
  BOOST_CHECK_EQUAL("/file/to/file", path);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, path);
  
  path = TRI_GetAbsolutePath("file/to/file", "/tmp");
  BOOST_CHECK_EQUAL("/tmp/file/to/file", path);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, path);
  
  path = TRI_GetAbsolutePath("c:file/to/file", "/tmp");
  BOOST_CHECK_EQUAL("c:file/to/file", path);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, path);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE_END ()

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
