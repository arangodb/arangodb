// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/property.h"

#include "src/field-type.h"
#include "src/handles-inl.h"
#include "src/objects-inl.h"
#include "src/objects/name-inl.h"
#include "src/ostreams.h"

namespace v8 {
namespace internal {

std::ostream& operator<<(std::ostream& os,
                         const PropertyAttributes& attributes) {
  os << "[";
  os << (((attributes & READ_ONLY) == 0) ? "W" : "_");    // writable
  os << (((attributes & DONT_ENUM) == 0) ? "E" : "_");    // enumerable
  os << (((attributes & DONT_DELETE) == 0) ? "C" : "_");  // configurable
  os << "]";
  return os;
}

Descriptor::Descriptor() : details_(Smi::kZero) {}

Descriptor::Descriptor(Handle<Name> key, const MaybeObjectHandle& value,
                       PropertyKind kind, PropertyAttributes attributes,
                       PropertyLocation location, PropertyConstness constness,
                       Representation representation, int field_index)
    : key_(key),
      value_(value),
      details_(kind, attributes, location, constness, representation,
               field_index) {
  DCHECK(key->IsUniqueName());
  DCHECK_IMPLIES(key->IsPrivate(), !details_.IsEnumerable());
}

Descriptor::Descriptor(Handle<Name> key, const MaybeObjectHandle& value,
                       PropertyDetails details)
    : key_(key), value_(value), details_(details) {
  DCHECK(key->IsUniqueName());
  DCHECK_IMPLIES(key->IsPrivate(), !details_.IsEnumerable());
}

Descriptor Descriptor::DataField(Isolate* isolate, Handle<Name> key,
                                 int field_index, PropertyAttributes attributes,
                                 Representation representation) {
  return DataField(key, field_index, attributes, PropertyConstness::kMutable,
                   representation, MaybeObjectHandle(FieldType::Any(isolate)));
}

Descriptor Descriptor::DataField(Handle<Name> key, int field_index,
                                 PropertyAttributes attributes,
                                 PropertyConstness constness,
                                 Representation representation,
                                 const MaybeObjectHandle& wrapped_field_type) {
  DCHECK(wrapped_field_type->IsSmi() || wrapped_field_type->IsWeak());
  PropertyDetails details(kData, attributes, kField, constness, representation,
                          field_index);
  return Descriptor(key, wrapped_field_type, details);
}

Descriptor Descriptor::DataConstant(Handle<Name> key, Handle<Object> value,
                                    PropertyAttributes attributes) {
  return Descriptor(key, MaybeObjectHandle(value), kData, attributes,
                    kDescriptor, PropertyConstness::kConst,
                    value->OptimalRepresentation(), 0);
}

Descriptor Descriptor::DataConstant(Isolate* isolate, Handle<Name> key,
                                    int field_index, Handle<Object> value,
                                    PropertyAttributes attributes) {
  if (FLAG_track_constant_fields) {
    MaybeObjectHandle any_type(FieldType::Any(), isolate);
    return DataField(key, field_index, attributes, PropertyConstness::kConst,
                     Representation::Tagged(), any_type);

  } else {
    return Descriptor(key, MaybeObjectHandle(value), kData, attributes,
                      kDescriptor, PropertyConstness::kConst,
                      value->OptimalRepresentation(), field_index);
  }
}

Descriptor Descriptor::AccessorConstant(Handle<Name> key,
                                        Handle<Object> foreign,
                                        PropertyAttributes attributes) {
  return Descriptor(key, MaybeObjectHandle(foreign), kAccessor, attributes,
                    kDescriptor, PropertyConstness::kConst,
                    Representation::Tagged(), 0);
}

// Outputs PropertyDetails as a dictionary details.
void PropertyDetails::PrintAsSlowTo(std::ostream& os) {
  os << "(";
  if (constness() == PropertyConstness::kConst) os << "const ";
  os << (kind() == kData ? "data" : "accessor");
  os << ", dict_index: " << dictionary_index();
  os << ", attrs: " << attributes() << ")";
}

// Outputs PropertyDetails as a descriptor array details.
void PropertyDetails::PrintAsFastTo(std::ostream& os, PrintMode mode) {
  os << "(";
  if (constness() == PropertyConstness::kConst) os << "const ";
  os << (kind() == kData ? "data" : "accessor");
  if (location() == kField) {
    os << " field";
    if (mode & kPrintFieldIndex) {
      os << " " << field_index();
    }
    if (mode & kPrintRepresentation) {
      os << ":" << representation().Mnemonic();
    }
  } else {
    os << " descriptor";
  }
  if (mode & kPrintPointer) {
    os << ", p: " << pointer();
  }
  if (mode & kPrintAttributes) {
    os << ", attrs: " << attributes();
  }
  os << ")";
}

#ifdef OBJECT_PRINT
void PropertyDetails::Print(bool dictionary_mode) {
  StdoutStream os;
  if (dictionary_mode) {
    PrintAsSlowTo(os);
  } else {
    PrintAsFastTo(os, PrintMode::kPrintFull);
  }
  os << "\n" << std::flush;
}
#endif

}  // namespace internal
}  // namespace v8
