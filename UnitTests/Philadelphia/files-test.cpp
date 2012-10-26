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

#include <boost/test/unit_test.hpp>

#include "Basics/StringBuffer.h"
#include "BasicsC/files.h"

using namespace triagens::basics;


// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct CFilesSetup {
  CFilesSetup () : _directory(TRI_UNKNOWN_MEM_ZONE) {
    BOOST_TEST_MESSAGE("setup files");

    _directory.appendText("/tmp/arangotest-");
    _directory.appendInteger((uint64_t) TRI_microtime());

    TRI_CreateDirectory(_directory.c_str());
  }

  ~CFilesSetup () {
    BOOST_TEST_MESSAGE("tear-down files");
    
    // let's be sure we delete the right stuff
    assert(_directory.length() > 10);
    assert(memcmp((void*) _directory.c_str(), (void*) "/tmp/arangotest-", 16) == 0);

    TRI_RemoveDirectory(_directory.c_str());
  }

  StringBuffer* writeFile (const char* blob) {
    static uint64_t counter = 0;

    StringBuffer* filename = new StringBuffer(TRI_UNKNOWN_MEM_ZONE);
    filename->appendText(_directory);
    filename->appendText("/tmp-");
    filename->appendInteger(++counter);

    int fd = TRI_OPEN(filename->c_str(), O_RDWR | O_CREAT);

    if (fd) {
      ::write(fd, blob, strlen(blob));
      ::close(fd);
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
  BOOST_CHECK_EQUAL(strlen(buffer), (int) TRI_SizeFile(filename->c_str()));

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
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE_END ()

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
