////////////////////////////////////////////////////////////////////////////////
/// @brief Library to build up VPack documents.
///
/// DISCLAIMER
///
/// Copyright 2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Jan Steemann
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "velocypack/velocypack-common.h"

// this header can be included multiple times

namespace {
// unconditional typedefs
#ifndef VELOCYPACK_ALIAS_VPACKVALUELENGTH
#define VELOCYPACK_ALIAS_VPACKVALUELENGTH
using VPackValueLength = arangodb::velocypack::ValueLength;
#endif

// conditional typedefs, only used when the respective headers are already
// included

// note:
// classes from Basics.h are for internal use only and are not exposed here

#ifdef VELOCYPACK_ITERATOR_H
#ifndef VELOCYPACK_ALIAS_ITERATOR
#define VELOCYPACK_ALIAS_ITERATOR
using VPackArrayIterator = arangodb::velocypack::ArrayIterator;
using VPackObjectIterator = arangodb::velocypack::ObjectIterator;
#endif
#endif

#ifdef VELOCYPACK_BUILDER_H
#ifndef VELOCYPACK_ALIAS_BUILDER
#define VELOCYPACK_ALIAS_BUILDER
using VPackBuilder = arangodb::velocypack::Builder;
using VPackBuilderNonDeleter = arangodb::velocypack::BuilderNonDeleter;
using VPackBuilderContainer = arangodb::velocypack::BuilderContainer;
using VPackObjectBuilder = arangodb::velocypack::ObjectBuilder;
using VPackArrayBuilder = arangodb::velocypack::ArrayBuilder;
#endif
#endif

#ifdef VELOCYPACK_COMPARE_H
#ifndef VELOCYPACK_ALIAS_COMPARE
#define VELOCYPACK_ALIAS_COMPARE
using VPackNormalizedCompare = arangodb::velocypack::NormalizedCompare;
#endif
#endif

#ifdef VELOCYPACK_BUFFER_H
#ifndef VELOCYPACK_ALIAS_BUFFER
#define VELOCYPACK_ALIAS_BUFFER
using VPackCharBuffer = arangodb::velocypack::CharBuffer;
using VPackBufferUInt8 = arangodb::velocypack::UInt8Buffer;
template<typename T> using VPackBuffer = arangodb::velocypack::Buffer<T>;
#endif
#endif

#ifdef VELOCYPACK_SINK_H
#ifndef VELOCYPACK_ALIAS_SINK
#define VELOCYPACK_ALIAS_SINK
using VPackSink = arangodb::velocypack::Sink;
using VPackCharBufferSink = arangodb::velocypack::CharBufferSink;
using VPackStringSink = arangodb::velocypack::StringSink;
using VPackStringStreamSink = arangodb::velocypack::StringStreamSink;
#endif
#endif

#ifdef VELOCYPACK_COLLECTION_H
#ifndef VELOCYPACK_ALIAS_COLLECTION
#define VELOCYPACK_ALIAS_COLLECTION
using VPackCollection = arangodb::velocypack::Collection;
#endif
#endif

#ifdef VELOCYPACK_ATTRIBUTETRANSLATOR_H
#ifndef VELOCYPACK_ALIAS_ATTRIBUTETRANSLATOR
#define VELOCYPACK_ALIAS_ATTRIBUTETRANSLATOR
using VPackAttributeTranslator = arangodb::velocypack::AttributeTranslator;
#endif
#endif

#ifdef VELOCYPACK_DUMPER_H
#ifndef VELOCYPACK_ALIAS_DUMPER
#define VELOCYPACK_ALIAS_DUMPER
using VPackDumper = arangodb::velocypack::Dumper;
#endif
#endif

#ifdef VELOCYPACK_EXCEPTION_H
#ifndef VELOCYPACK_ALIAS_EXCEPTION
#define VELOCYPACK_ALIAS_EXCEPTION
using VPackException = arangodb::velocypack::Exception;
#endif
#endif

#ifdef VELOCYPACK_HEXDUMP_H
#ifndef VELOCYPACK_ALIAS_HEXDUMP
#define VELOCYPACK_ALIAS_HEXDUMP
using VPackHexDump = arangodb::velocypack::HexDump;
#endif
#endif

#ifdef VELOCYPACK_OPTIONS_H
#ifndef VELOCYPACK_ALIAS_OPTIONS
#define VELOCYPACK_ALIAS_OPTIONS
using VPackOptions = arangodb::velocypack::Options;
using VPackCustomTypeHandler = arangodb::velocypack::CustomTypeHandler;
#endif
#endif

#ifdef VELOCYPACK_PARSER_H
#ifndef VELOCYPACK_ALIAS_PARSER
#define VELOCYPACK_ALIAS_PARSER
using VPackParser = arangodb::velocypack::Parser;
#endif
#endif

#ifdef VELOCYPACK_SERIALIZABLE_H
#ifndef VELOCYPACK_ALIAS_SERIALIZABLE
#define VELOCYPACK_ALIAS_SERIALIZABLE
using VPackSerializable = arangodb::velocypack::Serializable;
using VPackSerialize = arangodb::velocypack::Serialize;
#endif
#endif

#ifdef VELOCYPACK_SLICE_H
#ifndef VELOCYPACK_ALIAS_SLICE
#define VELOCYPACK_ALIAS_SLICE
using VPackSlice = arangodb::velocypack::Slice;
#endif
#endif

#ifdef VELOCYPACK_SLICE_CONTAINER_H
#ifndef VELOCYPACK_ALIAS_SLICE_CONTAINER
#define VELOCYPACK_ALIAS_SLICE_CONTAINER
using VPackSliceContainer = arangodb::velocypack::SliceContainer;
#endif
#endif

#ifdef VELOCYPACK_STRINGREF_H
#ifndef VELOCYPACK_ALIAS_STRINGREF
#define VELOCYPACK_ALIAS_STRINGREF
using VPackStringRef = arangodb::velocypack::StringRef;
#endif
#endif

#ifdef VELOCYPACK_UTF8HELPER_H
#ifndef VELOCYPACK_ALIAS_UTF8HELPER
#define VELOCYPACK_ALIAS_UTF8HELPER
using VPackUtf8Helper = arangodb::velocypack::Utf8Helper;
#endif
#endif

#ifdef VELOCYPACK_VALIDATOR_H
#ifndef VELOCYPACK_ALIAS_VALIDATOR
#define VELOCYPACK_ALIAS_VALIDATOR
using VPackValidator = arangodb::velocypack::Validator;
#endif
#endif

#ifdef VELOCYPACK_VALUE_H
#ifndef VELOCYPACK_ALIAS_VALUE
#define VELOCYPACK_ALIAS_VALUE
using VPackValue = arangodb::velocypack::Value;
using VPackValuePair = arangodb::velocypack::ValuePair;
#endif
#endif

#ifdef VELOCYPACK_VALUETYPE_H
#ifndef VELOCYPACK_ALIAS_VALUETYPE
#define VELOCYPACK_ALIAS_VALUETYPE
using VPackValueType = arangodb::velocypack::ValueType;
#endif
#endif

#ifdef VELOCYPACK_VERSION_H
#ifndef VELOCYPACK_ALIAS_VERSION
#define VELOCYPACK_ALIAS_VERSION
using VPackVersion = arangodb::velocypack::Version;
#endif
#endif
}
