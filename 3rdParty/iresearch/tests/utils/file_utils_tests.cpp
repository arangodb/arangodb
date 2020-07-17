////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "tests_shared.hpp"
#include "utils/file_utils.hpp"

#ifdef _WIN32
  #define STRING(str) L ## str
#else
  #define STRING(str) str
#endif

TEST(file_utils_tests, path_parts) {
  typedef irs::file_utils::path_parts_t::ref_t ref_t;

  // nullptr
  {
    auto parts = irs::file_utils::path_parts(nullptr);
    ASSERT_EQ(ref_t::NIL, parts.dirname);
    ASSERT_EQ(ref_t::NIL, parts.basename);
    ASSERT_EQ(ref_t::NIL, parts.stem);
    ASSERT_EQ(ref_t::NIL, parts.extension);
  }

  // ...........................................................................
  // no parent
  // ...........................................................................

  // no parent, stem(empty), no extension
  {
    auto data = STRING("");
    auto parts = irs::file_utils::path_parts(data);
    ASSERT_EQ(ref_t::NIL, parts.dirname);
    ASSERT_EQ(ref_t(), parts.basename);
    ASSERT_EQ(ref_t(), parts.stem);
    ASSERT_EQ(ref_t::NIL, parts.extension);
  }

  // no parent, stem(empty), extension(empty)
  {
    auto data = STRING(".");
    auto parts = irs::file_utils::path_parts(data);
    ASSERT_EQ(ref_t::NIL, parts.dirname);
    ASSERT_EQ(ref_t(STRING(".")), parts.basename);
    ASSERT_EQ(ref_t(STRING("")), parts.stem);
    ASSERT_EQ(ref_t(STRING("")), parts.extension);
  }

  // no parent, stem(empty), extension(non-empty)
  {
    auto data = STRING(".xyz");
    auto parts = irs::file_utils::path_parts(data);
    ASSERT_EQ(ref_t::NIL, parts.dirname);
    ASSERT_EQ(ref_t(STRING(".xyz")), parts.basename);
    ASSERT_EQ(ref_t(STRING("")), parts.stem);
    ASSERT_EQ(ref_t(STRING("xyz")), parts.extension);
  }

  // no parent, stem(non-empty), no extension
  {
    auto data = STRING("abc");
    auto parts = irs::file_utils::path_parts(data);
    ASSERT_EQ(ref_t::NIL, parts.dirname);
    ASSERT_EQ(ref_t(STRING("abc")), parts.basename);
    ASSERT_EQ(ref_t(STRING("abc")), parts.stem);
    ASSERT_EQ(ref_t::NIL, parts.extension);
  }
  
  // no parent, stem(non-empty), extension(empty)
  {
    auto data = STRING("abc.");
    auto parts = irs::file_utils::path_parts(data);
    ASSERT_EQ(ref_t::NIL, parts.dirname);
    ASSERT_EQ(ref_t(STRING("abc.")), parts.basename);
    ASSERT_EQ(ref_t(STRING("abc")), parts.stem);
    ASSERT_EQ(ref_t(STRING("")), parts.extension);
  }

  // no parent, stem(non-empty), extension(non-empty)
  {
    auto data = STRING("abc.xyz");
    auto parts = irs::file_utils::path_parts(data);
    ASSERT_EQ(ref_t::NIL, parts.dirname);
    ASSERT_EQ(ref_t(STRING("abc.xyz")), parts.basename);
    ASSERT_EQ(ref_t(STRING("abc")), parts.stem);
    ASSERT_EQ(ref_t(STRING("xyz")), parts.extension);
  }

  // no parent, stem(non-empty), extension(non-empty) (multi-extension)
  {
    auto data = STRING("abc.def..xyz");
    auto parts = irs::file_utils::path_parts(data);
    ASSERT_EQ(ref_t::NIL, parts.dirname);
    ASSERT_EQ(ref_t(STRING("abc.def..xyz")), parts.basename);
    ASSERT_EQ(ref_t(STRING("abc.def.")), parts.stem);
    ASSERT_EQ(ref_t(STRING("xyz")), parts.extension);
  }

  // ...........................................................................
  // empty parent
  // ...........................................................................

  // parent(empty), stem(empty), no extension
  {
    auto data = STRING("/");
    auto parts = irs::file_utils::path_parts(data);
    ASSERT_EQ(ref_t(STRING("")), parts.dirname);
    ASSERT_EQ(ref_t(STRING("")), parts.basename);
    ASSERT_EQ(ref_t(STRING("")), parts.stem);
    ASSERT_EQ(ref_t::NIL, parts.extension);
  }

  // parent(empty), stem(empty), extension(empty)
  {
    auto data = STRING("/.");
    auto parts = irs::file_utils::path_parts(data);
    ASSERT_EQ(ref_t(STRING("")), parts.dirname);
    ASSERT_EQ(ref_t(STRING(".")), parts.basename);
    ASSERT_EQ(ref_t(STRING("")), parts.stem);
    ASSERT_EQ(ref_t(STRING("")), parts.extension);
  }

  // parent(empty), stem(empty), extension(non-empty)
  {
    auto data = STRING("/.xyz");
    auto parts = irs::file_utils::path_parts(data);
    ASSERT_EQ(ref_t(STRING("")), parts.dirname);
    ASSERT_EQ(ref_t(STRING(".xyz")), parts.basename);
    ASSERT_EQ(ref_t(STRING("")), parts.stem);
    ASSERT_EQ(ref_t(STRING("xyz")), parts.extension);
  }

  // parent(empty), stem(non-empty), no extension
  {
    auto data = STRING("/abc");
    auto parts = irs::file_utils::path_parts(data);
    ASSERT_EQ(ref_t(STRING("")), parts.dirname);
    ASSERT_EQ(ref_t(STRING("abc")), parts.basename);
    ASSERT_EQ(ref_t(STRING("abc")), parts.stem);
    ASSERT_EQ(ref_t::NIL, parts.extension);
  }

  // parent(empty), stem(non-empty), extension(empty)
  {
    auto data = STRING("/abc.");
    auto parts = irs::file_utils::path_parts(data);
    ASSERT_EQ(ref_t(STRING("")), parts.dirname);
    ASSERT_EQ(ref_t(STRING("abc.")), parts.basename);
    ASSERT_EQ(ref_t(STRING("abc")), parts.stem);
    ASSERT_EQ(ref_t(STRING("")), parts.extension);
  }

  // parent(empty), stem(non-empty), extension(non-empty)
  {
    auto data = STRING("/abc.xyz");
    auto parts = irs::file_utils::path_parts(data);
    ASSERT_EQ(ref_t(STRING("")), parts.dirname);
    ASSERT_EQ(ref_t(STRING("abc.xyz")), parts.basename);
    ASSERT_EQ(ref_t(STRING("abc")), parts.stem);
    ASSERT_EQ(ref_t(STRING("xyz")), parts.extension);
  }

  // parent(empty), stem(non-empty), extension(non-empty) (multi-extension)
  {
    auto data = STRING("/abc.def..xyz");
    auto parts = irs::file_utils::path_parts(data);
    ASSERT_EQ(ref_t(STRING("")), parts.dirname);
    ASSERT_EQ(ref_t(STRING("abc.def..xyz")), parts.basename);
    ASSERT_EQ(ref_t(STRING("abc.def.")), parts.stem);
    ASSERT_EQ(ref_t(STRING("xyz")), parts.extension);
  }

  // ...........................................................................
  // non-empty parent
  // ...........................................................................

  // parent(non-empty), stem(empty), no extension
  {
    auto data = STRING("klm/");
    auto parts = irs::file_utils::path_parts(data);
    ASSERT_EQ(ref_t(STRING("klm")), parts.dirname);
    ASSERT_EQ(ref_t(STRING("")), parts.basename);
    ASSERT_EQ(ref_t(STRING("")), parts.stem);
    ASSERT_EQ(ref_t::NIL, parts.extension);
  }

  // parent(non-empty), stem(empty), extension(empty)
  {
    auto data = STRING("klm/.");
    auto parts = irs::file_utils::path_parts(data);
    ASSERT_EQ(ref_t(STRING("klm")), parts.dirname);
    ASSERT_EQ(ref_t(STRING(".")), parts.basename);
    ASSERT_EQ(ref_t(STRING("")), parts.stem);
    ASSERT_EQ(ref_t(STRING("")), parts.extension);
  }

  // parent(non-empty), stem(empty), extension(non-empty)
  {
    auto data = STRING("klm/.xyz");
    auto parts = irs::file_utils::path_parts(data);
    ASSERT_EQ(ref_t(STRING("klm")), parts.dirname);
    ASSERT_EQ(ref_t(STRING(".xyz")), parts.basename);
    ASSERT_EQ(ref_t(STRING("")), parts.stem);
    ASSERT_EQ(ref_t(STRING("xyz")), parts.extension);
  }

  // parent(non-empty), stem(non-empty), no extension
  {
    auto data = STRING("klm/abc");
    auto parts = irs::file_utils::path_parts(data);
    ASSERT_EQ(ref_t(STRING("klm")), parts.dirname);
    ASSERT_EQ(ref_t(STRING("abc")), parts.basename);
    ASSERT_EQ(ref_t(STRING("abc")), parts.stem);
    ASSERT_EQ(ref_t::NIL, parts.extension);
  }

  // parent(non-empty), stem(non-empty), extension(empty)
  {
    auto data = STRING("klm/abc.");
    auto parts = irs::file_utils::path_parts(data);
    ASSERT_EQ(ref_t(STRING("klm")), parts.dirname);
    ASSERT_EQ(ref_t(STRING("abc.")), parts.basename);
    ASSERT_EQ(ref_t(STRING("abc")), parts.stem);
    ASSERT_EQ(ref_t(STRING("")), parts.extension);
  }

  // parent(non-empty), stem(non-empty), extension(non-empty)
  {
    auto data = STRING("klm/abc.xyz");
    auto parts = irs::file_utils::path_parts(data);
    ASSERT_EQ(ref_t(STRING("klm")), parts.dirname);
    ASSERT_EQ(ref_t(STRING("abc.xyz")), parts.basename);
    ASSERT_EQ(ref_t(STRING("abc")), parts.stem);
    ASSERT_EQ(ref_t(STRING("xyz")), parts.extension);
  }

  // parent(non-empty), stem(non-empty), extension(non-empty) (multi-extension)
  {
    auto data = STRING("klm/abc.def..xyz");
    auto parts = irs::file_utils::path_parts(data);
    ASSERT_EQ(ref_t(STRING("klm")), parts.dirname);
    ASSERT_EQ(ref_t(STRING("abc.def..xyz")), parts.basename);
    ASSERT_EQ(ref_t(STRING("abc.def.")), parts.stem);
    ASSERT_EQ(ref_t(STRING("xyz")), parts.extension);
  }

  // parent(non-empty), stem(non-empty), extension(non-empty) (multi-parent, multi-extension)
  {
    auto data = STRING("/123/klm/abc.def..xyz");
    auto parts = irs::file_utils::path_parts(data);
    ASSERT_EQ(ref_t(STRING("/123/klm")), parts.dirname);
    ASSERT_EQ(ref_t(STRING("abc.def..xyz")), parts.basename);
    ASSERT_EQ(ref_t(STRING("abc.def.")), parts.stem);
    ASSERT_EQ(ref_t(STRING("xyz")), parts.extension);
  }

  #ifdef _WIN32
    // ...........................................................................
    // win32 non-empty parent
    // ...........................................................................

    // parent(non-empty), stem(empty), no extension
    {
      auto data = STRING("klm\\");
      auto parts = irs::file_utils::path_parts(data);
      ASSERT_EQ(ref_t(STRING("klm")), parts.dirname);
      ASSERT_EQ(ref_t(STRING("")), parts.basename);
      ASSERT_EQ(ref_t(STRING("")), parts.stem);
      ASSERT_EQ(ref_t::NIL, parts.extension);
    }

    // parent(non-empty), stem(empty), extension(empty)
    {
      auto data = STRING("klm\\.");
      auto parts = irs::file_utils::path_parts(data);
      ASSERT_EQ(ref_t(STRING("klm")), parts.dirname);
      ASSERT_EQ(ref_t(STRING(".")), parts.basename);
      ASSERT_EQ(ref_t(STRING("")), parts.stem);
      ASSERT_EQ(ref_t(STRING("")), parts.extension);
    }

    // parent(non-empty), stem(empty), extension(non-empty)
    {
      auto data = STRING("klm\\.xyz");
      auto parts = irs::file_utils::path_parts(data);
      ASSERT_EQ(ref_t(STRING("klm")), parts.dirname);
      ASSERT_EQ(ref_t(STRING(".xyz")), parts.basename);
      ASSERT_EQ(ref_t(STRING("")), parts.stem);
      ASSERT_EQ(ref_t(STRING("xyz")), parts.extension);
    }

    // parent(non-empty), stem(non-empty), no extension
    {
      auto data = STRING("klm\\abc");
      auto parts = irs::file_utils::path_parts(data);
      ASSERT_EQ(ref_t(STRING("klm")), parts.dirname);
      ASSERT_EQ(ref_t(STRING("abc")), parts.basename);
      ASSERT_EQ(ref_t(STRING("abc")), parts.stem);
      ASSERT_EQ(ref_t::NIL, parts.extension);
    }

    // parent(non-empty), stem(non-empty), extension(empty)
    {
      auto data = STRING("klm\\abc.");
      auto parts = irs::file_utils::path_parts(data);
      ASSERT_EQ(ref_t(STRING("klm")), parts.dirname);
      ASSERT_EQ(ref_t(STRING("abc.")), parts.basename);
      ASSERT_EQ(ref_t(STRING("abc")), parts.stem);
      ASSERT_EQ(ref_t(STRING("")), parts.extension);
    }

    // parent(non-empty), stem(non-empty), extension(non-empty)
    {
      auto data = STRING("klm\\abc.xyz");
      auto parts = irs::file_utils::path_parts(data);
      ASSERT_EQ(ref_t(STRING("klm")), parts.dirname);
      ASSERT_EQ(ref_t(STRING("abc.xyz")), parts.basename);
      ASSERT_EQ(ref_t(STRING("abc")), parts.stem);
      ASSERT_EQ(ref_t(STRING("xyz")), parts.extension);
    }

    // parent(non-empty), stem(non-empty), extension(non-empty) (multi-extension)
    {
      auto data = STRING("klm\\abc.def..xyz");
      auto parts = irs::file_utils::path_parts(data);
      ASSERT_EQ(ref_t(STRING("klm")), parts.dirname);
      ASSERT_EQ(ref_t(STRING("abc.def..xyz")), parts.basename);
      ASSERT_EQ(ref_t(STRING("abc.def.")), parts.stem);
      ASSERT_EQ(ref_t(STRING("xyz")), parts.extension);
    }

    // parent(non-empty), stem(non-empty), extension(non-empty) (multi-parent, multi-extension)
    {
      auto data = STRING("/123\\klm/abc.def..xyz");
      auto parts = irs::file_utils::path_parts(data);
      ASSERT_EQ(ref_t(STRING("/123\\klm")), parts.dirname);
      ASSERT_EQ(ref_t(STRING("abc.def..xyz")), parts.basename);
      ASSERT_EQ(ref_t(STRING("abc.def.")), parts.stem);
      ASSERT_EQ(ref_t(STRING("xyz")), parts.extension);
    }
  #endif
}
