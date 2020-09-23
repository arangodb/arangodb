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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#include "gtest/gtest.h"

#include "Basics/vector.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                    private macros
// -----------------------------------------------------------------------------

#define VECTOR_INIT \
  TRI_vector_t v1; \
  TRI_InitVector(&v1, sizeof(int));

#define VECTOR_DESTROY \
  TRI_DestroyVector(&v1);

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test length after vector initialization
////////////////////////////////////////////////////////////////////////////////

TEST(CVectorTest, tst_length_init) {
  VECTOR_INIT

  EXPECT_EQ(0U, TRI_LengthVector(&v1));

  VECTOR_DESTROY
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test vector length after insertions
////////////////////////////////////////////////////////////////////////////////

TEST(CVectorTest, tst_length_insert) {
  VECTOR_INIT

  int p1 = 1;
  int p2 = 2;
  TRI_PushBackVector(&v1, &p1);
  EXPECT_EQ(1U, TRI_LengthVector(&v1));

  TRI_PushBackVector(&v1, &p1);
  EXPECT_EQ(2U, TRI_LengthVector(&v1));
  
  TRI_PushBackVector(&v1, &p2);
  EXPECT_EQ(3U, TRI_LengthVector(&v1));

  VECTOR_DESTROY
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test vector length after insertions & deletions
////////////////////////////////////////////////////////////////////////////////

TEST(CVectorTest, tst_length_insert_remove) {
  VECTOR_INIT

  int p1 = 1;
  int p2 = 2;
  int p3 = 3;
  TRI_PushBackVector(&v1, &p1);
  EXPECT_EQ(1U, TRI_LengthVector(&v1));

  TRI_RemoveVector(&v1, 0);
  EXPECT_EQ(0U, TRI_LengthVector(&v1));
  
  TRI_PushBackVector(&v1, &p2);
  TRI_PushBackVector(&v1, &p3);
  EXPECT_EQ(2U, TRI_LengthVector(&v1));
  
  TRI_RemoveVector(&v1, 0);
  EXPECT_EQ(1U, TRI_LengthVector(&v1));

  TRI_RemoveVector(&v1, 0);
  EXPECT_EQ(0U, TRI_LengthVector(&v1));

  VECTOR_DESTROY 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test removal of elements at invalid positions
////////////////////////////////////////////////////////////////////////////////

TEST(CVectorTest, tst_remove_invalid1) {
  VECTOR_INIT

  TRI_RemoveVector(&v1, 0); // invalid position
  TRI_RemoveVector(&v1, 0); // invalid position
  TRI_RemoveVector(&v1, 1); // invalid position
  TRI_RemoveVector(&v1, -1); // invalid position
  TRI_RemoveVector(&v1, 99); // invalid position
  
  EXPECT_EQ(0U, TRI_LengthVector(&v1));

  VECTOR_DESTROY 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test removal of elements at invalid positions
////////////////////////////////////////////////////////////////////////////////

TEST(CVectorTest, tst_remove_invalid2) {
  VECTOR_INIT
  
  int p1 = 1;
  int p2 = 2;

  TRI_PushBackVector(&v1, &p1);
  TRI_PushBackVector(&v1, &p2);

  TRI_RemoveVector(&v1, 0); // valid
  TRI_RemoveVector(&v1, 0); // valid
  TRI_RemoveVector(&v1, 0); // now invalid
  
  TRI_RemoveVector(&v1, 1); // invalid position
  TRI_RemoveVector(&v1, -1); // invalid position
  TRI_RemoveVector(&v1, 99); // invalid position

  VECTOR_DESTROY 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test at
////////////////////////////////////////////////////////////////////////////////

TEST(CVectorTest, tst_at_empty) {
  VECTOR_INIT

  void* r = nullptr;
  
  EXPECT_EQ(r, TRI_AtVector(&v1, 0));
  EXPECT_EQ(r, TRI_AtVector(&v1, 1));
  EXPECT_EQ(r, TRI_AtVector(&v1, -1));
  EXPECT_EQ(r, TRI_AtVector(&v1, 99));

  VECTOR_DESTROY 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test at and insert
////////////////////////////////////////////////////////////////////////////////

TEST(CVectorTest, tst_at_insert) {
  VECTOR_INIT
 
  int a = 1; 
  int b = 2; 
  int c = 3;
  int d = 4;
   
  TRI_PushBackVector(&v1, &a);
  EXPECT_EQ(1, *(int*) TRI_AtVector(&v1, 0));

  TRI_PushBackVector(&v1, &b);
  EXPECT_EQ(1, *(int*) TRI_AtVector(&v1, 0));
  EXPECT_EQ(2, *(int*) TRI_AtVector(&v1, 1));

  TRI_PushBackVector(&v1, &c);
  EXPECT_EQ(1, *(int*) TRI_AtVector(&v1, 0));
  EXPECT_EQ(2, *(int*) TRI_AtVector(&v1, 1));
  EXPECT_EQ(3, *(int*) TRI_AtVector(&v1, 2));

  TRI_PushBackVector(&v1, &d);
  EXPECT_EQ(1, *(int*) TRI_AtVector(&v1, 0));
  EXPECT_EQ(2, *(int*) TRI_AtVector(&v1, 1));
  EXPECT_EQ(3, *(int*) TRI_AtVector(&v1, 2));
  EXPECT_EQ(4, *(int*) TRI_AtVector(&v1, 3));

  VECTOR_DESTROY 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test at and insert and remove
////////////////////////////////////////////////////////////////////////////////

TEST(CVectorTest, tst_at_insert_remove) {
  VECTOR_INIT

  void* r = nullptr;
  int a = 1;
  int b = 2;
  int c = 3;
  int d = 4;
  
  TRI_PushBackVector(&v1, &a);
  EXPECT_EQ(1, *(int*) TRI_AtVector(&v1, 0));

  TRI_RemoveVector(&v1, 0);
  EXPECT_EQ(r, TRI_AtVector(&v1, 0));

  TRI_PushBackVector(&v1, &b);
  EXPECT_EQ(2, *(int*) TRI_AtVector(&v1, 0));

  TRI_PushBackVector(&v1, &c);
  EXPECT_EQ(2, *(int*) TRI_AtVector(&v1, 0));
  EXPECT_EQ(3, *(int*) TRI_AtVector(&v1, 1));
  
  TRI_RemoveVector(&v1, 0);
  EXPECT_EQ(3, *(int*) TRI_AtVector(&v1, 0));
  
  TRI_PushBackVector(&v1, &d);
  TRI_PushBackVector(&v1, &a);
  
  EXPECT_EQ(3, *(int*) TRI_AtVector(&v1, 0));
  EXPECT_EQ(4, *(int*) TRI_AtVector(&v1, 1));
  EXPECT_EQ(1, *(int*) TRI_AtVector(&v1, 2));

  TRI_RemoveVector(&v1, 1);
  EXPECT_EQ(3, *(int*) TRI_AtVector(&v1, 0));
  EXPECT_EQ(1, *(int*) TRI_AtVector(&v1, 1));

  VECTOR_DESTROY 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test duplicate pointers
////////////////////////////////////////////////////////////////////////////////

TEST(CVectorTest, tst_push_back_duplicate) {
  VECTOR_INIT
  int a = 1;
  int b = 2;

  TRI_PushBackVector(&v1, &a);
  TRI_PushBackVector(&v1, &a);
  TRI_PushBackVector(&v1, &a);
  TRI_PushBackVector(&v1, &a);
  TRI_PushBackVector(&v1, &b);
  TRI_PushBackVector(&v1, &b);

  EXPECT_EQ(6U, TRI_LengthVector(&v1));
  EXPECT_EQ(1, *(int*) TRI_AtVector(&v1, 0));
  EXPECT_EQ(1, *(int*) TRI_AtVector(&v1, 1));
  EXPECT_EQ(1, *(int*) TRI_AtVector(&v1, 2));
  EXPECT_EQ(1, *(int*) TRI_AtVector(&v1, 3));
  EXPECT_EQ(2, *(int*) TRI_AtVector(&v1, 4));
  EXPECT_EQ(2, *(int*) TRI_AtVector(&v1, 5));

  VECTOR_DESTROY
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test duplicate pointers
////////////////////////////////////////////////////////////////////////////////

TEST(CVectorTest, tst_remove_duplicate) {
  VECTOR_INIT
  int a = 1;
  int b = 2;

  TRI_PushBackVector(&v1, &a);
  TRI_PushBackVector(&v1, &a);
  TRI_PushBackVector(&v1, &a);
  TRI_PushBackVector(&v1, &b);
  TRI_PushBackVector(&v1, &b);

  EXPECT_EQ(5U, TRI_LengthVector(&v1));
  EXPECT_EQ(1, *(int*) TRI_AtVector(&v1, 0));
  EXPECT_EQ(1, *(int*) TRI_AtVector(&v1, 1));
  EXPECT_EQ(1, *(int*) TRI_AtVector(&v1, 2));
  EXPECT_EQ(2, *(int*) TRI_AtVector(&v1, 3));
  EXPECT_EQ(2, *(int*) TRI_AtVector(&v1, 4));
  
  TRI_RemoveVector(&v1, 4); 
  TRI_RemoveVector(&v1, 0); 
  TRI_RemoveVector(&v1, 1); 
  EXPECT_EQ(2U, TRI_LengthVector(&v1));
  EXPECT_EQ(1, *(int*) TRI_AtVector(&v1, 0));
  EXPECT_EQ(2, *(int*) TRI_AtVector(&v1, 1));

  VECTOR_DESTROY
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test push back and remove
////////////////////////////////////////////////////////////////////////////////

TEST(CVectorTest, tst_push_back_remove) {
  VECTOR_INIT

  int a = 1;
  int b = 2;
  int c = 3;
  int d = 4;

  TRI_PushBackVector(&v1, &a);
  TRI_PushBackVector(&v1, &b);
  TRI_PushBackVector(&v1, &c);
  TRI_PushBackVector(&v1, &d);
  TRI_PushBackVector(&v1, &a);
  TRI_PushBackVector(&v1, &b);
  TRI_PushBackVector(&v1, &c);
  TRI_PushBackVector(&v1, &d);
  TRI_PushBackVector(&v1, &a);
  TRI_PushBackVector(&v1, &a);

  EXPECT_EQ(10U, TRI_LengthVector(&v1));
  EXPECT_EQ(1, *(int*) TRI_AtVector(&v1, 0));
  EXPECT_EQ(2, *(int*) TRI_AtVector(&v1, 1));
  EXPECT_EQ(3, *(int*) TRI_AtVector(&v1, 2));
  EXPECT_EQ(4, *(int*) TRI_AtVector(&v1, 3));
  EXPECT_EQ(1, *(int*) TRI_AtVector(&v1, 4));
  EXPECT_EQ(2, *(int*) TRI_AtVector(&v1, 5));
  EXPECT_EQ(3, *(int*) TRI_AtVector(&v1, 6));
  EXPECT_EQ(4, *(int*) TRI_AtVector(&v1, 7));
  EXPECT_EQ(1, *(int*) TRI_AtVector(&v1, 8));
  EXPECT_EQ(1, *(int*) TRI_AtVector(&v1, 9));

  TRI_RemoveVector(&v1, 4); 
  EXPECT_EQ(9U, TRI_LengthVector(&v1));
  EXPECT_EQ(1, *(int*) TRI_AtVector(&v1, 0));
  EXPECT_EQ(2, *(int*) TRI_AtVector(&v1, 1));
  EXPECT_EQ(3, *(int*) TRI_AtVector(&v1, 2));
  EXPECT_EQ(4, *(int*) TRI_AtVector(&v1, 3));
  EXPECT_EQ(2, *(int*) TRI_AtVector(&v1, 4));
  EXPECT_EQ(3, *(int*) TRI_AtVector(&v1, 5));
  EXPECT_EQ(4, *(int*) TRI_AtVector(&v1, 6));
  EXPECT_EQ(1, *(int*) TRI_AtVector(&v1, 7));
  EXPECT_EQ(1, *(int*) TRI_AtVector(&v1, 8));
  
  TRI_RemoveVector(&v1, 0); 
  EXPECT_EQ(8U, TRI_LengthVector(&v1));
  EXPECT_EQ(2, *(int*) TRI_AtVector(&v1, 0));
  EXPECT_EQ(3, *(int*) TRI_AtVector(&v1, 1));
  EXPECT_EQ(4, *(int*) TRI_AtVector(&v1, 2));
  EXPECT_EQ(2, *(int*) TRI_AtVector(&v1, 3));
  EXPECT_EQ(3, *(int*) TRI_AtVector(&v1, 4));
  EXPECT_EQ(4, *(int*) TRI_AtVector(&v1, 5));
  EXPECT_EQ(1, *(int*) TRI_AtVector(&v1, 6));
  EXPECT_EQ(1, *(int*) TRI_AtVector(&v1, 7));
  
  TRI_RemoveVector(&v1, 7); 
  EXPECT_EQ(7U, TRI_LengthVector(&v1));
  EXPECT_EQ(2, *(int*) TRI_AtVector(&v1, 0));
  EXPECT_EQ(3, *(int*) TRI_AtVector(&v1, 1));
  EXPECT_EQ(4, *(int*) TRI_AtVector(&v1, 2));
  EXPECT_EQ(2, *(int*) TRI_AtVector(&v1, 3));
  EXPECT_EQ(3, *(int*) TRI_AtVector(&v1, 4));
  EXPECT_EQ(4, *(int*) TRI_AtVector(&v1, 5));
  EXPECT_EQ(1, *(int*) TRI_AtVector(&v1, 6));

  TRI_RemoveVector(&v1, 0); 
  TRI_RemoveVector(&v1, 0); 
  EXPECT_EQ(5U, TRI_LengthVector(&v1));
  EXPECT_EQ(4, *(int*) TRI_AtVector(&v1, 0));
  EXPECT_EQ(2, *(int*) TRI_AtVector(&v1, 1));
  EXPECT_EQ(3, *(int*) TRI_AtVector(&v1, 2));
  EXPECT_EQ(4, *(int*) TRI_AtVector(&v1, 3));
  EXPECT_EQ(1, *(int*) TRI_AtVector(&v1, 4));

  TRI_RemoveVector(&v1, 1); 
  TRI_RemoveVector(&v1, 1); 
  EXPECT_EQ(3U, TRI_LengthVector(&v1));
  EXPECT_EQ(4, *(int*) TRI_AtVector(&v1, 0));
  EXPECT_EQ(4, *(int*) TRI_AtVector(&v1, 1));
  EXPECT_EQ(1, *(int*) TRI_AtVector(&v1, 2));
  
  TRI_RemoveVector(&v1, 1); 
  EXPECT_EQ(2U, TRI_LengthVector(&v1));
  EXPECT_EQ(4, *(int*) TRI_AtVector(&v1, 0));
  EXPECT_EQ(1, *(int*) TRI_AtVector(&v1, 1));
  
  TRI_RemoveVector(&v1, 1); 
  TRI_RemoveVector(&v1, 0); 
  EXPECT_EQ(0U, TRI_LengthVector(&v1));

  VECTOR_DESTROY 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test set
////////////////////////////////////////////////////////////////////////////////

TEST(CVectorTest, tst_set) {
  VECTOR_INIT

  int a = 1;
  int b = 2;
  int c = 3;
  int d = 4;

  TRI_PushBackVector(&v1, &a);
  TRI_PushBackVector(&v1, &b);
  TRI_PushBackVector(&v1, &c);
  TRI_PushBackVector(&v1, &d);

  EXPECT_EQ(4U, TRI_LengthVector(&v1));
  EXPECT_EQ(1, *(int*) TRI_AtVector(&v1, 0));
  EXPECT_EQ(2, *(int*) TRI_AtVector(&v1, 1));
  EXPECT_EQ(3, *(int*) TRI_AtVector(&v1, 2));
  EXPECT_EQ(4, *(int*) TRI_AtVector(&v1, 3));
  EXPECT_EQ(1, *(int*) TRI_BeginVector(&v1));
  EXPECT_EQ(4, *((int*) (int*) TRI_BeginVector(&v1) + TRI_LengthVector(&v1) - 1));

  TRI_SetVector(&v1, 0, &d);
  TRI_SetVector(&v1, 1, &c);
  TRI_SetVector(&v1, 2, &b);
  TRI_SetVector(&v1, 3, &a);

  EXPECT_EQ(4U, TRI_LengthVector(&v1));
  EXPECT_EQ(4, *(int*) TRI_AtVector(&v1, 0));
  EXPECT_EQ(3, *(int*) TRI_AtVector(&v1, 1));
  EXPECT_EQ(2, *(int*) TRI_AtVector(&v1, 2));
  EXPECT_EQ(1, *(int*) TRI_AtVector(&v1, 3));
  EXPECT_EQ(4, *(int*) TRI_BeginVector(&v1));
  EXPECT_EQ(1, *((int*) (int*) TRI_BeginVector(&v1) + TRI_LengthVector(&v1) - 1));

  TRI_SetVector(&v1, 0, &b);
  EXPECT_EQ(2, *(int*) TRI_AtVector(&v1, 0));
  EXPECT_EQ(2, *(int*) TRI_BeginVector(&v1));

  VECTOR_DESTROY 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test modifications
////////////////////////////////////////////////////////////////////////////////

TEST(CVectorTest, tst_value_modifications) {
  VECTOR_INIT
  
  int a = 1;
  int b = 2;
  int c = 3;
  int d = 4;

  TRI_PushBackVector(&v1, &a);
  TRI_PushBackVector(&v1, &b);
  TRI_PushBackVector(&v1, &c);
  TRI_PushBackVector(&v1, &d);

  EXPECT_EQ(1, *(int*) TRI_AtVector(&v1, 0));
  EXPECT_EQ(2, *(int*) TRI_AtVector(&v1, 1));
  EXPECT_EQ(3, *(int*) TRI_AtVector(&v1, 2));
  EXPECT_EQ(4, *(int*) TRI_AtVector(&v1, 3));

  a = 99;
  b = 42;
  c = -1;
  d = 0;
  
  EXPECT_EQ(1, *(int*) TRI_AtVector(&v1, 0));
  EXPECT_EQ(2, *(int*) TRI_AtVector(&v1, 1));
  EXPECT_EQ(3, *(int*) TRI_AtVector(&v1, 2));
  EXPECT_EQ(4, *(int*) TRI_AtVector(&v1, 3));
  
  VECTOR_DESTROY 
}
