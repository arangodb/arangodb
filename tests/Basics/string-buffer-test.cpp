////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for TRI_string_buffer_t
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
/// @author Tim Becker
/// @author Dr. Frank Celler
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#include "catch.hpp"

#include "Basics/StringBuffer.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                    private macros
// -----------------------------------------------------------------------------

#define BOOSTa_CHECK_EQUAL_COLLECTION(start, bb, tolerance) { \
    using std::distance; \
    using std::begin; \
    using std::end; \
    auto a = begin(aa), ae = end(aa); \
    auto b = begin(bb); \
    BOOST_REQUIRE_EQUAL(distance(a, ae), distance(b, end(bb))); \
    for(; a != ae; ++a, ++b) { \
        BOOST_CHECK_CLOSE(*a, *b, tolerance); \
    } \
}

#define STRLEN(a) (strnlen((a), 1024))

// -----------------------------------------------------------------------------
// --SECTION--                                                 private constants
// -----------------------------------------------------------------------------

#define ABC_C "ABCDEFGHIJKLMNOP"
#define AEP_C "AEPDEFGHIJKLMNOP"
#define REP_C "REPDEFGHIJKLMNOP"
#define STR_C "The quick brown fox jumped over the laxy dog"

static char const* ABC_const = ABC_C;
static char const* AEP = AEP_C;
static char const* F_2_T = "56789A";
static char const* ONETWOTHREE = "123";
static char const* REP = REP_C;
static char const* STR = STR_C;
static char const* STRSTR = STR_C STR_C;
static char const* STRSTRABC_const = STR_C STR_C ABC_C;
static char const* TWNTYA = "aaaaaaaaaaaaaaaaaaaa";
static char const* Z_2_T = "0123456789A";

#define TRI_LastCharStringBuffer(s) *(TRI_EndStringBuffer(s) - 1)

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("CStringBufferTest", "[string]") {

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_str_append
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_str_append") {
  size_t l1, l2;

  TRI_string_buffer_t sb;
  TRI_InitStringBuffer(&sb);

  TRI_AppendStringStringBuffer(&sb, STR);
  TRI_AppendStringStringBuffer(&sb, STR);

  l1 = STRLEN(STRSTR);
  l2 = STRLEN(sb._buffer);
  
  CHECK(l1 == l2);

  CHECK(std::string(STRSTR, l1) == std::string(sb._buffer, l2));
  
  TRI_AppendString2StringBuffer(&sb, ABC_const, 3); // ABC_const ... Z

  l2 = STRLEN(sb._buffer);
  
  CHECK(std::string(STRSTRABC_const, l2) == std::string(sb._buffer, l2));

  TRI_ClearStringBuffer(&sb);
  TRI_AppendStringStringBuffer(&sb, STR);

  l2 = STRLEN(sb._buffer);

  CHECK(std::string(STRSTR, l2) == std::string(sb._buffer, l2));

  TRI_DestroyStringBuffer(&sb);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_char_append
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_char_append") {
  size_t l1, l2, i;

  TRI_string_buffer_t sb;
  TRI_InitStringBuffer(&sb);
  
  for (i = 0;  i != 20;  ++i) {
    TRI_AppendCharStringBuffer(&sb, 'a');
  }

  l1 = STRLEN(TWNTYA);
  l2 = STRLEN(sb._buffer);
  
  CHECK((l1) == l2);
  
  CHECK(std::string(TWNTYA, l1) == std::string(sb._buffer, l2));

  TRI_DestroyStringBuffer(&sb);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_swp
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_swp") {
  size_t l1, l2, i;

  TRI_string_buffer_t sb1, sb2;
  TRI_InitStringBuffer(&sb1);
  TRI_InitStringBuffer(&sb2);
  
  for (i = 0;  i != 20;  ++i) {
    TRI_AppendCharStringBuffer(&sb1, 'a');
  }

  TRI_AppendStringStringBuffer(&sb2, STR);

  TRI_SwapStringBuffer(&sb1, &sb2);

  l1 = STRLEN(TWNTYA);
  l2 = STRLEN(STR);
  
  CHECK(std::string(TWNTYA, l1) == std::string(sb2._buffer, l1));

  CHECK(std::string(STR, l2) == std::string(sb1._buffer, l2));

  TRI_DestroyStringBuffer(&sb1);
  TRI_DestroyStringBuffer(&sb2);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_begin_end_empty_clear
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_begin_end_empty_clear") {
  size_t l1;
  const char * ptr;

  TRI_string_buffer_t sb;
  TRI_InitStringBuffer(&sb);
  
  TRI_AppendStringStringBuffer(&sb, STR);
  
  ptr = TRI_BeginStringBuffer(&sb);

  CHECK((void*) sb._buffer == (void*) ptr);

  l1 = STRLEN(STR);
  ptr = TRI_EndStringBuffer(&sb);

  CHECK((void*)(sb._buffer + l1) == (void*) ptr);

  CHECK(! TRI_EmptyStringBuffer(&sb));

  TRI_ClearStringBuffer(&sb);

  CHECK(TRI_EmptyStringBuffer(&sb));

  TRI_DestroyStringBuffer(&sb);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_cpy
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_cpy") {
  size_t l1, i;

  TRI_string_buffer_t sb1, sb2;
  TRI_InitStringBuffer(&sb1);
  TRI_InitStringBuffer(&sb2);
  
  for (i = 0;  i != 20;  ++i) {
    TRI_AppendCharStringBuffer(&sb1, 'a');
  }

  TRI_AppendStringStringBuffer(&sb2, STR);
  TRI_CopyStringBuffer(&sb1, &sb2);

  l1 = STRLEN(STR);

  CHECK((l1) == STRLEN(sb1._buffer));

  CHECK(std::string(STR, l1) == std::string(sb2._buffer, l1));

  CHECK(std::string(STR, l1) == std::string(sb1._buffer, l1));

  TRI_DestroyStringBuffer(&sb1);
  TRI_DestroyStringBuffer(&sb2);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_erase_frnt
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_erase_frnt") {
  size_t l;

  TRI_string_buffer_t sb;
  TRI_InitStringBuffer(&sb);
  TRI_AppendStringStringBuffer(&sb, Z_2_T);
  TRI_EraseFrontStringBuffer(&sb, 5);
  
  CHECK(strlen(Z_2_T) - 5 == TRI_LengthStringBuffer(&sb));

  l = STRLEN(sb._buffer);

  CHECK(std::string(F_2_T, l) == std::string(sb._buffer, l));

  TRI_EraseFrontStringBuffer(&sb, 15);
  CHECK((0UL) == TRI_LengthStringBuffer(&sb));
  
  CHECK(TRI_EmptyStringBuffer(&sb));

  TRI_DestroyStringBuffer(&sb);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_erase_frnt
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_erase_frnt2") {
  size_t l;

  TRI_string_buffer_t sb;
  TRI_InitStringBuffer(&sb);
  TRI_AppendStringStringBuffer(&sb, "abcdef");
  TRI_EraseFrontStringBuffer(&sb, 5);

  l = STRLEN(sb._buffer);

  CHECK((1UL) == l);
  CHECK((1UL) == TRI_LengthStringBuffer(&sb));
  CHECK(std::string("f") == sb._buffer);

  // clang 5.1 failes without the cast
  CHECK(((unsigned int) 'f') == (unsigned int) sb._buffer[0]);
  CHECK(((unsigned int) '\0') == (unsigned int) sb._buffer[1]);
  CHECK(((unsigned int) '\0') == (unsigned int) sb._buffer[2]);
  CHECK(((unsigned int) '\0') == (unsigned int) sb._buffer[3]);
  CHECK(((unsigned int) '\0') == (unsigned int) sb._buffer[4]);
  CHECK(((unsigned int) '\0') == (unsigned int) sb._buffer[5]);
  CHECK(((unsigned int) '\0') == (unsigned int) sb._buffer[6]);

  TRI_DestroyStringBuffer(&sb);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_erase_frnt
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_erase_frnt3") {
  size_t l, i;

  TRI_string_buffer_t sb;
  TRI_InitStringBuffer(&sb);
  for (i = 0;  i != 500;  ++i) {
    TRI_AppendCharStringBuffer(&sb, 'a');
  }
  TRI_EraseFrontStringBuffer(&sb, 1);

  l = STRLEN(sb._buffer);
  
  CHECK((499UL) == l);
  CHECK((499UL) == TRI_LengthStringBuffer(&sb));

  // clang 5.1 failes without the cast
  CHECK(((unsigned int) 'a') == (unsigned int) sb._buffer[498]);
  CHECK(((unsigned int) '\0') == (unsigned int) sb._buffer[499]);
  CHECK(((unsigned int) '\0') == (unsigned int) sb._buffer[500]);
  
  TRI_EraseFrontStringBuffer(&sb, 1);

  l = STRLEN(sb._buffer);
  
  CHECK((498UL) == l);
  CHECK((498UL) == TRI_LengthStringBuffer(&sb));

  // clang 5.1 failes without the cast
  CHECK(((unsigned int) 'a') == (unsigned int) sb._buffer[497]);
  CHECK(((unsigned int) '\0') == (unsigned int) sb._buffer[498]);
  CHECK(((unsigned int) '\0') == (unsigned int) sb._buffer[499]);
  CHECK(((unsigned int) '\0') == (unsigned int) sb._buffer[500]);
  
  TRI_EraseFrontStringBuffer(&sb, 1000);

  l = STRLEN(sb._buffer);
  
  CHECK((0UL) == l);
  CHECK((0UL) == TRI_LengthStringBuffer(&sb));

  // clang 5.1 failes without the cast
  CHECK(((unsigned int) '\0') == (unsigned int) sb._buffer[0]);
  CHECK(((unsigned int) '\0') == (unsigned int) sb._buffer[1]);
  CHECK(((unsigned int) '\0') == (unsigned int) sb._buffer[496]);
  CHECK(((unsigned int) '\0') == (unsigned int) sb._buffer[497]);
  CHECK(((unsigned int) '\0') == (unsigned int) sb._buffer[498]);
  CHECK(((unsigned int) '\0') == (unsigned int) sb._buffer[499]);
  CHECK(((unsigned int) '\0') == (unsigned int) sb._buffer[500]);

  TRI_DestroyStringBuffer(&sb);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_replace
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_replace") {
  size_t l;

  TRI_string_buffer_t sb;
  TRI_string_buffer_t sb2;

  TRI_InitStringBuffer(&sb);

  TRI_AppendStringStringBuffer(&sb, ABC_const);
  TRI_ReplaceStringStringBuffer(&sb, "REP", 3);
  
  l = STRLEN(sb._buffer);

  CHECK(std::string(REP, l) == std::string(sb._buffer, l));

  TRI_ReplaceStringStringBuffer(&sb, ABC_const, 1);
  l = STRLEN(sb._buffer);

  CHECK(std::string(AEP, l) == std::string(sb._buffer, l));

  TRI_ClearStringBuffer(&sb);
  TRI_AppendStringStringBuffer(&sb, ABC_const);

  TRI_InitStringBuffer(&sb2);
  TRI_AppendStringStringBuffer(&sb2, "REP");

  TRI_DestroyStringBuffer(&sb);
  TRI_DestroyStringBuffer(&sb2);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_smpl_utils
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_smpl_utils") {
  char const* a234 = "234";
  char const* a23412 = "23412";
  char const* a2341212125 = "23412-12.125";

  // these are built on prev. tested building blocks...
  TRI_string_buffer_t sb;
  TRI_InitStringBuffer(&sb);
  TRI_AppendInteger3StringBuffer(&sb, 123);

  CHECK(std::string(ONETWOTHREE, STRLEN(ONETWOTHREE)) == std::string(sb._buffer, STRLEN(sb._buffer)));

  TRI_ClearStringBuffer(&sb);
  TRI_AppendInteger3StringBuffer(&sb, 1234);

  CHECK(std::string(a234, STRLEN(a234)) == std::string(sb._buffer, STRLEN(sb._buffer)));
  
  TRI_AppendDoubleStringBuffer(&sb, 12.0);

  CHECK(std::string(a23412, STRLEN(a23412)) == std::string(sb._buffer, STRLEN(sb._buffer)));

  TRI_AppendDoubleStringBuffer(&sb, -12.125);

  CHECK(std::string(a2341212125, STRLEN(a2341212125)) == std::string(sb._buffer, STRLEN(sb._buffer)));

  TRI_DestroyStringBuffer(&sb);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_length
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_length") {
  TRI_string_buffer_t sb;

  TRI_InitStringBuffer(&sb);

  CHECK((0UL) == TRI_LengthStringBuffer(&sb));

  TRI_AppendStringStringBuffer(&sb, ONETWOTHREE);

  CHECK((strlen(ONETWOTHREE)) == TRI_LengthStringBuffer(&sb));

  TRI_AppendInt32StringBuffer(&sb, 123);

  CHECK((strlen(ONETWOTHREE) + 3) == TRI_LengthStringBuffer(&sb));

  TRI_DestroyStringBuffer(&sb);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_clear
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_clear") {
  TRI_string_buffer_t sb;

  TRI_InitStringBuffer(&sb);
  CHECK((0UL) == TRI_LengthStringBuffer(&sb));

  // clear an empty buffer
  TRI_ClearStringBuffer(&sb);
  CHECK((0UL) == TRI_LengthStringBuffer(&sb));

  TRI_AppendStringStringBuffer(&sb, "foo bar baz");
  CHECK((11UL) == TRI_LengthStringBuffer(&sb));

  const char* ptr = TRI_BeginStringBuffer(&sb);
  TRI_ClearStringBuffer(&sb);
  CHECK((0UL) == TRI_LengthStringBuffer(&sb));

  // buffer should still point to ptr
  CHECK(((void*) ptr) == (void*) TRI_BeginStringBuffer(&sb));

  TRI_DestroyStringBuffer(&sb);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_steal
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_steal") {
  TRI_string_buffer_t sb;

  TRI_InitStringBuffer(&sb);
  TRI_AppendStringStringBuffer(&sb, "foo bar baz");

  const char* ptr = TRI_BeginStringBuffer(&sb);
 
  // steal the buffer
  char* stolen = TRI_StealStringBuffer(&sb);
  
  // buffer is now empty
  CHECK((0UL) == TRI_LengthStringBuffer(&sb));
  CHECK(((void*) 0) == (void*) TRI_BeginStringBuffer(&sb));

  // stolen should still point to ptr
  CHECK(((void*) stolen) == (void*) ptr);
  CHECK((0) == strcmp(stolen, ptr));

  TRI_DestroyStringBuffer(&sb);

  // destroying the string buffer should not affect us
  CHECK(((void*) stolen) == (void*) ptr);
  CHECK((0) == strcmp(stolen, ptr));

  // must manually free the string
  TRI_Free(stolen);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_last_char
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_last_char") {
  TRI_string_buffer_t sb;

  TRI_InitStringBuffer(&sb);

  TRI_AppendStringStringBuffer(&sb, "f");
  CHECK(((unsigned int) 'f') == (unsigned int) TRI_LastCharStringBuffer(&sb));

  TRI_AppendCharStringBuffer(&sb, '1');
  CHECK(((unsigned int) '1') == (unsigned int) TRI_LastCharStringBuffer(&sb));
  
  TRI_AppendCharStringBuffer(&sb, '\n');
  CHECK(((unsigned int) '\n') == (unsigned int) TRI_LastCharStringBuffer(&sb));

  TRI_ClearStringBuffer(&sb);
  CHECK((0UL) == TRI_LengthStringBuffer(&sb));
  
  for (size_t i = 0; i < 100; ++i) {
    TRI_AppendStringStringBuffer(&sb, "the quick brown fox jumped over the lazy dog");
    CHECK(((unsigned int) 'g') == (unsigned int) TRI_LastCharStringBuffer(&sb));
  }
  TRI_AppendCharStringBuffer(&sb, '.');
  CHECK(((unsigned int) '.') == (unsigned int) TRI_LastCharStringBuffer(&sb));
  
  TRI_AnnihilateStringBuffer(&sb);

  TRI_DestroyStringBuffer(&sb);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_reserve
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_reserve") {
  TRI_string_buffer_t sb;

  TRI_InitStringBuffer(&sb);
  CHECK((0UL) == TRI_LengthStringBuffer(&sb));
 
  TRI_ReserveStringBuffer(&sb, 0);
  CHECK((0UL) == TRI_LengthStringBuffer(&sb));

  TRI_ReserveStringBuffer(&sb, 1000);
  CHECK((0UL) == TRI_LengthStringBuffer(&sb));

  TRI_AppendStringStringBuffer(&sb, "f");
  CHECK((1UL) == TRI_LengthStringBuffer(&sb));

  for (size_t i = 0; i < 5000; ++i) {
    TRI_AppendCharStringBuffer(&sb, '.');
  }
  CHECK((5001UL) == TRI_LengthStringBuffer(&sb));
  
  TRI_ReserveStringBuffer(&sb, 1000);
  CHECK((5001UL) == TRI_LengthStringBuffer(&sb));

  TRI_DestroyStringBuffer(&sb);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_timing
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_timing") {
  char buffer[1024];
  size_t const repeats = 100;

  size_t const loop1 =  25 * 10000;
  size_t const loop2 = 200 * 10000;

  TRI_string_buffer_t sb;
  size_t i;
  size_t j;

  double t1 = TRI_microtime();

  // integer
  for (j = 0;  j < repeats;  ++j) {
    TRI_InitStringBuffer(&sb);

    for (i = 0;  i < loop1;  ++i) {
      TRI_AppendInt32StringBuffer(&sb, 12345678);
    }

    CHECK((loop1 * 8) == TRI_LengthStringBuffer(&sb));

    TRI_DestroyStringBuffer(&sb);
  }

  t1 = TRI_microtime() - t1;

  snprintf(buffer, sizeof(buffer), "time for integer append: %f msec", t1 * 1000);

  // character
  t1 = TRI_microtime();

  for (j = 0;  j < repeats;  ++j) {
    TRI_InitStringBuffer(&sb);

    for (i = 0;  i < loop2;  ++i) {
      TRI_AppendCharStringBuffer(&sb, 'A');
    }

    CHECK((loop2) == TRI_LengthStringBuffer(&sb));

    TRI_DestroyStringBuffer(&sb);
  }

  t1 = TRI_microtime() - t1;

  snprintf(buffer, sizeof(buffer), "time for character append: %f msec", t1 * 1000);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_doubles
////////////////////////////////////////////////////////////////////////////////

// try to turn off compiler warning for deliberate division by zero
SECTION("tst_doubles") {
  TRI_string_buffer_t sb;
  double value;
  
  TRI_InitStringBuffer(&sb);

  // + inf
  value = HUGE_VAL;
  TRI_AppendDoubleStringBuffer(&sb, value);
  CHECK(std::string("inf") == sb._buffer);

  // - inf
  value = -HUGE_VAL;
  TRI_ClearStringBuffer(&sb);
  TRI_AppendDoubleStringBuffer(&sb, value);
  CHECK(std::string("-inf") == sb._buffer);
  
  value = INFINITY;

  TRI_ClearStringBuffer(&sb);
  TRI_AppendDoubleStringBuffer(&sb, value);
  CHECK(std::string("inf") == sb._buffer);
  
#ifdef NAN  
  // NaN
  value = NAN;
  TRI_ClearStringBuffer(&sb);
  TRI_AppendDoubleStringBuffer(&sb, value);
  CHECK(std::string("NaN") == sb._buffer);
#endif
  
  // big numbers, hopefully this is portable enough
  double n = 244536.0;
  value = n * n * n * n;
  TRI_ClearStringBuffer(&sb);
  TRI_AppendDoubleStringBuffer(&sb, value);
  CHECK(std::string("3575783498001355400000") == sb._buffer);

  value *= -1.0;
  TRI_ClearStringBuffer(&sb);
  TRI_AppendDoubleStringBuffer(&sb, value);
  CHECK(std::string("-3575783498001355400000") == sb._buffer);

  TRI_DestroyStringBuffer(&sb);
}

}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
