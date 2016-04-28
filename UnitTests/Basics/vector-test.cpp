////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for TRI_vector_t
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

#include "Basics/vector.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                    private macros
// -----------------------------------------------------------------------------

#define VECTOR_INIT \
  TRI_vector_t v1; \
  TRI_InitVector(&v1, TRI_CORE_MEM_ZONE, sizeof(int));

#define VECTOR_DESTROY \
  TRI_DestroyVector(&v1);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private constants
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct CVectorSetup {
  CVectorSetup () {
    BOOST_TEST_MESSAGE("setup TRI_vector_t");
  }

  ~CVectorSetup () {
    BOOST_TEST_MESSAGE("tear-down TRI_vector_t");
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_SUITE(CVectorTest, CVectorSetup)

////////////////////////////////////////////////////////////////////////////////
/// @brief test length after vector initialization
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_length_init) {
  VECTOR_INIT

  BOOST_CHECK_EQUAL((size_t) 0, TRI_LengthVector(&v1));

  VECTOR_DESTROY
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test vector length after insertions
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_length_insert) {
  VECTOR_INIT

  int p1 = 1;
  int p2 = 2;
  TRI_PushBackVector(&v1, &p1);
  BOOST_CHECK_EQUAL((size_t) 1, TRI_LengthVector(&v1));

  TRI_PushBackVector(&v1, &p1);
  BOOST_CHECK_EQUAL((size_t) 2, TRI_LengthVector(&v1));
  
  TRI_PushBackVector(&v1, &p2);
  BOOST_CHECK_EQUAL((size_t) 3, TRI_LengthVector(&v1));

  VECTOR_DESTROY
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test vector length after insertions & deletions
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_length_insert_remove) {
  VECTOR_INIT

  int p1 = 1;
  int p2 = 2;
  int p3 = 3;
  TRI_PushBackVector(&v1, &p1);
  BOOST_CHECK_EQUAL((size_t) 1, TRI_LengthVector(&v1));

  TRI_RemoveVector(&v1, 0);
  BOOST_CHECK_EQUAL((size_t) 0, TRI_LengthVector(&v1));
  
  TRI_PushBackVector(&v1, &p2);
  TRI_PushBackVector(&v1, &p3);
  BOOST_CHECK_EQUAL((size_t) 2, TRI_LengthVector(&v1));
  
  TRI_RemoveVector(&v1, 0);
  BOOST_CHECK_EQUAL((size_t) 1, TRI_LengthVector(&v1));

  TRI_RemoveVector(&v1, 0);
  BOOST_CHECK_EQUAL((size_t) 0, TRI_LengthVector(&v1));

  VECTOR_DESTROY 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test removal of elements at invalid positions
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_remove_invalid1) {
  VECTOR_INIT

  TRI_RemoveVector(&v1, 0); // invalid position
  TRI_RemoveVector(&v1, 0); // invalid position
  TRI_RemoveVector(&v1, 1); // invalid position
  TRI_RemoveVector(&v1, -1); // invalid position
  TRI_RemoveVector(&v1, 99); // invalid position
  
  BOOST_CHECK_EQUAL((size_t) 0, TRI_LengthVector(&v1));

  VECTOR_DESTROY 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test removal of elements at invalid positions
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_remove_invalid2) {
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

BOOST_AUTO_TEST_CASE (tst_at_empty) {
  VECTOR_INIT

  void* r = 0;
  
  BOOST_CHECK_EQUAL(r, TRI_AtVector(&v1, 0));
  BOOST_CHECK_EQUAL(r, TRI_AtVector(&v1, 1));
  BOOST_CHECK_EQUAL(r, TRI_AtVector(&v1, -1));
  BOOST_CHECK_EQUAL(r, TRI_AtVector(&v1, 99));

  VECTOR_DESTROY 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test at and insert
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_at_insert) {
  VECTOR_INIT
 
  int a = 1; 
  int b = 2; 
  int c = 3;
  int d = 4;
   
  TRI_PushBackVector(&v1, &a);
  BOOST_CHECK_EQUAL(1, *(int*) TRI_AtVector(&v1, 0));

  TRI_PushBackVector(&v1, &b);
  BOOST_CHECK_EQUAL(1, *(int*) TRI_AtVector(&v1, 0));
  BOOST_CHECK_EQUAL(2, *(int*) TRI_AtVector(&v1, 1));

  TRI_PushBackVector(&v1, &c);
  BOOST_CHECK_EQUAL(1, *(int*) TRI_AtVector(&v1, 0));
  BOOST_CHECK_EQUAL(2, *(int*) TRI_AtVector(&v1, 1));
  BOOST_CHECK_EQUAL(3, *(int*) TRI_AtVector(&v1, 2));

  TRI_PushBackVector(&v1, &d);
  BOOST_CHECK_EQUAL(1, *(int*) TRI_AtVector(&v1, 0));
  BOOST_CHECK_EQUAL(2, *(int*) TRI_AtVector(&v1, 1));
  BOOST_CHECK_EQUAL(3, *(int*) TRI_AtVector(&v1, 2));
  BOOST_CHECK_EQUAL(4, *(int*) TRI_AtVector(&v1, 3));

  VECTOR_DESTROY 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test at and insert and remove
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_at_insert_remove) {
  VECTOR_INIT

  void* r = 0;
  int a = 1;
  int b = 2;
  int c = 3;
  int d = 4;
  
  TRI_PushBackVector(&v1, &a);
  BOOST_CHECK_EQUAL(1, *(int*) TRI_AtVector(&v1, 0));

  TRI_RemoveVector(&v1, 0);
  BOOST_CHECK_EQUAL(r, TRI_AtVector(&v1, 0));

  TRI_PushBackVector(&v1, &b);
  BOOST_CHECK_EQUAL(2, *(int*) TRI_AtVector(&v1, 0));

  TRI_PushBackVector(&v1, &c);
  BOOST_CHECK_EQUAL(2, *(int*) TRI_AtVector(&v1, 0));
  BOOST_CHECK_EQUAL(3, *(int*) TRI_AtVector(&v1, 1));
  
  TRI_RemoveVector(&v1, 0);
  BOOST_CHECK_EQUAL(3, *(int*) TRI_AtVector(&v1, 0));
  
  TRI_PushBackVector(&v1, &d);
  TRI_PushBackVector(&v1, &a);
  
  BOOST_CHECK_EQUAL(3, *(int*) TRI_AtVector(&v1, 0));
  BOOST_CHECK_EQUAL(4, *(int*) TRI_AtVector(&v1, 1));
  BOOST_CHECK_EQUAL(1, *(int*) TRI_AtVector(&v1, 2));

  TRI_RemoveVector(&v1, 1);
  BOOST_CHECK_EQUAL(3, *(int*) TRI_AtVector(&v1, 0));
  BOOST_CHECK_EQUAL(1, *(int*) TRI_AtVector(&v1, 1));

  VECTOR_DESTROY 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test duplicate pointers
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_push_back_duplicate) {
  VECTOR_INIT
  int a = 1;
  int b = 2;

  TRI_PushBackVector(&v1, &a);
  TRI_PushBackVector(&v1, &a);
  TRI_PushBackVector(&v1, &a);
  TRI_PushBackVector(&v1, &a);
  TRI_PushBackVector(&v1, &b);
  TRI_PushBackVector(&v1, &b);

  BOOST_CHECK_EQUAL((size_t) 6, TRI_LengthVector(&v1));
  BOOST_CHECK_EQUAL(1, *(int*) TRI_AtVector(&v1, 0));
  BOOST_CHECK_EQUAL(1, *(int*) TRI_AtVector(&v1, 1));
  BOOST_CHECK_EQUAL(1, *(int*) TRI_AtVector(&v1, 2));
  BOOST_CHECK_EQUAL(1, *(int*) TRI_AtVector(&v1, 3));
  BOOST_CHECK_EQUAL(2, *(int*) TRI_AtVector(&v1, 4));
  BOOST_CHECK_EQUAL(2, *(int*) TRI_AtVector(&v1, 5));

  VECTOR_DESTROY
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test duplicate pointers
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_remove_duplicate) {
  VECTOR_INIT
  int a = 1;
  int b = 2;

  TRI_PushBackVector(&v1, &a);
  TRI_PushBackVector(&v1, &a);
  TRI_PushBackVector(&v1, &a);
  TRI_PushBackVector(&v1, &b);
  TRI_PushBackVector(&v1, &b);

  BOOST_CHECK_EQUAL((size_t) 5, TRI_LengthVector(&v1));
  BOOST_CHECK_EQUAL(1, *(int*) TRI_AtVector(&v1, 0));
  BOOST_CHECK_EQUAL(1, *(int*) TRI_AtVector(&v1, 1));
  BOOST_CHECK_EQUAL(1, *(int*) TRI_AtVector(&v1, 2));
  BOOST_CHECK_EQUAL(2, *(int*) TRI_AtVector(&v1, 3));
  BOOST_CHECK_EQUAL(2, *(int*) TRI_AtVector(&v1, 4));
  
  TRI_RemoveVector(&v1, 4); 
  TRI_RemoveVector(&v1, 0); 
  TRI_RemoveVector(&v1, 1); 
  BOOST_CHECK_EQUAL((size_t) 2, TRI_LengthVector(&v1));
  BOOST_CHECK_EQUAL(1, *(int*) TRI_AtVector(&v1, 0));
  BOOST_CHECK_EQUAL(2, *(int*) TRI_AtVector(&v1, 1));

  VECTOR_DESTROY
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test push back and remove
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_push_back_remove) {
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

  BOOST_CHECK_EQUAL((size_t) 10, TRI_LengthVector(&v1));
  BOOST_CHECK_EQUAL(1, *(int*) TRI_AtVector(&v1, 0));
  BOOST_CHECK_EQUAL(2, *(int*) TRI_AtVector(&v1, 1));
  BOOST_CHECK_EQUAL(3, *(int*) TRI_AtVector(&v1, 2));
  BOOST_CHECK_EQUAL(4, *(int*) TRI_AtVector(&v1, 3));
  BOOST_CHECK_EQUAL(1, *(int*) TRI_AtVector(&v1, 4));
  BOOST_CHECK_EQUAL(2, *(int*) TRI_AtVector(&v1, 5));
  BOOST_CHECK_EQUAL(3, *(int*) TRI_AtVector(&v1, 6));
  BOOST_CHECK_EQUAL(4, *(int*) TRI_AtVector(&v1, 7));
  BOOST_CHECK_EQUAL(1, *(int*) TRI_AtVector(&v1, 8));
  BOOST_CHECK_EQUAL(1, *(int*) TRI_AtVector(&v1, 9));

  TRI_RemoveVector(&v1, 4); 
  BOOST_CHECK_EQUAL((size_t) 9, TRI_LengthVector(&v1));
  BOOST_CHECK_EQUAL(1, *(int*) TRI_AtVector(&v1, 0));
  BOOST_CHECK_EQUAL(2, *(int*) TRI_AtVector(&v1, 1));
  BOOST_CHECK_EQUAL(3, *(int*) TRI_AtVector(&v1, 2));
  BOOST_CHECK_EQUAL(4, *(int*) TRI_AtVector(&v1, 3));
  BOOST_CHECK_EQUAL(2, *(int*) TRI_AtVector(&v1, 4));
  BOOST_CHECK_EQUAL(3, *(int*) TRI_AtVector(&v1, 5));
  BOOST_CHECK_EQUAL(4, *(int*) TRI_AtVector(&v1, 6));
  BOOST_CHECK_EQUAL(1, *(int*) TRI_AtVector(&v1, 7));
  BOOST_CHECK_EQUAL(1, *(int*) TRI_AtVector(&v1, 8));
  
  TRI_RemoveVector(&v1, 0); 
  BOOST_CHECK_EQUAL((size_t) 8, TRI_LengthVector(&v1));
  BOOST_CHECK_EQUAL(2, *(int*) TRI_AtVector(&v1, 0));
  BOOST_CHECK_EQUAL(3, *(int*) TRI_AtVector(&v1, 1));
  BOOST_CHECK_EQUAL(4, *(int*) TRI_AtVector(&v1, 2));
  BOOST_CHECK_EQUAL(2, *(int*) TRI_AtVector(&v1, 3));
  BOOST_CHECK_EQUAL(3, *(int*) TRI_AtVector(&v1, 4));
  BOOST_CHECK_EQUAL(4, *(int*) TRI_AtVector(&v1, 5));
  BOOST_CHECK_EQUAL(1, *(int*) TRI_AtVector(&v1, 6));
  BOOST_CHECK_EQUAL(1, *(int*) TRI_AtVector(&v1, 7));
  
  TRI_RemoveVector(&v1, 7); 
  BOOST_CHECK_EQUAL((size_t) 7, TRI_LengthVector(&v1));
  BOOST_CHECK_EQUAL(2, *(int*) TRI_AtVector(&v1, 0));
  BOOST_CHECK_EQUAL(3, *(int*) TRI_AtVector(&v1, 1));
  BOOST_CHECK_EQUAL(4, *(int*) TRI_AtVector(&v1, 2));
  BOOST_CHECK_EQUAL(2, *(int*) TRI_AtVector(&v1, 3));
  BOOST_CHECK_EQUAL(3, *(int*) TRI_AtVector(&v1, 4));
  BOOST_CHECK_EQUAL(4, *(int*) TRI_AtVector(&v1, 5));
  BOOST_CHECK_EQUAL(1, *(int*) TRI_AtVector(&v1, 6));

  TRI_RemoveVector(&v1, 0); 
  TRI_RemoveVector(&v1, 0); 
  BOOST_CHECK_EQUAL((size_t) 5, TRI_LengthVector(&v1));
  BOOST_CHECK_EQUAL(4, *(int*) TRI_AtVector(&v1, 0));
  BOOST_CHECK_EQUAL(2, *(int*) TRI_AtVector(&v1, 1));
  BOOST_CHECK_EQUAL(3, *(int*) TRI_AtVector(&v1, 2));
  BOOST_CHECK_EQUAL(4, *(int*) TRI_AtVector(&v1, 3));
  BOOST_CHECK_EQUAL(1, *(int*) TRI_AtVector(&v1, 4));

  TRI_RemoveVector(&v1, 1); 
  TRI_RemoveVector(&v1, 1); 
  BOOST_CHECK_EQUAL((size_t) 3, TRI_LengthVector(&v1));
  BOOST_CHECK_EQUAL(4, *(int*) TRI_AtVector(&v1, 0));
  BOOST_CHECK_EQUAL(4, *(int*) TRI_AtVector(&v1, 1));
  BOOST_CHECK_EQUAL(1, *(int*) TRI_AtVector(&v1, 2));
  
  TRI_RemoveVector(&v1, 1); 
  BOOST_CHECK_EQUAL((size_t) 2, TRI_LengthVector(&v1));
  BOOST_CHECK_EQUAL(4, *(int*) TRI_AtVector(&v1, 0));
  BOOST_CHECK_EQUAL(1, *(int*) TRI_AtVector(&v1, 1));
  
  TRI_RemoveVector(&v1, 1); 
  TRI_RemoveVector(&v1, 0); 
  BOOST_CHECK_EQUAL((size_t) 0, TRI_LengthVector(&v1));

  VECTOR_DESTROY 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test set
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_set) {
  VECTOR_INIT

  int a = 1;
  int b = 2;
  int c = 3;
  int d = 4;

  TRI_PushBackVector(&v1, &a);
  TRI_PushBackVector(&v1, &b);
  TRI_PushBackVector(&v1, &c);
  TRI_PushBackVector(&v1, &d);

  BOOST_CHECK_EQUAL((size_t) 4, TRI_LengthVector(&v1));
  BOOST_CHECK_EQUAL(1, *(int*) TRI_AtVector(&v1, 0));
  BOOST_CHECK_EQUAL(2, *(int*) TRI_AtVector(&v1, 1));
  BOOST_CHECK_EQUAL(3, *(int*) TRI_AtVector(&v1, 2));
  BOOST_CHECK_EQUAL(4, *(int*) TRI_AtVector(&v1, 3));
  BOOST_CHECK_EQUAL(1, *(int*) TRI_BeginVector(&v1));
  BOOST_CHECK_EQUAL(4, *((int*) (int*) TRI_BeginVector(&v1) + TRI_LengthVector(&v1) - 1));

  TRI_SetVector(&v1, 0, &d);
  TRI_SetVector(&v1, 1, &c);
  TRI_SetVector(&v1, 2, &b);
  TRI_SetVector(&v1, 3, &a);

  BOOST_CHECK_EQUAL((size_t) 4, TRI_LengthVector(&v1));
  BOOST_CHECK_EQUAL(4, *(int*) TRI_AtVector(&v1, 0));
  BOOST_CHECK_EQUAL(3, *(int*) TRI_AtVector(&v1, 1));
  BOOST_CHECK_EQUAL(2, *(int*) TRI_AtVector(&v1, 2));
  BOOST_CHECK_EQUAL(1, *(int*) TRI_AtVector(&v1, 3));
  BOOST_CHECK_EQUAL(4, *(int*) TRI_BeginVector(&v1));
  BOOST_CHECK_EQUAL(1, *((int*) (int*) TRI_BeginVector(&v1) + TRI_LengthVector(&v1) - 1));

  TRI_SetVector(&v1, 0, &b);
  BOOST_CHECK_EQUAL(2, *(int*) TRI_AtVector(&v1, 0));
  BOOST_CHECK_EQUAL(2, *(int*) TRI_BeginVector(&v1));

  VECTOR_DESTROY 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test modifications
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_value_modifications) {
  VECTOR_INIT
  
  int a = 1;
  int b = 2;
  int c = 3;
  int d = 4;

  TRI_PushBackVector(&v1, &a);
  TRI_PushBackVector(&v1, &b);
  TRI_PushBackVector(&v1, &c);
  TRI_PushBackVector(&v1, &d);

  BOOST_CHECK_EQUAL(1, *(int*) TRI_AtVector(&v1, 0));
  BOOST_CHECK_EQUAL(2, *(int*) TRI_AtVector(&v1, 1));
  BOOST_CHECK_EQUAL(3, *(int*) TRI_AtVector(&v1, 2));
  BOOST_CHECK_EQUAL(4, *(int*) TRI_AtVector(&v1, 3));

  a = 99;
  b = 42;
  c = -1;
  d = 0;
  
  BOOST_CHECK_EQUAL(1, *(int*) TRI_AtVector(&v1, 0));
  BOOST_CHECK_EQUAL(2, *(int*) TRI_AtVector(&v1, 1));
  BOOST_CHECK_EQUAL(3, *(int*) TRI_AtVector(&v1, 2));
  BOOST_CHECK_EQUAL(4, *(int*) TRI_AtVector(&v1, 3));
  
  VECTOR_DESTROY 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE_END ()

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
