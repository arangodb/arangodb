////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for TRI_associative_pointer_t
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

#include "BasicsC/associative.h"
#include "BasicsC/hashes.h"
#include "BasicsC/tri-strings.h"
#include "BasicsC/conversions.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                    private macros
// -----------------------------------------------------------------------------

#define INIT_ASSOC \
  TRI_associative_pointer_t a1; \
  TRI_InitAssociativePointer(&a1, TRI_CORE_MEM_ZONE, TRI_HashStringKeyAssociativePointer, HashElement, IsEqualKeyElement, IsEqualElementElement);

#define DESTROY_ASSOC \
  TRI_DestroyAssociativePointer(&a1);
  
#define ELEMENT(name, k, v1, v2, v3) \
  data_container_t name; \
  name.key = k; \
  name.a   = v1; \
  name.b   = v2; \
  name.c   = v3;

typedef struct data_container_s {
  char* key;
  int a;
  int b;
  int c;
}
data_container_t;

uint64_t HashElement (TRI_associative_pointer_t* a, void const* e) {
  data_container_s* element = (data_container_s*) e;

  return TRI_FnvHashString(element->key);
}

bool IsEqualKeyElement (TRI_associative_pointer_t* a, void const* k, void const* r) {
  data_container_s* element = (data_container_s*) r;

  return TRI_EqualString((char*) k, element->key);
}

bool IsEqualElementElement (TRI_associative_pointer_t* a, void const* l, void const* r) {
  data_container_s* left = (data_container_s*) l;
  data_container_s* right = (data_container_s*) r;

  return TRI_EqualString(left->key, right->key);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 private constants
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct CAssociativePointerSetup {
  CAssociativePointerSetup () {
    BOOST_TEST_MESSAGE("setup TRI_associative_pointer_t");
  }

  ~CAssociativePointerSetup () {
    BOOST_TEST_MESSAGE("tear-down TRI_associative_pointer_t");
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_SUITE(CAssociativePointerTest, CAssociativePointerSetup)

////////////////////////////////////////////////////////////////////////////////
/// @brief test initialisation
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_init) {
  INIT_ASSOC

  BOOST_CHECK_EQUAL((size_t) 0, a1._nrUsed);

  DESTROY_ASSOC
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test unique insertion
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_insert_key_unique) {
  INIT_ASSOC

  void* r = 0;

  ELEMENT(e1, (char*) "test1", 1, 2, 3)
  BOOST_CHECK_EQUAL(r, TRI_InsertKeyAssociativePointer(&a1, e1.key, &e1, false));
  BOOST_CHECK_EQUAL((size_t) 1, a1._nrUsed);
  BOOST_CHECK_EQUAL(&e1, TRI_LookupByKeyAssociativePointer(&a1, "test1"));
  
  ELEMENT(e2, (char*) "test2", 2, 3, 4)
  BOOST_CHECK_EQUAL(r, TRI_InsertKeyAssociativePointer(&a1, e2.key, &e2, false));
  BOOST_CHECK_EQUAL((size_t) 2, a1._nrUsed);
  BOOST_CHECK_EQUAL(&e2, TRI_LookupByKeyAssociativePointer(&a1, "test2"));
  
  ELEMENT(e3, (char*) "test3", 99, 3, 5)
  BOOST_CHECK_EQUAL(r, TRI_InsertKeyAssociativePointer(&a1, e3.key, &e3, false));
  BOOST_CHECK_EQUAL((size_t) 3, a1._nrUsed);
  BOOST_CHECK_EQUAL(&e3, TRI_LookupByKeyAssociativePointer(&a1, "test3"));

  DESTROY_ASSOC
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test non-unique insertion
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_insert_key_nonunique) {
  INIT_ASSOC

  void* r = 0;

  ELEMENT(e1, (char*) "test1", 1, 2, 3)
  BOOST_CHECK_EQUAL(r, TRI_InsertKeyAssociativePointer(&a1, e1.key, &e1, false));
  BOOST_CHECK_EQUAL((size_t) 1, a1._nrUsed);
  BOOST_CHECK_EQUAL(&e1, TRI_LookupByKeyAssociativePointer(&a1, "test1"));
  
  ELEMENT(e2, (char*) "test1", 2, 3, 4)
  BOOST_CHECK_EQUAL(&e1, TRI_InsertKeyAssociativePointer(&a1, e2.key, &e2, false));
  BOOST_CHECK_EQUAL((size_t) 1, a1._nrUsed);
  BOOST_CHECK_EQUAL(&e1, TRI_LookupByKeyAssociativePointer(&a1, "test1"));
  
  ELEMENT(e3, (char*) "test2", 99, 3, 5)
  BOOST_CHECK_EQUAL(r, TRI_InsertKeyAssociativePointer(&a1, e3.key, &e3, false));
  BOOST_CHECK_EQUAL((size_t) 2, a1._nrUsed);
  BOOST_CHECK_EQUAL(&e3, TRI_LookupByKeyAssociativePointer(&a1, "test2"));
  
  ELEMENT(e4, (char*) "test1", 99, 3, 5)
  BOOST_CHECK_EQUAL(&e1, TRI_InsertKeyAssociativePointer(&a1, e4.key, &e4, false));
  BOOST_CHECK_EQUAL((size_t) 2, a1._nrUsed);
  BOOST_CHECK_EQUAL(&e1, TRI_LookupByKeyAssociativePointer(&a1, "test1"));
  
  ELEMENT(e5, (char*) "test2", -99, 33, 15)
  BOOST_CHECK_EQUAL(&e3, TRI_InsertKeyAssociativePointer(&a1, e5.key, &e5, false));
  BOOST_CHECK_EQUAL((size_t) 2, a1._nrUsed);
  BOOST_CHECK_EQUAL(&e3, TRI_LookupByKeyAssociativePointer(&a1, "test2"));

  DESTROY_ASSOC
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test non-unique insertion w/ overwrite
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_insert_key_nonunique_overwrite) {
  INIT_ASSOC

  void* r = 0;

  ELEMENT(e1, (char*) "test1", 1, 2, 3)
  BOOST_CHECK_EQUAL(r, TRI_InsertKeyAssociativePointer(&a1, e1.key, &e1, true));
  BOOST_CHECK_EQUAL((size_t) 1, a1._nrUsed);
  BOOST_CHECK_EQUAL(&e1, TRI_LookupByKeyAssociativePointer(&a1, "test1"));
  
  ELEMENT(e2, (char*) "test1", 2, 3, 4)
  BOOST_CHECK_EQUAL(&e1, TRI_InsertKeyAssociativePointer(&a1, e2.key, &e2, true));
  BOOST_CHECK_EQUAL((size_t) 1, a1._nrUsed);
  BOOST_CHECK_EQUAL(&e2, TRI_LookupByKeyAssociativePointer(&a1, "test1"));
  
  ELEMENT(e3, (char*) "test2", 99, 3, 5)
  BOOST_CHECK_EQUAL(r, TRI_InsertKeyAssociativePointer(&a1, e3.key, &e3, true));
  BOOST_CHECK_EQUAL((size_t) 2, a1._nrUsed);
  BOOST_CHECK_EQUAL(&e3, TRI_LookupByKeyAssociativePointer(&a1, "test2"));
  
  ELEMENT(e4, (char*) "test1", 99, 3, 5)
  BOOST_CHECK_EQUAL(&e2, TRI_InsertKeyAssociativePointer(&a1, e4.key, &e4, true));
  BOOST_CHECK_EQUAL((size_t) 2, a1._nrUsed);
  BOOST_CHECK_EQUAL(&e4, TRI_LookupByKeyAssociativePointer(&a1, "test1"));
  
  ELEMENT(e5, (char*) "test2", -99, 33, 15)
  BOOST_CHECK_EQUAL(&e3, TRI_InsertKeyAssociativePointer(&a1, e5.key, &e5, true));
  BOOST_CHECK_EQUAL((size_t) 2, a1._nrUsed);
  BOOST_CHECK_EQUAL(&e5, TRI_LookupByKeyAssociativePointer(&a1, "test2"));

  DESTROY_ASSOC
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test key lookup
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_lookup_key) {
  INIT_ASSOC

  void* r = 0;

  ELEMENT(e1, (char*) "test1", 1, 2, 3)
  TRI_InsertKeyAssociativePointer(&a1, e1.key, &e1, false);
  
  ELEMENT(e2, (char*) "test2", 2, 3, 4)
  TRI_InsertKeyAssociativePointer(&a1, e2.key, &e2, false);
  
  ELEMENT(e3, (char*) "test3", 3, 4, 5)
  TRI_InsertKeyAssociativePointer(&a1, e3.key, &e3, false);

  ELEMENT(e4, (char*) "test4", 4, 5, 6)
  TRI_InsertKeyAssociativePointer(&a1, e4.key, &e4, false);

  data_container_t* r1 = (data_container_t*) TRI_LookupByKeyAssociativePointer(&a1, "test1");
  BOOST_CHECK_EQUAL(&e1, r1);
  BOOST_CHECK_EQUAL("test1", r1->key);
  BOOST_CHECK_EQUAL(1, r1->a);
  BOOST_CHECK_EQUAL(2, r1->b);
  BOOST_CHECK_EQUAL(3, r1->c);

  data_container_t* r2 = (data_container_t*) TRI_LookupByKeyAssociativePointer(&a1, "test2");
  BOOST_CHECK_EQUAL(&e2, r2);
  BOOST_CHECK_EQUAL("test2", r2->key);
  BOOST_CHECK_EQUAL(2, r2->a);
  BOOST_CHECK_EQUAL(3, r2->b);
  BOOST_CHECK_EQUAL(4, r2->c);

  data_container_t* r3 = (data_container_t*) TRI_LookupByKeyAssociativePointer(&a1, "test3");
  BOOST_CHECK_EQUAL(&e3, r3);
  BOOST_CHECK_EQUAL("test3", r3->key);
  BOOST_CHECK_EQUAL(3, r3->a);
  BOOST_CHECK_EQUAL(4, r3->b);
  BOOST_CHECK_EQUAL(5, r3->c);

  data_container_t* r4 = (data_container_t*) TRI_LookupByKeyAssociativePointer(&a1, "test4");
  BOOST_CHECK_EQUAL(&e4, r4);
  BOOST_CHECK_EQUAL("test4", r4->key);
  BOOST_CHECK_EQUAL(4, r4->a);
  BOOST_CHECK_EQUAL(5, r4->b);
  BOOST_CHECK_EQUAL(6, r4->c);

  BOOST_CHECK_EQUAL(r, TRI_LookupByKeyAssociativePointer(&a1, "test0"));
  BOOST_CHECK_EQUAL(r, TRI_LookupByKeyAssociativePointer(&a1, "test5"));
  BOOST_CHECK_EQUAL(r, TRI_LookupByKeyAssociativePointer(&a1, "TEST1"));
  BOOST_CHECK_EQUAL(r, TRI_LookupByKeyAssociativePointer(&a1, "peng!"));

  DESTROY_ASSOC
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test removal
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_remove_key) {
  INIT_ASSOC

  void* r = 0;

  ELEMENT(e1, (char*) "test1", 1, 2, 3)
  TRI_InsertKeyAssociativePointer(&a1, e1.key, &e1, false);
  
  ELEMENT(e2, (char*) "test2", 2, 3, 4)
  TRI_InsertKeyAssociativePointer(&a1, e2.key, &e2, false);
  
  ELEMENT(e3, (char*) "test3", 3, 4, 5)
  TRI_InsertKeyAssociativePointer(&a1, e3.key, &e3, false);

  ELEMENT(e4, (char*) "test4", 4, 5, 6)
  TRI_InsertKeyAssociativePointer(&a1, e4.key, &e4, false);

  data_container_t* r1 = (data_container_t*) TRI_RemoveKeyAssociativePointer(&a1, "test1");
  BOOST_CHECK_EQUAL(&e1, r1);
  BOOST_CHECK_EQUAL("test1", r1->key);
  BOOST_CHECK_EQUAL(1, r1->a);
  BOOST_CHECK_EQUAL(2, r1->b);
  BOOST_CHECK_EQUAL(3, r1->c);
  BOOST_CHECK_EQUAL((size_t) 3, a1._nrUsed);
  BOOST_CHECK_EQUAL(r, TRI_LookupByKeyAssociativePointer(&a1, "test1"));

  data_container_t* r2 = (data_container_t*) TRI_RemoveKeyAssociativePointer(&a1, "test2");
  BOOST_CHECK_EQUAL(&e2, r2);
  BOOST_CHECK_EQUAL("test2", r2->key);
  BOOST_CHECK_EQUAL(2, r2->a);
  BOOST_CHECK_EQUAL(3, r2->b);
  BOOST_CHECK_EQUAL(4, r2->c);
  BOOST_CHECK_EQUAL((size_t) 2, a1._nrUsed);
  BOOST_CHECK_EQUAL(r, TRI_LookupByKeyAssociativePointer(&a1, "test2"));

  data_container_t* rx1 = (data_container_t*) TRI_RemoveKeyAssociativePointer(&a1, "test2");
  BOOST_CHECK_EQUAL(r, rx1);
  BOOST_CHECK_EQUAL((size_t) 2, a1._nrUsed);

  data_container_t* rx2 = (data_container_t*) TRI_RemoveKeyAssociativePointer(&a1, "test0");
  BOOST_CHECK_EQUAL(r, rx2);
  BOOST_CHECK_EQUAL((size_t) 2, a1._nrUsed);
  
  data_container_t* r3 = (data_container_t*) TRI_RemoveKeyAssociativePointer(&a1, "test3");
  BOOST_CHECK_EQUAL(&e3, r3);
  BOOST_CHECK_EQUAL("test3", r3->key);
  BOOST_CHECK_EQUAL(3, r3->a);
  BOOST_CHECK_EQUAL(4, r3->b);
  BOOST_CHECK_EQUAL(5, r3->c);
  BOOST_CHECK_EQUAL((size_t) 1, a1._nrUsed);
  BOOST_CHECK_EQUAL(r, TRI_LookupByKeyAssociativePointer(&a1, "test3"));
  
  data_container_t* r4 = (data_container_t*) TRI_RemoveKeyAssociativePointer(&a1, "test4");
  BOOST_CHECK_EQUAL(&e4, r4);
  BOOST_CHECK_EQUAL("test4", r4->key);
  BOOST_CHECK_EQUAL(4, r4->a);
  BOOST_CHECK_EQUAL(5, r4->b);
  BOOST_CHECK_EQUAL(6, r4->c);
  BOOST_CHECK_EQUAL((size_t) 0, a1._nrUsed);
  BOOST_CHECK_EQUAL(r, TRI_LookupByKeyAssociativePointer(&a1, "test4"));

  DESTROY_ASSOC
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test removal when empty
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_remove_key_empty) {
  INIT_ASSOC

  void* r = 0;

  BOOST_CHECK_EQUAL(r, TRI_RemoveKeyAssociativePointer(&a1, "test1"));
  BOOST_CHECK_EQUAL(r, TRI_RemoveKeyAssociativePointer(&a1, "test2"));
  BOOST_CHECK_EQUAL(r, TRI_RemoveKeyAssociativePointer(&a1, "test3"));

  DESTROY_ASSOC
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test mass insertion 
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_mass_insert) {
  INIT_ASSOC

  char key[40];
  void* r = 0;

  for (size_t i = 1; i <= 1000; ++i) {
    char* num = TRI_StringUInt32((uint32_t) i);

    memset(&key, 0, sizeof(key));
    strcpy(key, "test");
    strcat(key, num);
    TRI_FreeString(TRI_CORE_MEM_ZONE, num);

    data_container_t* e = (data_container_t*) TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(data_container_t), false);
    e->key = TRI_DuplicateString(key);
    e->a = i;
    e->b = i + 1;
    e->c = i + 2;

    BOOST_CHECK_EQUAL(r, TRI_InsertKeyAssociativePointer(&a1, e->key, e, false));
    BOOST_CHECK_EQUAL(i, a1._nrUsed);

    data_container_t* s = (data_container_t*) TRI_LookupByKeyAssociativePointer(&a1, key);
    BOOST_CHECK_EQUAL(e, s);
    BOOST_CHECK_EQUAL((size_t) i, (size_t) s->a);
    BOOST_CHECK_EQUAL((size_t) i + 1, (size_t) s->b);
    BOOST_CHECK_EQUAL((size_t) i + 2, (size_t) s->c);
    BOOST_CHECK_EQUAL(key, s->key);
  }
  
  // clean up memory
  for (size_t i = 0; i < a1._nrAlloc; ++i) {
    data_container_t* s = (data_container_t*) a1._table[i];

    if (s) {
      // free element memory
      TRI_FreeString(TRI_CORE_MEM_ZONE, s->key);
      TRI_Free(TRI_CORE_MEM_ZONE, s);
    }
  }
  
  DESTROY_ASSOC
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test mass insertion & removal 
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_mass_insert_remove) {
  INIT_ASSOC

  char key[40];
  void* r = 0;

  // fill with 1000 elements
  for (size_t i = 1; i <= 1000; ++i) {
    char* num = TRI_StringUInt32((uint32_t) i);

    memset(&key, 0, sizeof(key));
    strcpy(key, "test");
    strcat(key, num);
    TRI_FreeString(TRI_CORE_MEM_ZONE, num);

    data_container_t* e = (data_container_t*) TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(data_container_t), false); 
    e->key = TRI_DuplicateString(key);
    e->a = i;
    e->b = i + 1;
    e->c = i + 2;

    BOOST_CHECK_EQUAL(r, TRI_InsertKeyAssociativePointer(&a1, e->key, e, false));
    BOOST_CHECK_EQUAL(i, a1._nrUsed);

    data_container_t* s = (data_container_t*) TRI_LookupByKeyAssociativePointer(&a1, key);
    BOOST_CHECK_EQUAL(e, s);
    BOOST_CHECK_EQUAL((size_t) i, (size_t) s->a);
    BOOST_CHECK_EQUAL((size_t) i + 1, (size_t) s->b);
    BOOST_CHECK_EQUAL((size_t) i + 2, (size_t) s->c);
    BOOST_CHECK_EQUAL(key, s->key);
  }

  
  // remove 500 elements
  for (size_t i = 1; i <= 1000; i += 2) {
    char* num = TRI_StringUInt32((uint32_t) i);

    memset(&key, 0, sizeof(key));
    strcpy(key, "test");
    strcat(key, num);
    TRI_FreeString(TRI_CORE_MEM_ZONE, num);
    
    data_container_t* s = (data_container_t*) TRI_RemoveKeyAssociativePointer(&a1, key);
    if (s) {
      // free element memory
      TRI_FreeString(TRI_CORE_MEM_ZONE, s->key);
      TRI_Free(TRI_CORE_MEM_ZONE, s);
    }
  }
  BOOST_CHECK_EQUAL((size_t) 500, a1._nrUsed);


  // check remaining elements 
  for (size_t i = 1; i <= 1000; ++i) {
    char* num = TRI_StringUInt32((uint32_t) i);

    memset(&key, 0, sizeof(key));
    strcpy(key, "test");
    strcat(key, num);
    TRI_FreeString(TRI_CORE_MEM_ZONE, num);

    data_container_t* s = (data_container_t*) TRI_LookupByKeyAssociativePointer(&a1, key);
    if (i % 2) {
      BOOST_REQUIRE_EQUAL(r, s);
    }
    else {
      BOOST_REQUIRE(s);
      BOOST_CHECK_EQUAL((size_t) i, (size_t) s->a);
      BOOST_CHECK_EQUAL((size_t) i + 1, (size_t) s->b);
      BOOST_CHECK_EQUAL((size_t) i + 2, (size_t) s->c);
      BOOST_CHECK_EQUAL(key, s->key);
    }
  }

  // clean up memory
  for (size_t i = 0; i < a1._nrAlloc; ++i) {
    data_container_t* s = (data_container_t*) a1._table[i];

    if (s) {
      // free element memory
      TRI_FreeString(TRI_CORE_MEM_ZONE, s->key);
      TRI_Free(TRI_CORE_MEM_ZONE, s);
    }
  }
  
  DESTROY_ASSOC
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE_END ()

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
