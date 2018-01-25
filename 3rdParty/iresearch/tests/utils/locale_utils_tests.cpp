////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"
#include "utils/locale_utils.hpp"

#if defined (__GNUC__)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

  #include <boost/locale.hpp>

#if defined (__GNUC__)
  #pragma GCC diagnostic pop
#endif

namespace tests {
  class LocaleUtilsTestSuite: public ::testing::Test {
    virtual void SetUp() {
      // Code here will be called immediately after the constructor (right before each test).
    }

    virtual void TearDown() {
      // Code here will be called immediately after each test (right before the destructor).
    }
  };
}

using namespace tests;
using namespace iresearch::locale_utils;

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(LocaleUtilsTestSuite, test_locale_create) {
  {
    std::locale locale = iresearch::locale_utils::locale(nullptr);

    ASSERT_EQ(std::locale::classic(), locale);
  }

  {
    std::locale locale = iresearch::locale_utils::locale(nullptr, true);

    ASSERT_EQ(std::string("c.UTF-8"), std::use_facet<boost::locale::info>(locale).name());
    ASSERT_TRUE(std::use_facet<boost::locale::info>(locale).utf8());
  }

  {
    std::locale locale = iresearch::locale_utils::locale("*");

    ASSERT_EQ(std::string("*"), std::use_facet<boost::locale::info>(locale).name());
    ASSERT_FALSE(std::use_facet<boost::locale::info>(locale).utf8());
  }

  {
    std::locale locale = iresearch::locale_utils::locale("C");

    ASSERT_EQ(std::string("C"), std::use_facet<boost::locale::info>(locale).name());
    ASSERT_FALSE(std::use_facet<boost::locale::info>(locale).utf8());
  }

  {
    std::locale locale = iresearch::locale_utils::locale("en");

    ASSERT_EQ(std::string("en"), std::use_facet<boost::locale::info>(locale).name());
    ASSERT_FALSE(std::use_facet<boost::locale::info>(locale).utf8());
  }

  {
    std::locale locale = iresearch::locale_utils::locale("en_US");

    ASSERT_EQ(std::string("en_US"), std::use_facet<boost::locale::info>(locale).name());
    ASSERT_FALSE(std::use_facet<boost::locale::info>(locale).utf8());
  }

  {
    std::locale locale = iresearch::locale_utils::locale("en_US.UTF-8");

    ASSERT_EQ(std::string("en_US.UTF-8"), std::use_facet<boost::locale::info>(locale).name());
    ASSERT_TRUE(std::use_facet<boost::locale::info>(locale).utf8());
  }

  {
    std::locale locale = iresearch::locale_utils::locale("ru_RU.KOI8-R");

    ASSERT_EQ(std::string("ru_RU.KOI8-R"), std::use_facet<boost::locale::info>(locale).name());
    ASSERT_FALSE(std::use_facet<boost::locale::info>(locale).utf8());
  }

  {
    std::locale locale = iresearch::locale_utils::locale("ru_RU.KOI8-R", true);

    ASSERT_EQ(std::string("ru_RU.UTF-8"), std::use_facet<boost::locale::info>(locale).name());
    ASSERT_TRUE(std::use_facet<boost::locale::info>(locale).utf8());
  }

  {
    std::locale locale = iresearch::locale_utils::locale("InvalidString");

    ASSERT_EQ(std::string("InvalidString"), std::use_facet<boost::locale::info>(locale).name());
  }
}

TEST_F(LocaleUtilsTestSuite, test_locale_build) {
  {
    {
      std::locale expected = boost::locale::generator().generate("");
      std::locale locale = iresearch::locale_utils::locale("", "", "");

      ASSERT_EQ(std::use_facet<boost::locale::info>(expected).name(), std::use_facet<boost::locale::info>(locale).name());
      ASSERT_EQ(std::use_facet<boost::locale::info>(expected).language(), std::use_facet<boost::locale::info>(locale).language());
      ASSERT_EQ(std::use_facet<boost::locale::info>(expected).country(), std::use_facet<boost::locale::info>(locale).country());
      ASSERT_EQ(std::use_facet<boost::locale::info>(expected).encoding(), std::use_facet<boost::locale::info>(locale).encoding());
      ASSERT_EQ(std::use_facet<boost::locale::info>(expected).variant(), std::use_facet<boost::locale::info>(locale).variant());
      ASSERT_EQ(std::use_facet<boost::locale::info>(expected).utf8(), std::use_facet<boost::locale::info>(locale).utf8());
    }

    {
      std::locale locale = iresearch::locale_utils::locale("en", "", "");

      ASSERT_EQ(std::string("en"), std::use_facet<boost::locale::info>(locale).name());
      ASSERT_EQ(std::string("en"), std::use_facet<boost::locale::info>(locale).language());
      ASSERT_EQ(std::string(""), std::use_facet<boost::locale::info>(locale).country());
      ASSERT_EQ(std::string("us-ascii"), std::use_facet<boost::locale::info>(locale).encoding());
      ASSERT_EQ(std::string(""), std::use_facet<boost::locale::info>(locale).variant());
      ASSERT_FALSE(std::use_facet<boost::locale::info>(locale).utf8());
    }

    {
      std::locale locale = iresearch::locale_utils::locale("en", "US", "");

      ASSERT_EQ(std::string("en_US"), std::use_facet<boost::locale::info>(locale).name());
      ASSERT_EQ(std::string("en"), std::use_facet<boost::locale::info>(locale).language());
      ASSERT_EQ(std::string("US"), std::use_facet<boost::locale::info>(locale).country());
      ASSERT_EQ(std::string("us-ascii"), std::use_facet<boost::locale::info>(locale).encoding());
      ASSERT_EQ(std::string(""), std::use_facet<boost::locale::info>(locale).variant());
      ASSERT_FALSE(std::use_facet<boost::locale::info>(locale).utf8());
    }

    {
      std::locale locale = iresearch::locale_utils::locale("en", "US", "ISO-8859-1");

      ASSERT_EQ(std::string("en_US.ISO-8859-1"), std::use_facet<boost::locale::info>(locale).name());
      ASSERT_EQ(std::string("en"), std::use_facet<boost::locale::info>(locale).language());
      ASSERT_EQ(std::string("US"), std::use_facet<boost::locale::info>(locale).country());
      ASSERT_EQ(std::string("iso-8859-1"), std::use_facet<boost::locale::info>(locale).encoding());
      ASSERT_EQ(std::string(""), std::use_facet<boost::locale::info>(locale).variant());
      ASSERT_FALSE(std::use_facet<boost::locale::info>(locale).utf8());
    }

    {
      std::locale locale = iresearch::locale_utils::locale("en", "US", "ISO-8859-1", "args");

      ASSERT_EQ(std::string("en_US.ISO-8859-1@args"), std::use_facet<boost::locale::info>(locale).name());
      ASSERT_EQ(std::string("en"), std::use_facet<boost::locale::info>(locale).language());
      ASSERT_EQ(std::string("US"), std::use_facet<boost::locale::info>(locale).country());
      ASSERT_EQ(std::string("iso-8859-1"), std::use_facet<boost::locale::info>(locale).encoding());
      ASSERT_EQ(std::string("args"), std::use_facet<boost::locale::info>(locale).variant());
      ASSERT_FALSE(std::use_facet<boost::locale::info>(locale).utf8());
    }

    {
      std::locale locale = iresearch::locale_utils::locale("en", "US", "UTF-8");

      ASSERT_EQ(std::string("en_US.UTF-8"), std::use_facet<boost::locale::info>(locale).name());
      ASSERT_EQ(std::string("en"), std::use_facet<boost::locale::info>(locale).language());
      ASSERT_EQ(std::string("US"), std::use_facet<boost::locale::info>(locale).country());
      ASSERT_EQ(std::string("utf-8"), std::use_facet<boost::locale::info>(locale).encoding());
      ASSERT_EQ(std::string(""), std::use_facet<boost::locale::info>(locale).variant());
      ASSERT_TRUE(std::use_facet<boost::locale::info>(locale).utf8());
    }

    // ...........................................................................
    // invalid
    // ...........................................................................

    {
      std::locale locale = iresearch::locale_utils::locale("", "US", "");

      ASSERT_EQ(std::string("_US"), std::use_facet<boost::locale::info>(locale).name());
      ASSERT_EQ(std::string("C"), std::use_facet<boost::locale::info>(locale).language());
      ASSERT_EQ(std::string(""), std::use_facet<boost::locale::info>(locale).country());
      ASSERT_EQ(std::string("us-ascii"), std::use_facet<boost::locale::info>(locale).encoding());
      ASSERT_EQ(std::string(""), std::use_facet<boost::locale::info>(locale).variant());
      ASSERT_FALSE(std::use_facet<boost::locale::info>(locale).utf8());
    }

    {
      std::locale locale = iresearch::locale_utils::locale("", "US", "ISO-8859-1");

      ASSERT_EQ(std::string("_US.ISO-8859-1"), std::use_facet<boost::locale::info>(locale).name());
      ASSERT_EQ(std::string("C"), std::use_facet<boost::locale::info>(locale).language());
      ASSERT_EQ(std::string(""), std::use_facet<boost::locale::info>(locale).country());
      ASSERT_EQ(std::string("us-ascii"), std::use_facet<boost::locale::info>(locale).encoding());
      ASSERT_EQ(std::string(""), std::use_facet<boost::locale::info>(locale).variant());
      ASSERT_FALSE(std::use_facet<boost::locale::info>(locale).utf8());
    }

    {
      std::locale locale = iresearch::locale_utils::locale("", "US", "ISO-8859-1", "args");

      ASSERT_EQ(std::string("_US.ISO-8859-1@args"), std::use_facet<boost::locale::info>(locale).name());
      ASSERT_EQ(std::string("C"), std::use_facet<boost::locale::info>(locale).language());
      ASSERT_EQ(std::string(""), std::use_facet<boost::locale::info>(locale).country());
      ASSERT_EQ(std::string("us-ascii"), std::use_facet<boost::locale::info>(locale).encoding());
      ASSERT_EQ(std::string(""), std::use_facet<boost::locale::info>(locale).variant());
      ASSERT_FALSE(std::use_facet<boost::locale::info>(locale).utf8());
    }

    {
      std::locale locale = iresearch::locale_utils::locale("", "US", "UTF-8");

      ASSERT_EQ(std::string("_US.UTF-8"), std::use_facet<boost::locale::info>(locale).name());
      ASSERT_EQ(std::string("C"), std::use_facet<boost::locale::info>(locale).language());
      ASSERT_EQ(std::string(""), std::use_facet<boost::locale::info>(locale).country());
      ASSERT_EQ(std::string("us-ascii"), std::use_facet<boost::locale::info>(locale).encoding());
      ASSERT_EQ(std::string(""), std::use_facet<boost::locale::info>(locale).variant());
      ASSERT_FALSE(std::use_facet<boost::locale::info>(locale).utf8());
    }

    {
      std::locale locale = iresearch::locale_utils::locale("", "", "ISO-8859-1");

      ASSERT_EQ(std::string(".ISO-8859-1"), std::use_facet<boost::locale::info>(locale).name());
      ASSERT_EQ(std::string("C"), std::use_facet<boost::locale::info>(locale).language());
      ASSERT_EQ(std::string(""), std::use_facet<boost::locale::info>(locale).country());
      ASSERT_EQ(std::string("us-ascii"), std::use_facet<boost::locale::info>(locale).encoding());
      ASSERT_EQ(std::string(""), std::use_facet<boost::locale::info>(locale).variant());
      ASSERT_FALSE(std::use_facet<boost::locale::info>(locale).utf8());
    }

    {
      std::locale locale = iresearch::locale_utils::locale("", "", "ISO-8859-1", "args");

      ASSERT_EQ(std::string(".ISO-8859-1@args"), std::use_facet<boost::locale::info>(locale).name());
      ASSERT_EQ(std::string("C"), std::use_facet<boost::locale::info>(locale).language());
      ASSERT_EQ(std::string(""), std::use_facet<boost::locale::info>(locale).country());
      ASSERT_EQ(std::string("us-ascii"), std::use_facet<boost::locale::info>(locale).encoding());
      ASSERT_EQ(std::string(""), std::use_facet<boost::locale::info>(locale).variant());
      ASSERT_FALSE(std::use_facet<boost::locale::info>(locale).utf8());
    }

    {
      std::locale locale = iresearch::locale_utils::locale("", "", "UTF-8");

      ASSERT_EQ(std::string(".UTF-8"), std::use_facet<boost::locale::info>(locale).name());
      ASSERT_EQ(std::string("C"), std::use_facet<boost::locale::info>(locale).language());
      ASSERT_EQ(std::string(""), std::use_facet<boost::locale::info>(locale).country());
      ASSERT_EQ(std::string("us-ascii"), std::use_facet<boost::locale::info>(locale).encoding());
      ASSERT_EQ(std::string(""), std::use_facet<boost::locale::info>(locale).variant());
      ASSERT_FALSE(std::use_facet<boost::locale::info>(locale).utf8());
    }

    {
      std::locale locale = iresearch::locale_utils::locale("", "", "UTF-8", "args");

      ASSERT_EQ(std::string(".UTF-8@args"), std::use_facet<boost::locale::info>(locale).name());
      ASSERT_EQ(std::string("C"), std::use_facet<boost::locale::info>(locale).language());
      ASSERT_EQ(std::string(""), std::use_facet<boost::locale::info>(locale).country());
      ASSERT_EQ(std::string("us-ascii"), std::use_facet<boost::locale::info>(locale).encoding());
      ASSERT_EQ(std::string(""), std::use_facet<boost::locale::info>(locale).variant());
      ASSERT_FALSE(std::use_facet<boost::locale::info>(locale).utf8());
    }

    // ...........................................................................
    // invalid
    // ...........................................................................

    {
      std::locale locale = iresearch::locale_utils::locale("_.@", "", "", "");

      ASSERT_EQ(std::string("_.@_.@"), std::use_facet<boost::locale::info>(locale).name());
      ASSERT_EQ(std::string("C"), std::use_facet<boost::locale::info>(locale).language());
      ASSERT_EQ(std::string(""), std::use_facet<boost::locale::info>(locale).country());
      ASSERT_EQ(std::string("us-ascii"), std::use_facet<boost::locale::info>(locale).encoding());
      ASSERT_EQ(std::string(""), std::use_facet<boost::locale::info>(locale).variant());
      ASSERT_FALSE(std::use_facet<boost::locale::info>(locale).utf8());
    }
  }
}

TEST_F(LocaleUtilsTestSuite, test_locale_info) {
  {
    std::locale locale = std::locale::classic();

    ASSERT_EQ(std::string(""), iresearch::locale_utils::country(locale));
    ASSERT_EQ(std::string("us-ascii"), iresearch::locale_utils::encoding(locale));
    ASSERT_EQ(std::string("c"), iresearch::locale_utils::language(locale));
    ASSERT_EQ(std::string("C"), iresearch::locale_utils::name(locale));
  }

  {
    std::locale locale = iresearch::locale_utils::locale(nullptr);

    ASSERT_EQ(std::string(""), iresearch::locale_utils::country(locale));
    ASSERT_EQ(std::string("us-ascii"), iresearch::locale_utils::encoding(locale));
    ASSERT_EQ(std::string("c"), iresearch::locale_utils::language(locale));
    ASSERT_EQ(std::string("C"), iresearch::locale_utils::name(locale));
  }

  {
    std::locale locale = iresearch::locale_utils::locale("*");

    ASSERT_EQ(std::string(""), iresearch::locale_utils::country(locale));
    ASSERT_EQ(std::string("us-ascii"), iresearch::locale_utils::encoding(locale));
    ASSERT_EQ(std::string("*"), iresearch::locale_utils::language(locale));
    ASSERT_EQ(std::string("*"), iresearch::locale_utils::name(locale));
  }

  {
    std::locale locale = iresearch::locale_utils::locale("C");

    ASSERT_EQ(std::string(""), iresearch::locale_utils::country(locale));
    ASSERT_EQ(std::string("us-ascii"), iresearch::locale_utils::encoding(locale));
    ASSERT_EQ(std::string("c"), iresearch::locale_utils::language(locale));
    ASSERT_EQ(std::string("C"), iresearch::locale_utils::name(locale));
  }

  {
    std::locale locale = iresearch::locale_utils::locale("en");

    ASSERT_EQ(std::string(""), iresearch::locale_utils::country(locale));
    ASSERT_EQ(std::string("us-ascii"), iresearch::locale_utils::encoding(locale));
    ASSERT_EQ(std::string("en"), iresearch::locale_utils::language(locale));
    ASSERT_EQ(std::string("en"), iresearch::locale_utils::name(locale));
  }

  {
    std::locale locale = iresearch::locale_utils::locale("en_US");

    ASSERT_EQ(std::string("US"), iresearch::locale_utils::country(locale));
    ASSERT_EQ(std::string("us-ascii"), iresearch::locale_utils::encoding(locale));
    ASSERT_EQ(std::string("en"), iresearch::locale_utils::language(locale));
    ASSERT_EQ(std::string("en_US"), iresearch::locale_utils::name(locale));
  }

  {
    std::locale locale = iresearch::locale_utils::locale("en_US.UTF-8");

    ASSERT_EQ(std::string("US"), iresearch::locale_utils::country(locale));
    ASSERT_EQ(std::string("utf-8"), iresearch::locale_utils::encoding(locale));
    ASSERT_EQ(std::string("en"), iresearch::locale_utils::language(locale));
    ASSERT_EQ(std::string("en_US.UTF-8"), iresearch::locale_utils::name(locale));
  }

  {
    std::locale locale = iresearch::locale_utils::locale("ru_RU.KOI8-R");

    ASSERT_EQ(std::string("RU"), iresearch::locale_utils::country(locale));
    ASSERT_EQ(std::string("koi8-r"), iresearch::locale_utils::encoding(locale));
    ASSERT_EQ(std::string("ru"), iresearch::locale_utils::language(locale));
    ASSERT_EQ(std::string("ru_RU.KOI8-R"), iresearch::locale_utils::name(locale));
  }

  {
    std::locale locale = iresearch::locale_utils::locale("InvalidString");

    ASSERT_EQ(std::string(""), iresearch::locale_utils::country(locale));
    ASSERT_EQ(std::string("us-ascii"), iresearch::locale_utils::encoding(locale));
    ASSERT_EQ(std::string("invalidstring"), iresearch::locale_utils::language(locale));
    ASSERT_EQ(std::string("InvalidString"), iresearch::locale_utils::name(locale));
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------