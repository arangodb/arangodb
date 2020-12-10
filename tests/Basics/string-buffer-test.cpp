////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Tim Becker
/// @author Dr. Frank Celler
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include <cmath>

#include "Basics/Common.h"

#include "gtest/gtest.h"

#include "Basics/StringBuffer.h"

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
/// @brief tst_str_append
////////////////////////////////////////////////////////////////////////////////

TEST(CStringBufferTest, tst_str_append) {
  size_t l1, l2;

  TRI_string_buffer_t sb;
  TRI_InitStringBuffer(&sb);

  TRI_AppendStringStringBuffer(&sb, STR);
  TRI_AppendStringStringBuffer(&sb, STR);

  l1 = STRLEN(STRSTR);
  l2 = STRLEN(sb._buffer);
  
  EXPECT_TRUE(l1 == l2);

  EXPECT_TRUE(std::string(STRSTR, l1) == std::string(sb._buffer, l2));
  
  TRI_AppendString2StringBuffer(&sb, ABC_const, 3); // ABC_const ... Z

  l2 = STRLEN(sb._buffer);
  
  EXPECT_TRUE(std::string(STRSTRABC_const, l2) == std::string(sb._buffer, l2));

  TRI_ClearStringBuffer(&sb);
  TRI_AppendStringStringBuffer(&sb, STR);

  l2 = STRLEN(sb._buffer);

  EXPECT_TRUE(std::string(STRSTR, l2) == std::string(sb._buffer, l2));

  TRI_DestroyStringBuffer(&sb);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_char_append
////////////////////////////////////////////////////////////////////////////////

TEST(CStringBufferTest, tst_char_append) {
  size_t l1, l2, i;

  TRI_string_buffer_t sb;
  TRI_InitStringBuffer(&sb);
  
  for (i = 0;  i != 20;  ++i) {
    TRI_AppendCharStringBuffer(&sb, 'a');
  }

  l1 = STRLEN(TWNTYA);
  l2 = STRLEN(sb._buffer);
  
  EXPECT_TRUE(l1 == l2);
  
  EXPECT_TRUE(std::string(TWNTYA, l1) == std::string(sb._buffer, l2));

  TRI_DestroyStringBuffer(&sb);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_swp
////////////////////////////////////////////////////////////////////////////////

TEST(CStringBufferTest, tst_swp) {
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
  
  EXPECT_TRUE(std::string(TWNTYA, l1) == std::string(sb2._buffer, l1));

  EXPECT_TRUE(std::string(STR, l2) == std::string(sb1._buffer, l2));

  TRI_DestroyStringBuffer(&sb1);
  TRI_DestroyStringBuffer(&sb2);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_begin_end_empty_clear
////////////////////////////////////////////////////////////////////////////////

TEST(CStringBufferTest, tst_begin_end_empty_clear) {
  size_t l1;
  const char * ptr;

  TRI_string_buffer_t sb;
  TRI_InitStringBuffer(&sb);
  
  TRI_AppendStringStringBuffer(&sb, STR);
  
  ptr = TRI_BeginStringBuffer(&sb);

  EXPECT_TRUE(sb._buffer == ptr);

  l1 = STRLEN(STR);
  ptr = TRI_EndStringBuffer(&sb);

  EXPECT_TRUE(sb._buffer + l1 == ptr);

  EXPECT_TRUE(! TRI_EmptyStringBuffer(&sb));

  TRI_ClearStringBuffer(&sb);

  EXPECT_TRUE(TRI_EmptyStringBuffer(&sb));

  TRI_DestroyStringBuffer(&sb);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_cpy
////////////////////////////////////////////////////////////////////////////////

TEST(CStringBufferTest, tst_cpy) {
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

  EXPECT_TRUE(l1 == STRLEN(sb1._buffer));

  EXPECT_TRUE(std::string(STR, l1) == std::string(sb2._buffer, l1));

  EXPECT_TRUE(std::string(STR, l1) == std::string(sb1._buffer, l1));

  TRI_DestroyStringBuffer(&sb1);
  TRI_DestroyStringBuffer(&sb2);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_erase_frnt
////////////////////////////////////////////////////////////////////////////////

TEST(CStringBufferTest, tst_erase_frnt) {
  size_t l;

  TRI_string_buffer_t sb;
  TRI_InitStringBuffer(&sb);
  TRI_AppendStringStringBuffer(&sb, Z_2_T);
  TRI_EraseFrontStringBuffer(&sb, 5);
  
  EXPECT_TRUE(strlen(Z_2_T) - 5 == TRI_LengthStringBuffer(&sb));

  l = STRLEN(sb._buffer);

  EXPECT_TRUE(std::string(F_2_T, l) == std::string(sb._buffer, l));

  TRI_EraseFrontStringBuffer(&sb, 15);
  EXPECT_TRUE((0UL) == TRI_LengthStringBuffer(&sb));
  
  EXPECT_TRUE(TRI_EmptyStringBuffer(&sb));

  TRI_DestroyStringBuffer(&sb);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_erase_frnt
////////////////////////////////////////////////////////////////////////////////

TEST(CStringBufferTest, tst_erase_frnt2) {
  size_t l;

  TRI_string_buffer_t sb;
  TRI_InitStringBuffer(&sb);
  TRI_AppendStringStringBuffer(&sb, "abcdef");
  TRI_EraseFrontStringBuffer(&sb, 5);

  l = STRLEN(sb._buffer);

  EXPECT_TRUE(1UL == l);
  EXPECT_TRUE(1UL == TRI_LengthStringBuffer(&sb));
  EXPECT_TRUE(std::string("f") == sb._buffer);

  // clang 5.1 failes without the cast
  EXPECT_TRUE('f' == sb._buffer[0]);
  EXPECT_TRUE('\0' == sb._buffer[1]);
  EXPECT_TRUE('\0' == sb._buffer[2]);
  EXPECT_TRUE('\0' == sb._buffer[3]);
  EXPECT_TRUE('\0' == sb._buffer[4]);
  EXPECT_TRUE('\0' == sb._buffer[5]);
  EXPECT_TRUE('\0' == sb._buffer[6]);

  TRI_DestroyStringBuffer(&sb);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_erase_frnt
////////////////////////////////////////////////////////////////////////////////

TEST(CStringBufferTest, tst_erase_frnt3) {
  size_t l, i;

  TRI_string_buffer_t sb;
  TRI_InitStringBuffer(&sb);
  for (i = 0;  i != 500;  ++i) {
    TRI_AppendCharStringBuffer(&sb, 'a');
  }
  TRI_EraseFrontStringBuffer(&sb, 1);

  l = STRLEN(sb._buffer);
  
  EXPECT_TRUE((499UL) == l);
  EXPECT_TRUE((499UL) == TRI_LengthStringBuffer(&sb));

  // clang 5.1 failes without the cast
  EXPECT_TRUE(((unsigned int) 'a') == (unsigned int) sb._buffer[498]);
  EXPECT_TRUE(((unsigned int) '\0') == (unsigned int) sb._buffer[499]);
  EXPECT_TRUE(((unsigned int) '\0') == (unsigned int) sb._buffer[500]);
  
  TRI_EraseFrontStringBuffer(&sb, 1);

  l = STRLEN(sb._buffer);
  
  EXPECT_TRUE((498UL) == l);
  EXPECT_TRUE((498UL) == TRI_LengthStringBuffer(&sb));

  // clang 5.1 failes without the cast
  EXPECT_TRUE(((unsigned int) 'a') == (unsigned int) sb._buffer[497]);
  EXPECT_TRUE(((unsigned int) '\0') == (unsigned int) sb._buffer[498]);
  EXPECT_TRUE(((unsigned int) '\0') == (unsigned int) sb._buffer[499]);
  EXPECT_TRUE(((unsigned int) '\0') == (unsigned int) sb._buffer[500]);
  
  TRI_EraseFrontStringBuffer(&sb, 1000);

  l = STRLEN(sb._buffer);
  
  EXPECT_TRUE((0UL) == l);
  EXPECT_TRUE((0UL) == TRI_LengthStringBuffer(&sb));

  // clang 5.1 failes without the cast
  EXPECT_TRUE(((unsigned int) '\0') == (unsigned int) sb._buffer[0]);
  EXPECT_TRUE(((unsigned int) '\0') == (unsigned int) sb._buffer[1]);
  EXPECT_TRUE(((unsigned int) '\0') == (unsigned int) sb._buffer[496]);
  EXPECT_TRUE(((unsigned int) '\0') == (unsigned int) sb._buffer[497]);
  EXPECT_TRUE(((unsigned int) '\0') == (unsigned int) sb._buffer[498]);
  EXPECT_TRUE(((unsigned int) '\0') == (unsigned int) sb._buffer[499]);
  EXPECT_TRUE(((unsigned int) '\0') == (unsigned int) sb._buffer[500]);

  TRI_DestroyStringBuffer(&sb);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_replace
////////////////////////////////////////////////////////////////////////////////

TEST(CStringBufferTest, tst_replace) {
  size_t l;

  TRI_string_buffer_t sb;
  TRI_string_buffer_t sb2;

  TRI_InitStringBuffer(&sb);

  TRI_AppendStringStringBuffer(&sb, ABC_const);
  TRI_ReplaceStringStringBuffer(&sb, "REP", 3);
  
  l = STRLEN(sb._buffer);

  EXPECT_TRUE(std::string(REP, l) == std::string(sb._buffer, l));

  TRI_ReplaceStringStringBuffer(&sb, ABC_const, 1);
  l = STRLEN(sb._buffer);

  EXPECT_TRUE(std::string(AEP, l) == std::string(sb._buffer, l));

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

TEST(CStringBufferTest, tst_smpl_utils) {
  // these are built on prev. tested building blocks...
  TRI_string_buffer_t sb;
  TRI_InitStringBuffer(&sb);
  
  TRI_AppendDoubleStringBuffer(&sb, 12.0);

  EXPECT_TRUE("12" == std::string(sb._buffer, STRLEN(sb._buffer)));

  TRI_AppendDoubleStringBuffer(&sb, -12.125);

  EXPECT_TRUE("12-12.125" == std::string(sb._buffer, STRLEN(sb._buffer)));

  TRI_DestroyStringBuffer(&sb);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_length
////////////////////////////////////////////////////////////////////////////////

TEST(CStringBufferTest, tst_length) {
  TRI_string_buffer_t sb;

  TRI_InitStringBuffer(&sb);

  EXPECT_TRUE(0UL == TRI_LengthStringBuffer(&sb));

  TRI_AppendStringStringBuffer(&sb, ONETWOTHREE);

  EXPECT_TRUE((strlen(ONETWOTHREE)) == TRI_LengthStringBuffer(&sb));

  TRI_AppendInt32StringBuffer(&sb, 123);

  EXPECT_TRUE((strlen(ONETWOTHREE) + 3) == TRI_LengthStringBuffer(&sb));

  TRI_DestroyStringBuffer(&sb);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_clear
////////////////////////////////////////////////////////////////////////////////

TEST(CStringBufferTest, tst_clear) {
  TRI_string_buffer_t sb;

  TRI_InitStringBuffer(&sb);
  EXPECT_TRUE(0UL == TRI_LengthStringBuffer(&sb));

  // clear an empty buffer
  TRI_ClearStringBuffer(&sb);
  EXPECT_TRUE(0UL == TRI_LengthStringBuffer(&sb));

  TRI_AppendStringStringBuffer(&sb, "foo bar baz");
  EXPECT_TRUE(11UL == TRI_LengthStringBuffer(&sb));

  const char* ptr = TRI_BeginStringBuffer(&sb);
  TRI_ClearStringBuffer(&sb);
  EXPECT_TRUE(0UL == TRI_LengthStringBuffer(&sb));

  // buffer should still point to ptr
  EXPECT_TRUE(ptr == TRI_BeginStringBuffer(&sb));

  TRI_DestroyStringBuffer(&sb);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_steal
////////////////////////////////////////////////////////////////////////////////

TEST(CStringBufferTest, tst_steal) {
  TRI_string_buffer_t sb;

  TRI_InitStringBuffer(&sb);
  TRI_AppendStringStringBuffer(&sb, "foo bar baz");

  const char* ptr = TRI_BeginStringBuffer(&sb);
 
  // steal the buffer
  char* stolen = TRI_StealStringBuffer(&sb);
  
  // buffer is now empty
  EXPECT_TRUE(0UL == TRI_LengthStringBuffer(&sb));
  EXPECT_TRUE(nullptr == TRI_BeginStringBuffer(&sb));

  // stolen should still point to ptr
  EXPECT_TRUE(stolen == ptr);
  EXPECT_TRUE(0 == strcmp(stolen, ptr));

  TRI_DestroyStringBuffer(&sb);

  // destroying the string buffer should not affect us
  EXPECT_TRUE(stolen == ptr);
  EXPECT_TRUE(0 == strcmp(stolen, ptr));

  // must manually free the string
  TRI_Free(stolen);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_last_char
////////////////////////////////////////////////////////////////////////////////

TEST(CStringBufferTest, tst_last_char) {
  TRI_string_buffer_t sb;

  TRI_InitStringBuffer(&sb);

  TRI_AppendStringStringBuffer(&sb, "f");
  EXPECT_TRUE('f' == TRI_LastCharStringBuffer(&sb));

  TRI_AppendCharStringBuffer(&sb, '1');
  EXPECT_TRUE('1' == TRI_LastCharStringBuffer(&sb));
  
  TRI_AppendCharStringBuffer(&sb, '\n');
  EXPECT_TRUE('\n' == TRI_LastCharStringBuffer(&sb));

  TRI_ClearStringBuffer(&sb);
  EXPECT_TRUE(0UL == TRI_LengthStringBuffer(&sb));
  
  for (size_t i = 0; i < 100; ++i) {
    TRI_AppendStringStringBuffer(&sb, "the quick brown fox jumped over the lazy dog");
    EXPECT_TRUE('g' == TRI_LastCharStringBuffer(&sb));
  }
  TRI_AppendCharStringBuffer(&sb, '.');
  EXPECT_TRUE('.' == TRI_LastCharStringBuffer(&sb));
  
  TRI_AnnihilateStringBuffer(&sb);

  TRI_DestroyStringBuffer(&sb);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_reserve
////////////////////////////////////////////////////////////////////////////////

TEST(CStringBufferTest, tst_reserve) {
  TRI_string_buffer_t sb;

  TRI_InitStringBuffer(&sb);
  EXPECT_TRUE(0UL == TRI_LengthStringBuffer(&sb));
 
  TRI_ReserveStringBuffer(&sb, 0);
  EXPECT_TRUE(0UL == TRI_LengthStringBuffer(&sb));

  TRI_ReserveStringBuffer(&sb, 1000);
  EXPECT_TRUE(0UL == TRI_LengthStringBuffer(&sb));

  TRI_AppendStringStringBuffer(&sb, "f");
  EXPECT_TRUE(1UL == TRI_LengthStringBuffer(&sb));

  for (size_t i = 0; i < 5000; ++i) {
    TRI_AppendCharStringBuffer(&sb, '.');
  }
  EXPECT_TRUE(5001UL == TRI_LengthStringBuffer(&sb));
  
  TRI_ReserveStringBuffer(&sb, 1000);
  EXPECT_TRUE(5001UL == TRI_LengthStringBuffer(&sb));

  TRI_DestroyStringBuffer(&sb);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tst_doubles
////////////////////////////////////////////////////////////////////////////////

// try to turn off compiler warning for deliberate division by zero
TEST(CStringBufferTest, tst_doubles) {
  TRI_string_buffer_t sb;
  double value;
  
  TRI_InitStringBuffer(&sb);

  // + inf
  value = HUGE_VAL;
  TRI_AppendDoubleStringBuffer(&sb, value);
  EXPECT_TRUE(std::string("inf") == sb._buffer);

  // - inf
  value = -HUGE_VAL;
  TRI_ClearStringBuffer(&sb);
  TRI_AppendDoubleStringBuffer(&sb, value);
  EXPECT_TRUE(std::string("-inf") == sb._buffer);
  
  value = INFINITY;

  TRI_ClearStringBuffer(&sb);
  TRI_AppendDoubleStringBuffer(&sb, value);
  EXPECT_TRUE(std::string("inf") == sb._buffer);
  
#ifdef NAN  
  // NaN
  value = NAN;
  TRI_ClearStringBuffer(&sb);
  TRI_AppendDoubleStringBuffer(&sb, value);
  EXPECT_TRUE(std::string("NaN") == sb._buffer);
#endif
  
  // big numbers, hopefully this is portable enough
  double n = 244536.0;
  value = n * n * n * n;
  TRI_ClearStringBuffer(&sb);
  TRI_AppendDoubleStringBuffer(&sb, value);
  EXPECT_TRUE(std::string("3575783498001355400000") == sb._buffer);

  value *= -1.0;
  TRI_ClearStringBuffer(&sb);
  TRI_AppendDoubleStringBuffer(&sb, value);
  EXPECT_TRUE(std::string("-3575783498001355400000") == sb._buffer);

  TRI_DestroyStringBuffer(&sb);
}
