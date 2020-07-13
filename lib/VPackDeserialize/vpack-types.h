#ifndef DESERIALIZE_VPACK_TYPES_H
#define DESERIALIZE_VPACK_TYPES_H


#ifndef DESERIALIZER_NO_VPACK_TYPES
#include "velocypack/Slice.h"
#include "velocypack/Iterator.h"

namespace deserializer {
using slice_type = arangodb::velocypack::Slice;
using object_iterator = arangodb::velocypack::ObjectIterator;
using array_iterator = arangodb::velocypack::ArrayIterator;
}
#endif

#endif  // DESERIALIZE_VPACK_TYPES_H
