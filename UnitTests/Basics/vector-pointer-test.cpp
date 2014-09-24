////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for TRI_vector_pointer_t
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

#include "Basics/vector.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                    private macros
// -----------------------------------------------------------------------------

#define VECTOR_DATA \
  int p = 1; (void) p; \
  char s[30] = "test string"; (void) s;

#define VECTOR_DATA2 \
  int a = 1; (void) a; \
  int b = 2; (void) b; \
  int c = 3; (void) c; \
  int d = 4; (void) d; 

#define VECTOR_INIT \
  TRI_vector_pointer_t v1; \
  TRI_InitVectorPointer(&v1, TRI_CORE_MEM_ZONE);

#define VECTOR_DESTROY \
  TRI_DestroyVectorPointer(&v1);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private constants
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct CVectorPointerSetup {
  CVectorPointerSetup () {
    BOOST_TEST_MESSAGE("setup TRI_vector_pointer_t");
  }

  ~CVectorPointerSetup () {
    BOOST_TEST_MESSAGE("tear-down TRI_vector_pointer_t");
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_SUITE(CVectorPointerTest, CVectorPointerSetup)

////////////////////////////////////////////////////////////////////////////////
/// @brief test size
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_length_size) {
  BOOST_CHECK_EQUAL((size_t) 4 * sizeof(void*), sizeof(TRI_vector_pointer_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test length after vector initialisation
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_length_init) {
  VECTOR_INIT

  BOOST_CHECK_EQUAL((size_t) 0, v1._length);
  BOOST_CHECK_EQUAL(true, TRI_EmptyVectorPointer(&v1));

  VECTOR_DESTROY
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test vector length after insertions
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_length_insert) {
  VECTOR_INIT
  VECTOR_DATA

  BOOST_CHECK_EQUAL(true, TRI_EmptyVectorPointer(&v1));

  TRI_PushBackVectorPointer(&v1, &p);
  BOOST_CHECK_EQUAL((size_t) 1, v1._length);
  BOOST_CHECK_EQUAL(false, TRI_EmptyVectorPointer(&v1));

  TRI_PushBackVectorPointer(&v1, &p);
  BOOST_CHECK_EQUAL((size_t) 2, v1._length);
  BOOST_CHECK_EQUAL(false, TRI_EmptyVectorPointer(&v1));
  
  TRI_PushBackVectorPointer(&v1, s);
  BOOST_CHECK_EQUAL((size_t) 3, v1._length);
  BOOST_CHECK_EQUAL(false, TRI_EmptyVectorPointer(&v1));

  TRI_PushBackVectorPointer(&v1, s);
  BOOST_CHECK_EQUAL((size_t) 4, v1._length);
  BOOST_CHECK_EQUAL(false, TRI_EmptyVectorPointer(&v1));

  VECTOR_DESTROY
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test vector length after insertions & deletions
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_length_insert_remove) {
  VECTOR_INIT
  VECTOR_DATA

  BOOST_CHECK_EQUAL(true, TRI_EmptyVectorPointer(&v1));

  TRI_PushBackVectorPointer(&v1, &p);
  BOOST_CHECK_EQUAL((size_t) 1, v1._length);
  BOOST_CHECK_EQUAL(false, TRI_EmptyVectorPointer(&v1));

  TRI_RemoveVectorPointer(&v1, 0);
  BOOST_CHECK_EQUAL((size_t) 0, v1._length);
  BOOST_CHECK_EQUAL(true, TRI_EmptyVectorPointer(&v1));
  
  TRI_PushBackVectorPointer(&v1, s);
  TRI_PushBackVectorPointer(&v1, s);
  BOOST_CHECK_EQUAL((size_t) 2, v1._length);
  BOOST_CHECK_EQUAL(false, TRI_EmptyVectorPointer(&v1));
  
  TRI_RemoveVectorPointer(&v1, 0);
  BOOST_CHECK_EQUAL((size_t) 1, v1._length);
  BOOST_CHECK_EQUAL(false, TRI_EmptyVectorPointer(&v1));

  TRI_RemoveVectorPointer(&v1, 0);
  BOOST_CHECK_EQUAL((size_t) 0, v1._length);
  BOOST_CHECK_EQUAL(true, TRI_EmptyVectorPointer(&v1));

  VECTOR_DESTROY 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test vector length after clearing
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_length_clear) {
  VECTOR_INIT
  VECTOR_DATA

  BOOST_CHECK_EQUAL(true, TRI_EmptyVectorPointer(&v1));

  TRI_PushBackVectorPointer(&v1, &p);
  TRI_PushBackVectorPointer(&v1, s);
  TRI_PushBackVectorPointer(&v1, s);
  BOOST_CHECK_EQUAL((size_t) 3, v1._length);
 
  TRI_ClearVectorPointer(&v1); 
  BOOST_CHECK_EQUAL((size_t) 0, v1._length);
  BOOST_CHECK_EQUAL(true, TRI_EmptyVectorPointer(&v1));

  TRI_PushBackVectorPointer(&v1, s);
  BOOST_CHECK_EQUAL((size_t) 1, v1._length);
  BOOST_CHECK_EQUAL(false, TRI_EmptyVectorPointer(&v1));

  VECTOR_DESTROY 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test removal of elements at invalid positions
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_remove_invalid1) {
  VECTOR_INIT

  void* r = 0;

  BOOST_CHECK_EQUAL(r, TRI_RemoveVectorPointer(&v1, 0)); // invalid position
  BOOST_CHECK_EQUAL(r, TRI_RemoveVectorPointer(&v1, 0)); // invalid position
  BOOST_CHECK_EQUAL(r, TRI_RemoveVectorPointer(&v1, 1)); // invalid position
  BOOST_CHECK_EQUAL(r, TRI_RemoveVectorPointer(&v1, -1)); // invalid position
  BOOST_CHECK_EQUAL(r, TRI_RemoveVectorPointer(&v1, 99)); // invalid position
  
  BOOST_CHECK_EQUAL((size_t) 0, v1._length);
  BOOST_CHECK_EQUAL(true, TRI_EmptyVectorPointer(&v1));

  VECTOR_DESTROY 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test removal of elements at invalid positions
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_remove_invalid2) {
  VECTOR_INIT
  VECTOR_DATA
  
  void* r = 0;

  TRI_PushBackVectorPointer(&v1, &p);
  TRI_PushBackVectorPointer(&v1, s);

  BOOST_CHECK_EQUAL(&p, TRI_RemoveVectorPointer(&v1, 0)); // valid
  BOOST_CHECK_EQUAL(s, TRI_RemoveVectorPointer(&v1, 0)); // valid
  BOOST_CHECK_EQUAL(r, TRI_RemoveVectorPointer(&v1, 0)); // now invalid
  
  BOOST_CHECK_EQUAL(r, TRI_RemoveVectorPointer(&v1, 1)); // invalid position
  BOOST_CHECK_EQUAL(r, TRI_RemoveVectorPointer(&v1, -1)); // invalid position
  BOOST_CHECK_EQUAL(r, TRI_RemoveVectorPointer(&v1, 99)); // invalid position

  VECTOR_DESTROY 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test at
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_at_empty) {
  VECTOR_INIT

  void* r = 0;
  
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, 0));
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, 1));
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, -1));
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, 99));

  VECTOR_DESTROY 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test at and insert
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_at_insert) {
  VECTOR_INIT
  VECTOR_DATA2
  
  TRI_PushBackVectorPointer(&v1, &a);
  BOOST_CHECK_EQUAL(&a, TRI_AtVectorPointer(&v1, 0));

  TRI_PushBackVectorPointer(&v1, &b);
  BOOST_CHECK_EQUAL(&a, TRI_AtVectorPointer(&v1, 0));
  BOOST_CHECK_EQUAL(&b, TRI_AtVectorPointer(&v1, 1));

  TRI_PushBackVectorPointer(&v1, &c);
  BOOST_CHECK_EQUAL(&a, TRI_AtVectorPointer(&v1, 0));
  BOOST_CHECK_EQUAL(&b, TRI_AtVectorPointer(&v1, 1));
  BOOST_CHECK_EQUAL(&c, TRI_AtVectorPointer(&v1, 2));

  TRI_PushBackVectorPointer(&v1, &d);
  BOOST_CHECK_EQUAL(&a, TRI_AtVectorPointer(&v1, 0));
  BOOST_CHECK_EQUAL(&b, TRI_AtVectorPointer(&v1, 1));
  BOOST_CHECK_EQUAL(&c, TRI_AtVectorPointer(&v1, 2));
  BOOST_CHECK_EQUAL(&d, TRI_AtVectorPointer(&v1, 3));

  VECTOR_DESTROY 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test at and insert and remove
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_at_insert_remove) {
  VECTOR_INIT
  VECTOR_DATA2

  void* r = 0;
  
  TRI_PushBackVectorPointer(&v1, &a);
  BOOST_CHECK_EQUAL(&a, TRI_AtVectorPointer(&v1, 0));

  TRI_RemoveVectorPointer(&v1, 0);
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, 0));

  TRI_PushBackVectorPointer(&v1, &b);
  BOOST_CHECK_EQUAL(&b, TRI_AtVectorPointer(&v1, 0));

  TRI_PushBackVectorPointer(&v1, &c);
  BOOST_CHECK_EQUAL(&b, TRI_AtVectorPointer(&v1, 0));
  BOOST_CHECK_EQUAL(&c, TRI_AtVectorPointer(&v1, 1));
  
  TRI_RemoveVectorPointer(&v1, 0);
  BOOST_CHECK_EQUAL(&c, TRI_AtVectorPointer(&v1, 0));
  
  TRI_PushBackVectorPointer(&v1, &d);
  TRI_PushBackVectorPointer(&v1, &a);
  
  BOOST_CHECK_EQUAL(&c, TRI_AtVectorPointer(&v1, 0));
  BOOST_CHECK_EQUAL(&d, TRI_AtVectorPointer(&v1, 1));
  BOOST_CHECK_EQUAL(&a, TRI_AtVectorPointer(&v1, 2));

  TRI_RemoveVectorPointer(&v1, 1);
  BOOST_CHECK_EQUAL(&c, TRI_AtVectorPointer(&v1, 0));
  BOOST_CHECK_EQUAL(&a, TRI_AtVectorPointer(&v1, 1));

  VECTOR_DESTROY 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test duplicate pointers
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_push_back_duplicate) {
  VECTOR_INIT
  VECTOR_DATA2

  TRI_PushBackVectorPointer(&v1, &a);
  TRI_PushBackVectorPointer(&v1, &a);
  TRI_PushBackVectorPointer(&v1, &a);
  TRI_PushBackVectorPointer(&v1, &a);
  TRI_PushBackVectorPointer(&v1, &b);
  TRI_PushBackVectorPointer(&v1, &b);

  BOOST_CHECK_EQUAL((size_t) 6, v1._length);
  BOOST_CHECK_EQUAL(&a, TRI_AtVectorPointer(&v1, 0));
  BOOST_CHECK_EQUAL(&a, TRI_AtVectorPointer(&v1, 1));
  BOOST_CHECK_EQUAL(&a, TRI_AtVectorPointer(&v1, 2));
  BOOST_CHECK_EQUAL(&a, TRI_AtVectorPointer(&v1, 3));
  BOOST_CHECK_EQUAL(&b, TRI_AtVectorPointer(&v1, 4));
  BOOST_CHECK_EQUAL(&b, TRI_AtVectorPointer(&v1, 5));

  VECTOR_DESTROY
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test duplicate pointers
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_remove_duplicate) {
  VECTOR_INIT
  VECTOR_DATA2

  TRI_PushBackVectorPointer(&v1, &a);
  TRI_PushBackVectorPointer(&v1, &a);
  TRI_PushBackVectorPointer(&v1, &a);
  TRI_PushBackVectorPointer(&v1, &b);
  TRI_PushBackVectorPointer(&v1, &b);

  BOOST_CHECK_EQUAL((size_t) 5, v1._length);
  BOOST_CHECK_EQUAL(&a, TRI_AtVectorPointer(&v1, 0));
  BOOST_CHECK_EQUAL(&a, TRI_AtVectorPointer(&v1, 1));
  BOOST_CHECK_EQUAL(&a, TRI_AtVectorPointer(&v1, 2));
  BOOST_CHECK_EQUAL(&b, TRI_AtVectorPointer(&v1, 3));
  BOOST_CHECK_EQUAL(&b, TRI_AtVectorPointer(&v1, 4));
  
  TRI_RemoveVectorPointer(&v1, 4); 
  TRI_RemoveVectorPointer(&v1, 0); 
  TRI_RemoveVectorPointer(&v1, 1); 
  BOOST_CHECK_EQUAL((size_t) 2, v1._length);
  BOOST_CHECK_EQUAL(&a, TRI_AtVectorPointer(&v1, 0));
  BOOST_CHECK_EQUAL(&b, TRI_AtVectorPointer(&v1, 1));

  VECTOR_DESTROY
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test push back and remove
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_push_back_remove) {
  VECTOR_INIT
  VECTOR_DATA
  VECTOR_DATA2

  TRI_PushBackVectorPointer(&v1, &a);
  TRI_PushBackVectorPointer(&v1, &b);
  TRI_PushBackVectorPointer(&v1, &c);
  TRI_PushBackVectorPointer(&v1, &d);
  TRI_PushBackVectorPointer(&v1, &a);
  TRI_PushBackVectorPointer(&v1, &b);
  TRI_PushBackVectorPointer(&v1, &c);
  TRI_PushBackVectorPointer(&v1, &d);
  TRI_PushBackVectorPointer(&v1, &a);
  TRI_PushBackVectorPointer(&v1, &a);
  TRI_PushBackVectorPointer(&v1, s);

  BOOST_CHECK_EQUAL((size_t) 11, v1._length);
  BOOST_CHECK_EQUAL(&a, TRI_AtVectorPointer(&v1, 0));
  BOOST_CHECK_EQUAL(&b, TRI_AtVectorPointer(&v1, 1));
  BOOST_CHECK_EQUAL(&c, TRI_AtVectorPointer(&v1, 2));
  BOOST_CHECK_EQUAL(&d, TRI_AtVectorPointer(&v1, 3));
  BOOST_CHECK_EQUAL(&a, TRI_AtVectorPointer(&v1, 4));
  BOOST_CHECK_EQUAL(&b, TRI_AtVectorPointer(&v1, 5));
  BOOST_CHECK_EQUAL(&c, TRI_AtVectorPointer(&v1, 6));
  BOOST_CHECK_EQUAL(&d, TRI_AtVectorPointer(&v1, 7));
  BOOST_CHECK_EQUAL(&a, TRI_AtVectorPointer(&v1, 8));
  BOOST_CHECK_EQUAL(&a, TRI_AtVectorPointer(&v1, 9));
  BOOST_CHECK_EQUAL(s, TRI_AtVectorPointer(&v1, 10));

  BOOST_CHECK_EQUAL(&a, TRI_RemoveVectorPointer(&v1, 4)); 
  BOOST_CHECK_EQUAL((size_t) 10, v1._length);
  BOOST_CHECK_EQUAL(&a, TRI_AtVectorPointer(&v1, 0));
  BOOST_CHECK_EQUAL(&b, TRI_AtVectorPointer(&v1, 1));
  BOOST_CHECK_EQUAL(&c, TRI_AtVectorPointer(&v1, 2));
  BOOST_CHECK_EQUAL(&d, TRI_AtVectorPointer(&v1, 3));
  BOOST_CHECK_EQUAL(&b, TRI_AtVectorPointer(&v1, 4));
  BOOST_CHECK_EQUAL(&c, TRI_AtVectorPointer(&v1, 5));
  BOOST_CHECK_EQUAL(&d, TRI_AtVectorPointer(&v1, 6));
  BOOST_CHECK_EQUAL(&a, TRI_AtVectorPointer(&v1, 7));
  BOOST_CHECK_EQUAL(&a, TRI_AtVectorPointer(&v1, 8));
  BOOST_CHECK_EQUAL(s, TRI_AtVectorPointer(&v1, 9));
  
  BOOST_CHECK_EQUAL(&a, TRI_RemoveVectorPointer(&v1, 0)); 
  BOOST_CHECK_EQUAL((size_t) 9, v1._length);
  BOOST_CHECK_EQUAL(&b, TRI_AtVectorPointer(&v1, 0));
  BOOST_CHECK_EQUAL(&c, TRI_AtVectorPointer(&v1, 1));
  BOOST_CHECK_EQUAL(&d, TRI_AtVectorPointer(&v1, 2));
  BOOST_CHECK_EQUAL(&b, TRI_AtVectorPointer(&v1, 3));
  BOOST_CHECK_EQUAL(&c, TRI_AtVectorPointer(&v1, 4));
  BOOST_CHECK_EQUAL(&d, TRI_AtVectorPointer(&v1, 5));
  BOOST_CHECK_EQUAL(&a, TRI_AtVectorPointer(&v1, 6));
  BOOST_CHECK_EQUAL(&a, TRI_AtVectorPointer(&v1, 7));
  BOOST_CHECK_EQUAL(s, TRI_AtVectorPointer(&v1, 8));
  
  BOOST_CHECK_EQUAL(s, TRI_RemoveVectorPointer(&v1, 8)); 
  BOOST_CHECK_EQUAL((size_t) 8, v1._length);
  BOOST_CHECK_EQUAL(&b, TRI_AtVectorPointer(&v1, 0));
  BOOST_CHECK_EQUAL(&c, TRI_AtVectorPointer(&v1, 1));
  BOOST_CHECK_EQUAL(&d, TRI_AtVectorPointer(&v1, 2));
  BOOST_CHECK_EQUAL(&b, TRI_AtVectorPointer(&v1, 3));
  BOOST_CHECK_EQUAL(&c, TRI_AtVectorPointer(&v1, 4));
  BOOST_CHECK_EQUAL(&d, TRI_AtVectorPointer(&v1, 5));
  BOOST_CHECK_EQUAL(&a, TRI_AtVectorPointer(&v1, 6));
  BOOST_CHECK_EQUAL(&a, TRI_AtVectorPointer(&v1, 7));

  BOOST_CHECK_EQUAL(&b, TRI_RemoveVectorPointer(&v1, 0)); 
  BOOST_CHECK_EQUAL(&c, TRI_RemoveVectorPointer(&v1, 0)); 
  BOOST_CHECK_EQUAL((size_t) 6, v1._length);
  BOOST_CHECK_EQUAL(&d, TRI_AtVectorPointer(&v1, 0));
  BOOST_CHECK_EQUAL(&b, TRI_AtVectorPointer(&v1, 1));
  BOOST_CHECK_EQUAL(&c, TRI_AtVectorPointer(&v1, 2));
  BOOST_CHECK_EQUAL(&d, TRI_AtVectorPointer(&v1, 3));
  BOOST_CHECK_EQUAL(&a, TRI_AtVectorPointer(&v1, 4));
  BOOST_CHECK_EQUAL(&a, TRI_AtVectorPointer(&v1, 5));

  BOOST_CHECK_EQUAL(&b, TRI_RemoveVectorPointer(&v1, 1)); 
  BOOST_CHECK_EQUAL(&c, TRI_RemoveVectorPointer(&v1, 1)); 
  BOOST_CHECK_EQUAL((size_t) 4, v1._length);
  BOOST_CHECK_EQUAL(&d, TRI_AtVectorPointer(&v1, 0));
  BOOST_CHECK_EQUAL(&d, TRI_AtVectorPointer(&v1, 1));
  BOOST_CHECK_EQUAL(&a, TRI_AtVectorPointer(&v1, 2));
  BOOST_CHECK_EQUAL(&a, TRI_AtVectorPointer(&v1, 3));
  
  BOOST_CHECK_EQUAL(&d, TRI_RemoveVectorPointer(&v1, 1)); 
  BOOST_CHECK_EQUAL((size_t) 3, v1._length);
  BOOST_CHECK_EQUAL(&d, TRI_AtVectorPointer(&v1, 0));
  BOOST_CHECK_EQUAL(&a, TRI_AtVectorPointer(&v1, 1));
  BOOST_CHECK_EQUAL(&a, TRI_AtVectorPointer(&v1, 2));
  
  BOOST_CHECK_EQUAL(&a, TRI_RemoveVectorPointer(&v1, 1)); 
  BOOST_CHECK_EQUAL(&a, TRI_RemoveVectorPointer(&v1, 1)); 
  BOOST_CHECK_EQUAL((size_t) 1, v1._length);
  BOOST_CHECK_EQUAL(&d, TRI_AtVectorPointer(&v1, 0));

  VECTOR_DESTROY 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_insert) {
  VECTOR_INIT
  VECTOR_DATA2

  void* r = 0;

  // ...........................................................................
  // this test needs to be altered slightly e.g.
  // TRI_InsertVectorPointer(&v1, &a, 100);
  // TRI_InsertVectorPointer(&v1, &a, 20);
  // TRI_InsertVectorPointer(&v1, &a, 200);
  // ...........................................................................

  
  TRI_InsertVectorPointer(&v1, &a, 0);
  BOOST_CHECK_EQUAL((size_t) 1, v1._length);
  BOOST_CHECK_EQUAL(&a, TRI_AtVectorPointer(&v1, 0));
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, 1));

  TRI_InsertVectorPointer(&v1, &d, 0);
  BOOST_CHECK_EQUAL((size_t) 2, v1._length);
  BOOST_CHECK_EQUAL(&d, TRI_AtVectorPointer(&v1, 0));
  BOOST_CHECK_EQUAL(&a, TRI_AtVectorPointer(&v1, 1));
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, 2));

  TRI_InsertVectorPointer(&v1, &b, 10);
  BOOST_CHECK_EQUAL((size_t) 11, v1._length);
  BOOST_CHECK_EQUAL(&d, TRI_AtVectorPointer(&v1, 0));
  BOOST_CHECK_EQUAL(&a, TRI_AtVectorPointer(&v1, 1));
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, 2));
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, 9));
  BOOST_CHECK_EQUAL(&b, TRI_AtVectorPointer(&v1, 10));
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, 11));

  TRI_InsertVectorPointer(&v1, &c, 10);
  BOOST_CHECK_EQUAL((size_t) 12, v1._length);
  BOOST_CHECK_EQUAL(&d, TRI_AtVectorPointer(&v1, 0));
  BOOST_CHECK_EQUAL(&a, TRI_AtVectorPointer(&v1, 1));
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, 2));
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, 9));
  BOOST_CHECK_EQUAL(&c, TRI_AtVectorPointer(&v1, 10));
  BOOST_CHECK_EQUAL(&b, TRI_AtVectorPointer(&v1, 11));
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, 12));

  TRI_InsertVectorPointer(&v1, &d, 9);
  BOOST_CHECK_EQUAL((size_t) 13, v1._length);
  BOOST_CHECK_EQUAL(&d, TRI_AtVectorPointer(&v1, 0));
  BOOST_CHECK_EQUAL(&a, TRI_AtVectorPointer(&v1, 1));
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, 2));
  BOOST_CHECK_EQUAL(&d, TRI_AtVectorPointer(&v1, 9));
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, 10));
  BOOST_CHECK_EQUAL(&c, TRI_AtVectorPointer(&v1, 11));
  BOOST_CHECK_EQUAL(&b, TRI_AtVectorPointer(&v1, 12));
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, 13));
  
  TRI_InsertVectorPointer(&v1, &c, 100);
  BOOST_CHECK_EQUAL((size_t) 101, v1._length);
  
  BOOST_CHECK_EQUAL(&d, TRI_AtVectorPointer(&v1, 0));
  BOOST_CHECK_EQUAL(&a, TRI_AtVectorPointer(&v1, 1));
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, 2));
  BOOST_CHECK_EQUAL(&d, TRI_AtVectorPointer(&v1, 9));
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, 10));
  BOOST_CHECK_EQUAL(&c, TRI_AtVectorPointer(&v1, 11));
  BOOST_CHECK_EQUAL(&b, TRI_AtVectorPointer(&v1, 12));
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, 13));
  BOOST_CHECK_EQUAL(&c, TRI_AtVectorPointer(&v1, 100));
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, 101));
  
  VECTOR_DESTROY 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert and remove
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_insert_remove) {
  VECTOR_INIT
  VECTOR_DATA2

  void* r = 0;

  TRI_InsertVectorPointer(&v1, &a, 0);
  TRI_InsertVectorPointer(&v1, &b, 4);
  TRI_InsertVectorPointer(&v1, &c, 8);
  TRI_InsertVectorPointer(&v1, &d, 12);

  BOOST_CHECK_EQUAL((size_t) 13, v1._length);
  
  BOOST_CHECK_EQUAL(&a, TRI_AtVectorPointer(&v1, 0));
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, 1));
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, 3));
  BOOST_CHECK_EQUAL(&b, TRI_AtVectorPointer(&v1, 4));
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, 5));
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, 7));
  BOOST_CHECK_EQUAL(&c, TRI_AtVectorPointer(&v1, 8));
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, 9));
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, 11));
  BOOST_CHECK_EQUAL(&d, TRI_AtVectorPointer(&v1, 12));
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, 13));

  BOOST_CHECK_EQUAL(&a, TRI_RemoveVectorPointer(&v1, 0));
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, 0));
  BOOST_CHECK_EQUAL((size_t) 12, v1._length);
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, 0));
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, 1));
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, 2));
  BOOST_CHECK_EQUAL(&b, TRI_AtVectorPointer(&v1, 3));
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, 4));
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, 6));
  BOOST_CHECK_EQUAL(&c, TRI_AtVectorPointer(&v1, 7));
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, 8));
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, 10));
  BOOST_CHECK_EQUAL(&d, TRI_AtVectorPointer(&v1, 11));
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, 12));
  
  BOOST_CHECK_EQUAL(r, TRI_RemoveVectorPointer(&v1, 0));
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, 0));
  BOOST_CHECK_EQUAL((size_t) 11, v1._length);
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, 0));
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, 1));
  BOOST_CHECK_EQUAL(&b, TRI_AtVectorPointer(&v1, 2));
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, 3));
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, 4));
  BOOST_CHECK_EQUAL(&c, TRI_AtVectorPointer(&v1, 6));
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, 7));
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, 8));
  BOOST_CHECK_EQUAL(&d, TRI_AtVectorPointer(&v1, 10));
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, 11));

  BOOST_CHECK_EQUAL(&d, TRI_RemoveVectorPointer(&v1, 10));
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, 10));
  BOOST_CHECK_EQUAL((size_t) 10, v1._length);
  
  BOOST_CHECK_EQUAL(&c, TRI_RemoveVectorPointer(&v1, 6));
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(&v1, 6));
  BOOST_CHECK_EQUAL((size_t) 9, v1._length);

  VECTOR_DESTROY 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test copy
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_copy) {
  VECTOR_INIT
  VECTOR_DATA2

  void* r = 0;
  TRI_vector_pointer_t* v2;

  TRI_InsertVectorPointer(&v1, &a, 0);
  TRI_InsertVectorPointer(&v1, &b, 1);
  TRI_InsertVectorPointer(&v1, &c, 2);
  TRI_InsertVectorPointer(&v1, &d, 3);

  v2 = TRI_CopyVectorPointer(TRI_CORE_MEM_ZONE, &v1);

  VECTOR_DESTROY 

  BOOST_CHECK_EQUAL((size_t) 4, v2->_length);
  BOOST_CHECK_EQUAL(&a, TRI_AtVectorPointer(v2, 0));
  BOOST_CHECK_EQUAL(&b, TRI_AtVectorPointer(v2, 1));
  BOOST_CHECK_EQUAL(&c, TRI_AtVectorPointer(v2, 2));
  BOOST_CHECK_EQUAL(&d, TRI_AtVectorPointer(v2, 3));
  BOOST_CHECK_EQUAL(r, TRI_AtVectorPointer(v2, 4));

  TRI_FreeVectorPointer(v2->_memoryZone, v2);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test modifications
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_value_modifications) {
  VECTOR_INIT
  VECTOR_DATA2

  TRI_vector_pointer_t* v2;

  TRI_InsertVectorPointer(&v1, &a, 0);
  TRI_InsertVectorPointer(&v1, &b, 1);
  TRI_InsertVectorPointer(&v1, &c, 2);
  TRI_InsertVectorPointer(&v1, &d, 3);

  BOOST_CHECK_EQUAL(1, *(int*) TRI_AtVectorPointer(&v1, 0));
  BOOST_CHECK_EQUAL(2, *(int*) TRI_AtVectorPointer(&v1, 1));
  BOOST_CHECK_EQUAL(3, *(int*) TRI_AtVectorPointer(&v1, 2));
  BOOST_CHECK_EQUAL(4, *(int*) TRI_AtVectorPointer(&v1, 3));

  v2 = TRI_CopyVectorPointer(TRI_CORE_MEM_ZONE, &v1);
  a = 99;
  b = 42;
  c = -1;
  d = 0;
  
  BOOST_CHECK_EQUAL(99, *(int*) TRI_AtVectorPointer(&v1, 0));
  BOOST_CHECK_EQUAL(42, *(int*) TRI_AtVectorPointer(&v1, 1));
  BOOST_CHECK_EQUAL(-1, *(int*) TRI_AtVectorPointer(&v1, 2));
  BOOST_CHECK_EQUAL(0, *(int*) TRI_AtVectorPointer(&v1, 3));
  
  VECTOR_DESTROY 

  BOOST_CHECK_EQUAL(99, *(int*) TRI_AtVectorPointer(v2, 0));
  BOOST_CHECK_EQUAL(42, *(int*) TRI_AtVectorPointer(v2, 1));
  BOOST_CHECK_EQUAL(-1, *(int*) TRI_AtVectorPointer(v2, 2));
  BOOST_CHECK_EQUAL(0, *(int*) TRI_AtVectorPointer(v2, 3));

  TRI_FreeVectorPointer(v2->_memoryZone, v2);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE_END ()

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
