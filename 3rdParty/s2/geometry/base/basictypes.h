//
// Copyright 2001 - 2003 Google, Inc.
//

#ifndef _BASICTYPES_H_
#define _BASICTYPES_H_

#include "casts.h"
#include "port.h"

//
// Google-specific types
//

// id for odp categories
typedef uint32_t CatId;
const CatId kIllegalCatId = static_cast<CatId>(0);

typedef uint32_t TermId;
const TermId kIllegalTermId = static_cast<TermId>(0);

typedef uint32_t HostId;
const HostId kIllegalHostId = static_cast<HostId>(0);

typedef uint32_t DomainId;
const DomainId kIllegalDomainId = static_cast<DomainId>(0);

// Pagerank related types.
// TODO(user) - we'd like to move this into google3/pagerank/
// prtype.h, but this datatype is used all over and that would be
// a major change.
// To get a complete picture of all the datatypes used for PageRank
// and the functions to convert between them, please see google3/
// pagerank/prtype.h
typedef uint16_t DocumentPageRank;  // value in [0, kNumPageRankValues)
const int kNumPageRankValues = 1 << (sizeof(DocumentPageRank) * 8);
const DocumentPageRank kIllegalPagerank = 0;

// Used for fielded search
typedef int32_t FieldValue;
const FieldValue kIllegalFieldValue = static_cast<FieldValue>(INT_MAX);

// It is expected that we *never* have a collision of Fingerprints for
// 2 distinct objects.  No object has kIllegalFprint as its Fingerprint.
typedef uint64_t Fprint;
const Fprint  kIllegalFprint = static_cast<Fprint>(0);
const Fprint  kMaxFprint = static_cast<Fprint>(UINT64_MAX);//kuint64max

// 64 bit checksum (see common/checksummer.{h,cc})
typedef uint64_t Checksum64;

const Checksum64 kIllegalChecksum = static_cast<Checksum64>(0);

// In contrast to Fingerprints, we *do* expect Hash<i> values to collide
// from time to time (although we obviously prefer them not to).  Also
// note that there is an illegal hash value for each size hash.
typedef uint32_t Hash32;
typedef uint16_t Hash16;
typedef unsigned char  Hash8;

const Hash32 kIllegalHash32 = static_cast<Hash32>(4294967295UL);    // 2^32-1
const Hash16 kIllegalHash16 = static_cast<Hash16>(65535U);          // 2^16-1
const Hash8  kIllegalHash8 = static_cast<Hash8>(255);               // 2^8-1



// MetatagId refers to metatag-id that we assign to
// each metatag <name, value> pair..
typedef uint32_t MetatagId;

// Argument type used in interfaces that can optionally take ownership
// of a passed in argument.  If TAKE_OWNERSHIP is passed, the called
// object takes ownership of the argument.  Otherwise it does not.
enum Ownership {
  DO_NOT_TAKE_OWNERSHIP,
  TAKE_OWNERSHIP
};

// Use these as the mlock_bytes parameter to MLock and MLockGeneral
enum { MLOCK_ALL = -1, MLOCK_NONE = 0 };

// The following enum should be used only as a constructor argument to indicate
// that the variable has static storage class, and that the constructor should
// do nothing to its state.  It indicates to the reader that it is legal to
// declare a static nistance of the class, provided the constructor is given
// the base::LINKER_INITIALIZED argument.  Normally, it is unsafe to declare a
// static variable that has a constructor or a destructor because invocation
// order is undefined.  However, IF the type can be initialized by filling with
// zeroes (which the loader does for static variables), AND the type's
// destructor does nothing to the storage, then a constructor for static initialization
// can be declared as
//       explicit MyClass(base::LinkerInitialized x) {}
// and invoked as
//       static MyClass my_variable_name(base::LINKER_INITIALIZED);
namespace base {
enum LinkerInitialized { LINKER_INITIALIZED };
}

#endif  // _BASICTYPES_H_
