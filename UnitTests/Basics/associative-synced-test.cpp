////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for TRI_associative_synced_t
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
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
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
  TRI_associative_synced_t a1; \
  TRI_InitAssociativeSynced(&a1, TRI_CORE_MEM_ZONE, HashKey, HashElement, IsEqualKeyElement, IsEqualElementElement);

#define DESTROY_ASSOC \
  TRI_DestroyAssociativeSynced(&a1);
  
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

uint64_t HashKey (TRI_associative_synced_t* a, void const* key) {
  return TRI_FnvHashString((char const*) key);
}

uint64_t HashElement (TRI_associative_synced_t* a, void const* e) {
  data_container_s* element = (data_container_s*) e;

  return TRI_FnvHashString(element->key);
}

bool IsEqualKeyElement (TRI_associative_synced_t* a, void const* k, void const* r) {
  data_container_s* element = (data_container_s*) r;

  return TRI_EqualString((char*) k, element->key);
}

bool IsEqualElementElement (TRI_associative_synced_t* a, void const* l, void const* r) {
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

struct CAssociativeSyncedSetup {
  CAssociativeSyncedSetup () {
  }

  ~CAssociativeSyncedSetup () {
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_SUITE(CAssociativeSyncedTest, CAssociativeSyncedSetup)

////////////////////////////////////////////////////////////////////////////////
/// @brief test initialisation
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_init) {
  INIT_ASSOC

  BOOST_CHECK_EQUAL((size_t) 0, TRI_GetLengthAssociativeSynced(&a1));

  DESTROY_ASSOC
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test unique insertion
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_insert_key_unique) {
  INIT_ASSOC

  void* r = 0;

  ELEMENT(e1, (char*) "test1", 1, 2, 3)
  BOOST_CHECK_EQUAL(r, TRI_InsertKeyAssociativeSynced(&a1, e1.key, &e1));
  BOOST_CHECK_EQUAL((size_t) 1, TRI_GetLengthAssociativeSynced(&a1));
  BOOST_CHECK_EQUAL(&e1, TRI_LookupByKeyAssociativeSynced(&a1, "test1"));
  
  ELEMENT(e2, (char*) "test2", 2, 3, 4)
  BOOST_CHECK_EQUAL(r, TRI_InsertKeyAssociativeSynced(&a1, e2.key, &e2));
  BOOST_CHECK_EQUAL((size_t) 2, TRI_GetLengthAssociativeSynced(&a1));
  BOOST_CHECK_EQUAL(&e2, TRI_LookupByKeyAssociativeSynced(&a1, "test2"));
  
  ELEMENT(e3, (char*) "test3", 99, 3, 5)
  BOOST_CHECK_EQUAL(r, TRI_InsertKeyAssociativeSynced(&a1, e3.key, &e3));
  BOOST_CHECK_EQUAL((size_t) 3, TRI_GetLengthAssociativeSynced(&a1));
  BOOST_CHECK_EQUAL(&e3, TRI_LookupByKeyAssociativeSynced(&a1, "test3"));

  DESTROY_ASSOC
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test non-unique insertion
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_insert_key_nonunique) {
  INIT_ASSOC

  void* r = 0;

  ELEMENT(e1, (char*) "test1", 1, 2, 3)
  BOOST_CHECK_EQUAL(r, TRI_InsertKeyAssociativeSynced(&a1, e1.key, &e1));
  BOOST_CHECK_EQUAL((size_t) 1, TRI_GetLengthAssociativeSynced(&a1));
  BOOST_CHECK_EQUAL(&e1, TRI_LookupByKeyAssociativeSynced(&a1, "test1"));
  
  ELEMENT(e2, (char*) "test1", 2, 3, 4)
  BOOST_CHECK_EQUAL(&e1, TRI_InsertKeyAssociativeSynced(&a1, e2.key, &e2));
  BOOST_CHECK_EQUAL((size_t) 1, TRI_GetLengthAssociativeSynced(&a1));
  BOOST_CHECK_EQUAL(&e1, TRI_LookupByKeyAssociativeSynced(&a1, "test1"));
  
  ELEMENT(e3, (char*) "test2", 99, 3, 5)
  BOOST_CHECK_EQUAL(r, TRI_InsertKeyAssociativeSynced(&a1, e3.key, &e3));
  BOOST_CHECK_EQUAL((size_t) 2, TRI_GetLengthAssociativeSynced(&a1));
  BOOST_CHECK_EQUAL(&e3, TRI_LookupByKeyAssociativeSynced(&a1, "test2"));
  
  ELEMENT(e4, (char*) "test1", 99, 3, 5)
  BOOST_CHECK_EQUAL(&e1, TRI_InsertKeyAssociativeSynced(&a1, e4.key, &e4));
  BOOST_CHECK_EQUAL((size_t) 2, TRI_GetLengthAssociativeSynced(&a1));
  BOOST_CHECK_EQUAL(&e1, TRI_LookupByKeyAssociativeSynced(&a1, "test1"));
  
  ELEMENT(e5, (char*) "test2", -99, 33, 15)
  BOOST_CHECK_EQUAL(&e3, TRI_InsertKeyAssociativeSynced(&a1, e5.key, &e5));
  BOOST_CHECK_EQUAL((size_t) 2, TRI_GetLengthAssociativeSynced(&a1));
  BOOST_CHECK_EQUAL(&e3, TRI_LookupByKeyAssociativeSynced(&a1, "test2"));

  DESTROY_ASSOC
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test key lookup
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_lookup_key) {
  INIT_ASSOC

  void* r = 0;

  ELEMENT(e1, (char*) "test1", 1, 2, 3)
  TRI_InsertKeyAssociativeSynced(&a1, e1.key, &e1);
  
  ELEMENT(e2, (char*) "test2", 2, 3, 4)
  TRI_InsertKeyAssociativeSynced(&a1, e2.key, &e2);
  
  ELEMENT(e3, (char*) "test3", 3, 4, 5)
  TRI_InsertKeyAssociativeSynced(&a1, e3.key, &e3);

  ELEMENT(e4, (char*) "test4", 4, 5, 6)
  TRI_InsertKeyAssociativeSynced(&a1, e4.key, &e4);

  data_container_t* r1 = (data_container_t*) TRI_LookupByKeyAssociativeSynced(&a1, "test1");
  BOOST_CHECK_EQUAL(&e1, r1);
  BOOST_CHECK_EQUAL("test1", r1->key);
  BOOST_CHECK_EQUAL(1, r1->a);
  BOOST_CHECK_EQUAL(2, r1->b);
  BOOST_CHECK_EQUAL(3, r1->c);

  data_container_t* r2 = (data_container_t*) TRI_LookupByKeyAssociativeSynced(&a1, "test2");
  BOOST_CHECK_EQUAL(&e2, r2);
  BOOST_CHECK_EQUAL("test2", r2->key);
  BOOST_CHECK_EQUAL(2, r2->a);
  BOOST_CHECK_EQUAL(3, r2->b);
  BOOST_CHECK_EQUAL(4, r2->c);

  data_container_t* r3 = (data_container_t*) TRI_LookupByKeyAssociativeSynced(&a1, "test3");
  BOOST_CHECK_EQUAL(&e3, r3);
  BOOST_CHECK_EQUAL("test3", r3->key);
  BOOST_CHECK_EQUAL(3, r3->a);
  BOOST_CHECK_EQUAL(4, r3->b);
  BOOST_CHECK_EQUAL(5, r3->c);

  data_container_t* r4 = (data_container_t*) TRI_LookupByKeyAssociativeSynced(&a1, "test4");
  BOOST_CHECK_EQUAL(&e4, r4);
  BOOST_CHECK_EQUAL("test4", r4->key);
  BOOST_CHECK_EQUAL(4, r4->a);
  BOOST_CHECK_EQUAL(5, r4->b);
  BOOST_CHECK_EQUAL(6, r4->c);

  BOOST_CHECK_EQUAL(r, TRI_LookupByKeyAssociativeSynced(&a1, "test0"));
  BOOST_CHECK_EQUAL(r, TRI_LookupByKeyAssociativeSynced(&a1, "test5"));
  BOOST_CHECK_EQUAL(r, TRI_LookupByKeyAssociativeSynced(&a1, "TEST1"));
  BOOST_CHECK_EQUAL(r, TRI_LookupByKeyAssociativeSynced(&a1, "peng!"));

  DESTROY_ASSOC
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test removal
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_remove_key) {
  INIT_ASSOC

  void* r = 0;

  ELEMENT(e1, (char*) "test1", 1, 2, 3)
  TRI_InsertKeyAssociativeSynced(&a1, e1.key, &e1);
  
  ELEMENT(e2, (char*) "test2", 2, 3, 4)
  TRI_InsertKeyAssociativeSynced(&a1, e2.key, &e2);
  
  ELEMENT(e3, (char*) "test3", 3, 4, 5)
  TRI_InsertKeyAssociativeSynced(&a1, e3.key, &e3);

  ELEMENT(e4, (char*) "test4", 4, 5, 6)
  TRI_InsertKeyAssociativeSynced(&a1, e4.key, &e4);

  data_container_t* r1 = (data_container_t*) TRI_RemoveKeyAssociativeSynced(&a1, "test1");
  BOOST_CHECK_EQUAL(&e1, r1);
  BOOST_CHECK_EQUAL("test1", r1->key);
  BOOST_CHECK_EQUAL(1, r1->a);
  BOOST_CHECK_EQUAL(2, r1->b);
  BOOST_CHECK_EQUAL(3, r1->c);
  BOOST_CHECK_EQUAL((size_t) 3, TRI_GetLengthAssociativeSynced(&a1));
  BOOST_CHECK_EQUAL(r, TRI_LookupByKeyAssociativeSynced(&a1, "test1"));

  data_container_t* r2 = (data_container_t*) TRI_RemoveKeyAssociativeSynced(&a1, "test2");
  BOOST_CHECK_EQUAL(&e2, r2);
  BOOST_CHECK_EQUAL("test2", r2->key);
  BOOST_CHECK_EQUAL(2, r2->a);
  BOOST_CHECK_EQUAL(3, r2->b);
  BOOST_CHECK_EQUAL(4, r2->c);
  BOOST_CHECK_EQUAL((size_t) 2, TRI_GetLengthAssociativeSynced(&a1));
  BOOST_CHECK_EQUAL(r, TRI_LookupByKeyAssociativeSynced(&a1, "test2"));

  data_container_t* rx1 = (data_container_t*) TRI_RemoveKeyAssociativeSynced(&a1, "test2");
  BOOST_CHECK_EQUAL(r, rx1);
  BOOST_CHECK_EQUAL((size_t) 2, TRI_GetLengthAssociativeSynced(&a1));

  data_container_t* rx2 = (data_container_t*) TRI_RemoveKeyAssociativeSynced(&a1, "test0");
  BOOST_CHECK_EQUAL(r, rx2);
  BOOST_CHECK_EQUAL((size_t) 2, TRI_GetLengthAssociativeSynced(&a1));
  
  data_container_t* r3 = (data_container_t*) TRI_RemoveKeyAssociativeSynced(&a1, "test3");
  BOOST_CHECK_EQUAL(&e3, r3);
  BOOST_CHECK_EQUAL("test3", r3->key);
  BOOST_CHECK_EQUAL(3, r3->a);
  BOOST_CHECK_EQUAL(4, r3->b);
  BOOST_CHECK_EQUAL(5, r3->c);
  BOOST_CHECK_EQUAL((size_t) 1, TRI_GetLengthAssociativeSynced(&a1));
  BOOST_CHECK_EQUAL(r, TRI_LookupByKeyAssociativeSynced(&a1, "test3"));
  
  data_container_t* r4 = (data_container_t*) TRI_RemoveKeyAssociativeSynced(&a1, "test4");
  BOOST_CHECK_EQUAL(&e4, r4);
  BOOST_CHECK_EQUAL("test4", r4->key);
  BOOST_CHECK_EQUAL(4, r4->a);
  BOOST_CHECK_EQUAL(5, r4->b);
  BOOST_CHECK_EQUAL(6, r4->c);
  BOOST_CHECK_EQUAL((size_t) 0, TRI_GetLengthAssociativeSynced(&a1));
  BOOST_CHECK_EQUAL(r, TRI_LookupByKeyAssociativeSynced(&a1, "test4"));

  DESTROY_ASSOC
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test removal when empty
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_remove_key_empty) {
  INIT_ASSOC

  void* r = 0;
  
  BOOST_CHECK_EQUAL((size_t) 0, TRI_GetLengthAssociativeSynced(&a1));

  BOOST_CHECK_EQUAL(r, TRI_RemoveKeyAssociativeSynced(&a1, "test1"));
  BOOST_CHECK_EQUAL(r, TRI_RemoveKeyAssociativeSynced(&a1, "test2"));
  BOOST_CHECK_EQUAL(r, TRI_RemoveKeyAssociativeSynced(&a1, "test3"));
  
  BOOST_CHECK_EQUAL((size_t) 0, TRI_GetLengthAssociativeSynced(&a1));

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

    BOOST_CHECK_EQUAL(r, TRI_InsertKeyAssociativeSynced(&a1, e->key, e));
    BOOST_CHECK_EQUAL(i, TRI_GetLengthAssociativeSynced(&a1));

    data_container_t* s = (data_container_t*) TRI_LookupByKeyAssociativeSynced(&a1, key);
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

    BOOST_CHECK_EQUAL(r, TRI_InsertKeyAssociativeSynced(&a1, e->key, e));
    BOOST_CHECK_EQUAL(i, TRI_GetLengthAssociativeSynced(&a1));

    data_container_t* s = (data_container_t*) TRI_LookupByKeyAssociativeSynced(&a1, key);
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
    
    data_container_t* s = (data_container_t*) TRI_RemoveKeyAssociativeSynced(&a1, key);
    if (s) {
      // free element memory
      TRI_FreeString(TRI_CORE_MEM_ZONE, s->key);
      TRI_Free(TRI_CORE_MEM_ZONE, s);
    }
  }
  BOOST_CHECK_EQUAL((size_t) 500, TRI_GetLengthAssociativeSynced(&a1));


  // check remaining elements 
  for (size_t i = 1; i <= 1000; ++i) {
    char* num = TRI_StringUInt32((uint32_t) i);

    memset(&key, 0, sizeof(key));
    strcpy(key, "test");
    strcat(key, num);
    TRI_FreeString(TRI_CORE_MEM_ZONE, num);

    data_container_t* s = (data_container_t*) TRI_LookupByKeyAssociativeSynced(&a1, key);
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
