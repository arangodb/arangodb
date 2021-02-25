// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "encoding.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>
#include <stack>

namespace v8_inspector_protocol_encoding {
// =============================================================================
// Status and Error codes
// =============================================================================

std::string Status::ToASCIIString() const {
  switch (error) {
    case Error::OK:
      return "OK";
    case Error::JSON_PARSER_UNPROCESSED_INPUT_REMAINS:
      return ToASCIIString("JSON: unprocessed input remains");
    case Error::JSON_PARSER_STACK_LIMIT_EXCEEDED:
      return ToASCIIString("JSON: stack limit exceeded");
    case Error::JSON_PARSER_NO_INPUT:
      return ToASCIIString("JSON: no input");
    case Error::JSON_PARSER_INVALID_TOKEN:
      return ToASCIIString("JSON: invalid token");
    case Error::JSON_PARSER_INVALID_NUMBER:
      return ToASCIIString("JSON: invalid number");
    case Error::JSON_PARSER_INVALID_STRING:
      return ToASCIIString("JSON: invalid string");
    case Error::JSON_PARSER_UNEXPECTED_ARRAY_END:
      return ToASCIIString("JSON: unexpected array end");
    case Error::JSON_PARSER_COMMA_OR_ARRAY_END_EXPECTED:
      return ToASCIIString("JSON: comma or array end expected");
    case Error::JSON_PARSER_STRING_LITERAL_EXPECTED:
      return ToASCIIString("JSON: string literal expected");
    case Error::JSON_PARSER_COLON_EXPECTED:
      return ToASCIIString("JSON: colon expected");
    case Error::JSON_PARSER_UNEXPECTED_MAP_END:
      return ToASCIIString("JSON: unexpected map end");
    case Error::JSON_PARSER_COMMA_OR_MAP_END_EXPECTED:
      return ToASCIIString("JSON: comma or map end expected");
    case Error::JSON_PARSER_VALUE_EXPECTED:
      return ToASCIIString("JSON: value expected");

    case Error::CBOR_INVALID_INT32:
      return ToASCIIString("CBOR: invalid int32");
    case Error::CBOR_INVALID_DOUBLE:
      return ToASCIIString("CBOR: invalid double");
    case Error::CBOR_INVALID_ENVELOPE:
      return ToASCIIString("CBOR: invalid envelope");
    case Error::CBOR_ENVELOPE_CONTENTS_LENGTH_MISMATCH:
      return ToASCIIString("CBOR: envelope contents length mismatch");
    case Error::CBOR_MAP_OR_ARRAY_EXPECTED_IN_ENVELOPE:
      return ToASCIIString("CBOR: map or array expected in envelope");
    case Error::CBOR_INVALID_STRING8:
      return ToASCIIString("CBOR: invalid string8");
    case Error::CBOR_INVALID_STRING16:
      return ToASCIIString("CBOR: invalid string16");
    case Error::CBOR_INVALID_BINARY:
      return ToASCIIString("CBOR: invalid binary");
    case Error::CBOR_UNSUPPORTED_VALUE:
      return ToASCIIString("CBOR: unsupported value");
    case Error::CBOR_NO_INPUT:
      return ToASCIIString("CBOR: no input");
    case Error::CBOR_INVALID_START_BYTE:
      return ToASCIIString("CBOR: invalid start byte");
    case Error::CBOR_UNEXPECTED_EOF_EXPECTED_VALUE:
      return ToASCIIString("CBOR: unexpected eof expected value");
    case Error::CBOR_UNEXPECTED_EOF_IN_ARRAY:
      return ToASCIIString("CBOR: unexpected eof in array");
    case Error::CBOR_UNEXPECTED_EOF_IN_MAP:
      return ToASCIIString("CBOR: unexpected eof in map");
    case Error::CBOR_INVALID_MAP_KEY:
      return ToASCIIString("CBOR: invalid map key");
    case Error::CBOR_STACK_LIMIT_EXCEEDED:
      return ToASCIIString("CBOR: stack limit exceeded");
    case Error::CBOR_TRAILING_JUNK:
      return ToASCIIString("CBOR: trailing junk");
    case Error::CBOR_MAP_START_EXPECTED:
      return ToASCIIString("CBOR: map start expected");
    case Error::CBOR_MAP_STOP_EXPECTED:
      return ToASCIIString("CBOR: map stop expected");
    case Error::CBOR_ARRAY_START_EXPECTED:
      return ToASCIIString("CBOR: array start expected");
    case Error::CBOR_ENVELOPE_SIZE_LIMIT_EXCEEDED:
      return ToASCIIString("CBOR: envelope size limit exceeded");

    case Error::BINDINGS_MANDATORY_FIELD_MISSING:
      return ToASCIIString("BINDINGS: mandatory field missing");
    case Error::BINDINGS_BOOL_VALUE_EXPECTED:
      return ToASCIIString("BINDINGS: bool value expected");
    case Error::BINDINGS_INT32_VALUE_EXPECTED:
      return ToASCIIString("BINDINGS: int32 value expected");
    case Error::BINDINGS_DOUBLE_VALUE_EXPECTED:
      return ToASCIIString("BINDINGS: double value expected");
    case Error::BINDINGS_STRING_VALUE_EXPECTED:
      return ToASCIIString("BINDINGS: string value expected");
    case Error::BINDINGS_STRING8_VALUE_EXPECTED:
      return ToASCIIString("BINDINGS: string8 value expected");
    case Error::BINDINGS_BINARY_VALUE_EXPECTED:
      return ToASCIIString("BINDINGS: binary value expected");
  }
  // Some compilers can't figure out that we can't get here.
  return "INVALID ERROR CODE";
}

std::string Status::ToASCIIString(const char* msg) const {
  return std::string(msg) + " at position " + std::to_string(pos);
}

namespace cbor {
namespace {
// Indicates the number of bits the "initial byte" needs to be shifted to the
// right after applying |kMajorTypeMask| to produce the major type in the
// lowermost bits.
static constexpr uint8_t kMajorTypeBitShift = 5u;
// Mask selecting the low-order 5 bits of the "initial byte", which is where
// the additional information is encoded.
static constexpr uint8_t kAdditionalInformationMask = 0x1f;
// Mask selecting the high-order 3 bits of the "initial byte", which indicates
// the major type of the encoded value.
static constexpr uint8_t kMajorTypeMask = 0xe0;
// Indicates the integer is in the following byte.
static constexpr uint8_t kAdditionalInformation1Byte = 24u;
// Indicates the integer is in the next 2 bytes.
static constexpr uint8_t kAdditionalInformation2Bytes = 25u;
// Indicates the integer is in the next 4 bytes.
static constexpr uint8_t kAdditionalInformation4Bytes = 26u;
// Indicates the integer is in the next 8 bytes.
static constexpr uint8_t kAdditionalInformation8Bytes = 27u;

// Encodes the initial byte, consisting of the |type| in the first 3 bits
// followed by 5 bits of |additional_info|.
constexpr uint8_t EncodeInitialByte(MajorType type, uint8_t additional_info) {
  return (static_cast<uint8_t>(type) << kMajorTypeBitShift) |
         (additional_info & kAdditionalInformationMask);
}

// TAG 24 indicates that what follows is a byte string which is
// encoded in CBOR format. We use this as a wrapper for
// maps and arrays, allowing us to skip them, because the
// byte string carries its size (byte length).
// https://tools.ietf.org/html/rfc7049#section-2.4.4.1
static constexpr uint8_t kInitialByteForEnvelope =
    EncodeInitialByte(MajorType::TAG, 24);
// The initial byte for a byte string with at most 2^32 bytes
// of payload. This is used for envelope encoding, even if
// the byte string is shorter.
static constexpr uint8_t kInitialByteFor32BitLengthByteString =
    EncodeInitialByte(MajorType::BYTE_STRING, 26);

// See RFC 7049 Section 2.2.1, indefinite length arrays / maps have additional
// info = 31.
static constexpr uint8_t kInitialByteIndefiniteLengthArray =
    EncodeInitialByte(MajorType::ARRAY, 31);
static constexpr uint8_t kInitialByteIndefiniteLengthMap =
    EncodeInitialByte(MajorType::MAP, 31);
// See RFC 7049 Section 2.3, Table 1; this is used for finishing indefinite
// length maps / arrays.
static constexpr uint8_t kStopByte =
    EncodeInitialByte(MajorType::SIMPLE_VALUE, 31);

// See RFC 7049 Section 2.3, Table 2.
static constexpr uint8_t kEncodedTrue =
    EncodeInitialByte(MajorType::SIMPLE_VALUE, 21);
static constexpr uint8_t kEncodedFalse =
    EncodeInitialByte(MajorType::SIMPLE_VALUE, 20);
static constexpr uint8_t kEncodedNull =
    EncodeInitialByte(MajorType::SIMPLE_VALUE, 22);
static constexpr uint8_t kInitialByteForDouble =
    EncodeInitialByte(MajorType::SIMPLE_VALUE, 27);

// See RFC 7049 Table 3 and Section 2.4.4.2. This is used as a prefix for
// arbitrary binary data encoded as BYTE_STRING.
static constexpr uint8_t kExpectedConversionToBase64Tag =
    EncodeInitialByte(MajorType::TAG, 22);

// Writes the bytes for |v| to |out|, starting with the most significant byte.
// See also: https://commandcenter.blogspot.com/2012/04/byte-order-fallacy.html
template <typename T, class C>
void WriteBytesMostSignificantByteFirst(T v, C* out) {
  for (int shift_bytes = sizeof(T) - 1; shift_bytes >= 0; --shift_bytes)
    out->push_back(0xff & (v >> (shift_bytes * 8)));
}

// Extracts sizeof(T) bytes from |in| to extract a value of type T
// (e.g. uint64_t, uint32_t, ...), most significant byte first.
// See also: https://commandcenter.blogspot.com/2012/04/byte-order-fallacy.html
template <typename T>
T ReadBytesMostSignificantByteFirst(span<uint8_t> in) {
  assert(in.size() >= sizeof(T));
  T result = 0;
  for (size_t shift_bytes = 0; shift_bytes < sizeof(T); ++shift_bytes)
    result |= T(in[sizeof(T) - 1 - shift_bytes]) << (shift_bytes * 8);
  return result;
}
}  // namespace

namespace internals {
// Reads the start of a token with definitive size from |bytes|.
// |type| is the major type as specified in RFC 7049 Section 2.1.
// |value| is the payload (e.g. for MajorType::UNSIGNED) or is the size
// (e.g. for BYTE_STRING).
// If successful, returns the number of bytes read. Otherwise returns 0.
size_t ReadTokenStart(span<uint8_t> bytes, MajorType* type, uint64_t* value) {
  if (bytes.empty())
    return 0;
  uint8_t initial_byte = bytes[0];
  *type = MajorType((initial_byte & kMajorTypeMask) >> kMajorTypeBitShift);

  uint8_t additional_information = initial_byte & kAdditionalInformationMask;
  if (additional_information < 24) {
    // Values 0-23 are encoded directly into the additional info of the
    // initial byte.
    *value = additional_information;
    return 1;
  }
  if (additional_information == kAdditionalInformation1Byte) {
    // Values 24-255 are encoded with one initial byte, followed by the value.
    if (bytes.size() < 2)
      return 0;
    *value = ReadBytesMostSignificantByteFirst<uint8_t>(bytes.subspan(1));
    return 2;
  }
  if (additional_information == kAdditionalInformation2Bytes) {
    // Values 256-65535: 1 initial byte + 2 bytes payload.
    if (bytes.size() < 1 + sizeof(uint16_t))
      return 0;
    *value = ReadBytesMostSignificantByteFirst<uint16_t>(bytes.subspan(1));
    return 3;
  }
  if (additional_information == kAdditionalInformation4Bytes) {
    // 32 bit uint: 1 initial byte + 4 bytes payload.
    if (bytes.size() < 1 + sizeof(uint32_t))
      return 0;
    *value = ReadBytesMostSignificantByteFirst<uint32_t>(bytes.subspan(1));
    return 5;
  }
  if (additional_information == kAdditionalInformation8Bytes) {
    // 64 bit uint: 1 initial byte + 8 bytes payload.
    if (bytes.size() < 1 + sizeof(uint64_t))
      return 0;
    *value = ReadBytesMostSignificantByteFirst<uint64_t>(bytes.subspan(1));
    return 9;
  }
  return 0;
}

// Writes the start of a token with |type|. The |value| may indicate the size,
// or it may be the payload if the value is an unsigned integer.
template <typename C>
void WriteTokenStartTmpl(MajorType type, uint64_t value, C* encoded) {
  if (value < 24) {
    // Values 0-23 are encoded directly into the additional info of the
    // initial byte.
    encoded->push_back(EncodeInitialByte(type, /*additional_info=*/value));
    return;
  }
  if (value <= std::numeric_limits<uint8_t>::max()) {
    // Values 24-255 are encoded with one initial byte, followed by the value.
    encoded->push_back(EncodeInitialByte(type, kAdditionalInformation1Byte));
    encoded->push_back(value);
    return;
  }
  if (value <= std::numeric_limits<uint16_t>::max()) {
    // Values 256-65535: 1 initial byte + 2 bytes payload.
    encoded->push_back(EncodeInitialByte(type, kAdditionalInformation2Bytes));
    WriteBytesMostSignificantByteFirst<uint16_t>(value, encoded);
    return;
  }
  if (value <= std::numeric_limits<uint32_t>::max()) {
    // 32 bit uint: 1 initial byte + 4 bytes payload.
    encoded->push_back(EncodeInitialByte(type, kAdditionalInformation4Bytes));
    WriteBytesMostSignificantByteFirst<uint32_t>(static_cast<uint32_t>(value),
                                                 encoded);
    return;
  }
  // 64 bit uint: 1 initial byte + 8 bytes payload.
  encoded->push_back(EncodeInitialByte(type, kAdditionalInformation8Bytes));
  WriteBytesMostSignificantByteFirst<uint64_t>(value, encoded);
}
void WriteTokenStart(MajorType type,
                     uint64_t value,
                     std::vector<uint8_t>* encoded) {
  WriteTokenStartTmpl(type, value, encoded);
}
void WriteTokenStart(MajorType type, uint64_t value, std::string* encoded) {
  WriteTokenStartTmpl(type, value, encoded);
}
}  // namespace internals

// =============================================================================
// Detecting CBOR content
// =============================================================================

uint8_t InitialByteForEnvelope() {
  return kInitialByteForEnvelope;
}
uint8_t InitialByteFor32BitLengthByteString() {
  return kInitialByteFor32BitLengthByteString;
}
bool IsCBORMessage(span<uint8_t> msg) {
  return msg.size() >= 6 && msg[0] == InitialByteForEnvelope() &&
         msg[1] == InitialByteFor32BitLengthByteString();
}

// =============================================================================
// Encoding invidiual CBOR items
// =============================================================================

uint8_t EncodeTrue() {
  return kEncodedTrue;
}
uint8_t EncodeFalse() {
  return kEncodedFalse;
}
uint8_t EncodeNull() {
  return kEncodedNull;
}

uint8_t EncodeIndefiniteLengthArrayStart() {
  return kInitialByteIndefiniteLengthArray;
}

uint8_t EncodeIndefiniteLengthMapStart() {
  return kInitialByteIndefiniteLengthMap;
}

uint8_t EncodeStop() {
  return kStopByte;
}

template <typename C>
void EncodeInt32Tmpl(int32_t value, C* out) {
  if (value >= 0) {
    internals::WriteTokenStart(MajorType::UNSIGNED, value, out);
  } else {
    uint64_t representation = static_cast<uint64_t>(-(value + 1));
    internals::WriteTokenStart(MajorType::NEGATIVE, representation, out);
  }
}
void EncodeInt32(int32_t value, std::vector<uint8_t>* out) {
  EncodeInt32Tmpl(value, out);
}
void EncodeInt32(int32_t value, std::string* out) {
  EncodeInt32Tmpl(value, out);
}

template <typename C>
void EncodeString16Tmpl(span<uint16_t> in, C* out) {
  uint64_t byte_length = static_cast<uint64_t>(in.size_bytes());
  internals::WriteTokenStart(MajorType::BYTE_STRING, byte_length, out);
  // When emitting UTF16 characters, we always write the least significant byte
  // first; this is because it's the native representation for X86.
  // TODO(johannes): Implement a more efficient thing here later, e.g.
  // casting *iff* the machine has this byte order.
  // The wire format for UTF16 chars will probably remain the same
  // (least significant byte first) since this way we can have
  // golden files, unittests, etc. that port easily and universally.
  // See also:
  // https://commandcenter.blogspot.com/2012/04/byte-order-fallacy.html
  for (const uint16_t two_bytes : in) {
    out->push_back(two_bytes);
    out->push_back(two_bytes >> 8);
  }
}
void EncodeString16(span<uint16_t> in, std::vector<uint8_t>* out) {
  EncodeString16Tmpl(in, out);
}
void EncodeString16(span<uint16_t> in, std::string* out) {
  EncodeString16Tmpl(in, out);
}

template <typename C>
void EncodeString8Tmpl(span<uint8_t> in, C* out) {
  internals::WriteTokenStart(MajorType::STRING,
                             static_cast<uint64_t>(in.size_bytes()), out);
  out->insert(out->end(), in.begin(), in.end());
}
void EncodeString8(span<uint8_t> in, std::vector<uint8_t>* out) {
  EncodeString8Tmpl(in, out);
}
void EncodeString8(span<uint8_t> in, std::string* out) {
  EncodeString8Tmpl(in, out);
}

template <typename C>
void EncodeFromLatin1Tmpl(span<uint8_t> latin1, C* out) {
  for (size_t ii = 0; ii < latin1.size(); ++ii) {
    if (latin1[ii] <= 127)
      continue;
    // If there's at least one non-ASCII char, convert to UTF8.
    std::vector<uint8_t> utf8(latin1.begin(), latin1.begin() + ii);
    for (; ii < latin1.size(); ++ii) {
      if (latin1[ii] <= 127) {
        utf8.push_back(latin1[ii]);
      } else {
        // 0xC0 means it's a UTF8 sequence with 2 bytes.
        utf8.push_back((latin1[ii] >> 6) | 0xc0);
        utf8.push_back((latin1[ii] | 0x80) & 0xbf);
      }
    }
    EncodeString8(SpanFrom(utf8), out);
    return;
  }
  EncodeString8(latin1, out);
}
void EncodeFromLatin1(span<uint8_t> latin1, std::vector<uint8_t>* out) {
  EncodeFromLatin1Tmpl(latin1, out);
}
void EncodeFromLatin1(span<uint8_t> latin1, std::string* out) {
  EncodeFromLatin1Tmpl(latin1, out);
}

template <typename C>
void EncodeFromUTF16Tmpl(span<uint16_t> utf16, C* out) {
  // If there's at least one non-ASCII char, encode as STRING16 (UTF16).
  for (uint16_t ch : utf16) {
    if (ch <= 127)
      continue;
    EncodeString16(utf16, out);
    return;
  }
  // It's all US-ASCII, strip out every second byte and encode as UTF8.
  internals::WriteTokenStart(MajorType::STRING,
                             static_cast<uint64_t>(utf16.size()), out);
  out->insert(out->end(), utf16.begin(), utf16.end());
}
void EncodeFromUTF16(span<uint16_t> utf16, std::vector<uint8_t>* out) {
  EncodeFromUTF16Tmpl(utf16, out);
}
void EncodeFromUTF16(span<uint16_t> utf16, std::string* out) {
  EncodeFromUTF16Tmpl(utf16, out);
}

template <typename C>
void EncodeBinaryTmpl(span<uint8_t> in, C* out) {
  out->push_back(kExpectedConversionToBase64Tag);
  uint64_t byte_length = static_cast<uint64_t>(in.size_bytes());
  internals::WriteTokenStart(MajorType::BYTE_STRING, byte_length, out);
  out->insert(out->end(), in.begin(), in.end());
}
void EncodeBinary(span<uint8_t> in, std::vector<uint8_t>* out) {
  EncodeBinaryTmpl(in, out);
}
void EncodeBinary(span<uint8_t> in, std::string* out) {
  EncodeBinaryTmpl(in, out);
}

// A double is encoded with a specific initial byte
// (kInitialByteForDouble) plus the 64 bits of payload for its value.
constexpr size_t kEncodedDoubleSize = 1 + sizeof(uint64_t);

// An envelope is encoded with a specific initial byte
// (kInitialByteForEnvelope), plus the start byte for a BYTE_STRING with a 32
// bit wide length, plus a 32 bit length for that string.
constexpr size_t kEncodedEnvelopeHeaderSize = 1 + 1 + sizeof(uint32_t);

template <typename C>
void EncodeDoubleTmpl(double value, C* out) {
  // The additional_info=27 indicates 64 bits for the double follow.
  // See RFC 7049 Section 2.3, Table 1.
  out->push_back(kInitialByteForDouble);
  union {
    double from_double;
    uint64_t to_uint64;
  } reinterpret;
  reinterpret.from_double = value;
  WriteBytesMostSignificantByteFirst<uint64_t>(reinterpret.to_uint64, out);
}
void EncodeDouble(double value, std::vector<uint8_t>* out) {
  EncodeDoubleTmpl(value, out);
}
void EncodeDouble(double value, std::string* out) {
  EncodeDoubleTmpl(value, out);
}

// =============================================================================
// cbor::EnvelopeEncoder - for wrapping submessages
// =============================================================================

template <typename C>
void EncodeStartTmpl(C* out, size_t* byte_size_pos) {
  assert(*byte_size_pos == 0);
  out->push_back(kInitialByteForEnvelope);
  out->push_back(kInitialByteFor32BitLengthByteString);
  *byte_size_pos = out->size();
  out->resize(out->size() + sizeof(uint32_t));
}

void EnvelopeEncoder::EncodeStart(std::vector<uint8_t>* out) {
  EncodeStartTmpl<std::vector<uint8_t>>(out, &byte_size_pos_);
}

void EnvelopeEncoder::EncodeStart(std::string* out) {
  EncodeStartTmpl<std::string>(out, &byte_size_pos_);
}

template <typename C>
bool EncodeStopTmpl(C* out, size_t* byte_size_pos) {
  assert(*byte_size_pos != 0);
  // The byte size is the size of the payload, that is, all the
  // bytes that were written past the byte size position itself.
  uint64_t byte_size = out->size() - (*byte_size_pos + sizeof(uint32_t));
  // We store exactly 4 bytes, so at most INT32MAX, with most significant
  // byte first.
  if (byte_size > std::numeric_limits<uint32_t>::max())
    return false;
  for (int shift_bytes = sizeof(uint32_t) - 1; shift_bytes >= 0;
       --shift_bytes) {
    (*out)[(*byte_size_pos)++] = 0xff & (byte_size >> (shift_bytes * 8));
  }
  return true;
}

bool EnvelopeEncoder::EncodeStop(std::vector<uint8_t>* out) {
  return EncodeStopTmpl(out, &byte_size_pos_);
}

bool EnvelopeEncoder::EncodeStop(std::string* out) {
  return EncodeStopTmpl(out, &byte_size_pos_);
}

// =============================================================================
// cbor::NewCBOREncoder - for encoding from a streaming parser
// =============================================================================

namespace {
template <typename C>
class CBOREncoder : public StreamingParserHandler {
 public:
  CBOREncoder(C* out, Status* status) : out_(out), status_(status) {
    *status_ = Status();
  }

  void HandleMapBegin() override {
    if (!status_->ok())
      return;
    envelopes_.emplace_back();
    envelopes_.back().EncodeStart(out_);
    out_->push_back(kInitialByteIndefiniteLengthMap);
  }

  void HandleMapEnd() override {
    if (!status_->ok())
      return;
    out_->push_back(kStopByte);
    assert(!envelopes_.empty());
    if (!envelopes_.back().EncodeStop(out_)) {
      HandleError(
          Status(Error::CBOR_ENVELOPE_SIZE_LIMIT_EXCEEDED, out_->size()));
      return;
    }
    envelopes_.pop_back();
  }

  void HandleArrayBegin() override {
    if (!status_->ok())
      return;
    envelopes_.emplace_back();
    envelopes_.back().EncodeStart(out_);
    out_->push_back(kInitialByteIndefiniteLengthArray);
  }

  void HandleArrayEnd() override {
    if (!status_->ok())
      return;
    out_->push_back(kStopByte);
    assert(!envelopes_.empty());
    if (!envelopes_.back().EncodeStop(out_)) {
      HandleError(
          Status(Error::CBOR_ENVELOPE_SIZE_LIMIT_EXCEEDED, out_->size()));
      return;
    }
    envelopes_.pop_back();
  }

  void HandleString8(span<uint8_t> chars) override {
    if (!status_->ok())
      return;
    EncodeString8(chars, out_);
  }

  void HandleString16(span<uint16_t> chars) override {
    if (!status_->ok())
      return;
    EncodeFromUTF16(chars, out_);
  }

  void HandleBinary(span<uint8_t> bytes) override {
    if (!status_->ok())
      return;
    EncodeBinary(bytes, out_);
  }

  void HandleDouble(double value) override {
    if (!status_->ok())
      return;
    EncodeDouble(value, out_);
  }

  void HandleInt32(int32_t value) override {
    if (!status_->ok())
      return;
    EncodeInt32(value, out_);
  }

  void HandleBool(bool value) override {
    if (!status_->ok())
      return;
    // See RFC 7049 Section 2.3, Table 2.
    out_->push_back(value ? kEncodedTrue : kEncodedFalse);
  }

  void HandleNull() override {
    if (!status_->ok())
      return;
    // See RFC 7049 Section 2.3, Table 2.
    out_->push_back(kEncodedNull);
  }

  void HandleError(Status error) override {
    if (!status_->ok())
      return;
    *status_ = error;
    out_->clear();
  }

 private:
  C* out_;
  std::vector<EnvelopeEncoder> envelopes_;
  Status* status_;
};
}  // namespace

std::unique_ptr<StreamingParserHandler> NewCBOREncoder(
    std::vector<uint8_t>* out,
    Status* status) {
  return std::unique_ptr<StreamingParserHandler>(
      new CBOREncoder<std::vector<uint8_t>>(out, status));
}
std::unique_ptr<StreamingParserHandler> NewCBOREncoder(std::string* out,
                                                       Status* status) {
  return std::unique_ptr<StreamingParserHandler>(
      new CBOREncoder<std::string>(out, status));
}

// =============================================================================
// cbor::CBORTokenizer - for parsing individual CBOR items
// =============================================================================

CBORTokenizer::CBORTokenizer(span<uint8_t> bytes) : bytes_(bytes) {
  ReadNextToken(/*enter_envelope=*/false);
}
CBORTokenizer::~CBORTokenizer() {}

CBORTokenTag CBORTokenizer::TokenTag() const {
  return token_tag_;
}

void CBORTokenizer::Next() {
  if (token_tag_ == CBORTokenTag::ERROR_VALUE ||
      token_tag_ == CBORTokenTag::DONE)
    return;
  ReadNextToken(/*enter_envelope=*/false);
}

void CBORTokenizer::EnterEnvelope() {
  assert(token_tag_ == CBORTokenTag::ENVELOPE);
  ReadNextToken(/*enter_envelope=*/true);
}

Status CBORTokenizer::Status() const {
  return status_;
}

// The following accessor functions ::GetInt32, ::GetDouble,
// ::GetString8, ::GetString16WireRep, ::GetBinary, ::GetEnvelopeContents
// assume that a particular token was recognized in ::ReadNextToken.
// That's where all the error checking is done. By design,
// the accessors (assuming the token was recognized) never produce
// an error.

int32_t CBORTokenizer::GetInt32() const {
  assert(token_tag_ == CBORTokenTag::INT32);
  // The range checks happen in ::ReadNextToken().
  return static_cast<int32_t>(
      token_start_type_ == MajorType::UNSIGNED
          ? token_start_internal_value_
          : -static_cast<int64_t>(token_start_internal_value_) - 1);
}

double CBORTokenizer::GetDouble() const {
  assert(token_tag_ == CBORTokenTag::DOUBLE);
  union {
    uint64_t from_uint64;
    double to_double;
  } reinterpret;
  reinterpret.from_uint64 = ReadBytesMostSignificantByteFirst<uint64_t>(
      bytes_.subspan(status_.pos + 1));
  return reinterpret.to_double;
}

span<uint8_t> CBORTokenizer::GetString8() const {
  assert(token_tag_ == CBORTokenTag::STRING8);
  auto length = static_cast<size_t>(token_start_internal_value_);
  return bytes_.subspan(status_.pos + (token_byte_length_ - length), length);
}

span<uint8_t> CBORTokenizer::GetString16WireRep() const {
  assert(token_tag_ == CBORTokenTag::STRING16);
  auto length = static_cast<size_t>(token_start_internal_value_);
  return bytes_.subspan(status_.pos + (token_byte_length_ - length), length);
}

span<uint8_t> CBORTokenizer::GetBinary() const {
  assert(token_tag_ == CBORTokenTag::BINARY);
  auto length = static_cast<size_t>(token_start_internal_value_);
  return bytes_.subspan(status_.pos + (token_byte_length_ - length), length);
}

span<uint8_t> CBORTokenizer::GetEnvelope() const {
  assert(token_tag_ == CBORTokenTag::ENVELOPE);
  auto length = static_cast<size_t>(token_start_internal_value_);
  return bytes_.subspan(status_.pos, length + kEncodedEnvelopeHeaderSize);
}

span<uint8_t> CBORTokenizer::GetEnvelopeContents() const {
  assert(token_tag_ == CBORTokenTag::ENVELOPE);
  auto length = static_cast<size_t>(token_start_internal_value_);
  return bytes_.subspan(status_.pos + kEncodedEnvelopeHeaderSize, length);
}

// All error checking happens in ::ReadNextToken, so that the accessors
// can avoid having to carry an error return value.
//
// With respect to checking the encoded lengths of strings, arrays, etc:
// On the wire, CBOR uses 1,2,4, and 8 byte unsigned integers, so
// we initially read them as uint64_t, usually into token_start_internal_value_.
//
// However, since these containers have a representation on the machine,
// we need to do corresponding size computations on the input byte array,
// output span (e.g. the payload for a string), etc., and size_t is
// machine specific (in practice either 32 bit or 64 bit).
//
// Further, we must avoid overflowing size_t. Therefore, we use this
// kMaxValidLength constant to:
// - Reject values that are larger than the architecture specific
//   max size_t (differs between 32 bit and 64 bit arch).
// - Reserve at least one bit so that we can check against overflows
//   when adding lengths (array / string length / etc.); we do this by
//   ensuring that the inputs to an addition are <= kMaxValidLength,
//   and then checking whether the sum went past it.
//
// See also
// https://chromium.googlesource.com/chromium/src/+/master/docs/security/integer-semantics.md
static const uint64_t kMaxValidLength =
    std::min<uint64_t>(std::numeric_limits<uint64_t>::max() >> 2,
                       std::numeric_limits<size_t>::max());

void CBORTokenizer::ReadNextToken(bool enter_envelope) {
  if (enter_envelope) {
    status_.pos += kEncodedEnvelopeHeaderSize;
  } else {
    status_.pos =
        status_.pos == Status::npos() ? 0 : status_.pos + token_byte_length_;
  }
  status_.error = Error::OK;
  if (status_.pos >= bytes_.size()) {
    token_tag_ = CBORTokenTag::DONE;
    return;
  }
  const size_t remaining_bytes = bytes_.size() - status_.pos;
  switch (bytes_[status_.pos]) {
    case kStopByte:
      SetToken(CBORTokenTag::STOP, 1);
      return;
    case kInitialByteIndefiniteLengthMap:
      SetToken(CBORTokenTag::MAP_START, 1);
      return;
    case kInitialByteIndefiniteLengthArray:
      SetToken(CBORTokenTag::ARRAY_START, 1);
      return;
    case kEncodedTrue:
      SetToken(CBORTokenTag::TRUE_VALUE, 1);
      return;
    case kEncodedFalse:
      SetToken(CBORTokenTag::FALSE_VALUE, 1);
      return;
    case kEncodedNull:
      SetToken(CBORTokenTag::NULL_VALUE, 1);
      return;
    case kExpectedConversionToBase64Tag: {  // BINARY
      const size_t bytes_read = internals::ReadTokenStart(
          bytes_.subspan(status_.pos + 1), &token_start_type_,
          &token_start_internal_value_);
      if (!bytes_read || token_start_type_ != MajorType::BYTE_STRING ||
          token_start_internal_value_ > kMaxValidLength) {
        SetError(Error::CBOR_INVALID_BINARY);
        return;
      }
      const uint64_t token_byte_length = token_start_internal_value_ +
                                         /* tag before token start: */ 1 +
                                         /* token start: */ bytes_read;
      if (token_byte_length > remaining_bytes) {
        SetError(Error::CBOR_INVALID_BINARY);
        return;
      }
      SetToken(CBORTokenTag::BINARY, static_cast<size_t>(token_byte_length));
      return;
    }
    case kInitialByteForDouble: {  // DOUBLE
      if (kEncodedDoubleSize > remaining_bytes) {
        SetError(Error::CBOR_INVALID_DOUBLE);
        return;
      }
      SetToken(CBORTokenTag::DOUBLE, kEncodedDoubleSize);
      return;
    }
    case kInitialByteForEnvelope: {  // ENVELOPE
      if (kEncodedEnvelopeHeaderSize > remaining_bytes) {
        SetError(Error::CBOR_INVALID_ENVELOPE);
        return;
      }
      // The envelope must be a byte string with 32 bit length.
      if (bytes_[status_.pos + 1] != kInitialByteFor32BitLengthByteString) {
        SetError(Error::CBOR_INVALID_ENVELOPE);
        return;
      }
      // Read the length of the byte string.
      token_start_internal_value_ = ReadBytesMostSignificantByteFirst<uint32_t>(
          bytes_.subspan(status_.pos + 2));
      if (token_start_internal_value_ > kMaxValidLength) {
        SetError(Error::CBOR_INVALID_ENVELOPE);
        return;
      }
      uint64_t token_byte_length =
          token_start_internal_value_ + kEncodedEnvelopeHeaderSize;
      if (token_byte_length > remaining_bytes) {
        SetError(Error::CBOR_INVALID_ENVELOPE);
        return;
      }
      SetToken(CBORTokenTag::ENVELOPE, static_cast<size_t>(token_byte_length));
      return;
    }
    default: {
      const size_t bytes_read = internals::ReadTokenStart(
          bytes_.subspan(status_.pos), &token_start_type_,
          &token_start_internal_value_);
      switch (token_start_type_) {
        case MajorType::UNSIGNED:  // INT32.
          // INT32 is a signed int32 (int32 makes sense for the
          // inspector_protocol, it's not a CBOR limitation), so we check
          // against the signed max, so that the allowable values are
          // 0, 1, 2, ... 2^31 - 1.
          if (!bytes_read || std::numeric_limits<int32_t>::max() <
                                 token_start_internal_value_) {
            SetError(Error::CBOR_INVALID_INT32);
            return;
          }
          SetToken(CBORTokenTag::INT32, bytes_read);
          return;
        case MajorType::NEGATIVE: {  // INT32.
          // INT32 is a signed int32 (int32 makes sense for the
          // inspector_protocol, it's not a CBOR limitation); in CBOR, the
          // negative values for INT32 are represented as NEGATIVE, that is, -1
          // INT32 is represented as 1 << 5 | 0 (major type 1, additional info
          // value 0).
          // The represented allowed values range is -1 to -2^31.
          // They are mapped into the encoded range of 0 to 2^31-1.
          // We check the payload in token_start_internal_value_ against
          // that range (2^31-1 is also known as
          // std::numeric_limits<int32_t>::max()).
          if (!bytes_read || token_start_internal_value_ >
                                 std::numeric_limits<int32_t>::max()) {
            SetError(Error::CBOR_INVALID_INT32);
            return;
          }
          SetToken(CBORTokenTag::INT32, bytes_read);
          return;
        }
        case MajorType::STRING: {  // STRING8.
          if (!bytes_read || token_start_internal_value_ > kMaxValidLength) {
            SetError(Error::CBOR_INVALID_STRING8);
            return;
          }
          uint64_t token_byte_length = token_start_internal_value_ + bytes_read;
          if (token_byte_length > remaining_bytes) {
            SetError(Error::CBOR_INVALID_STRING8);
            return;
          }
          SetToken(CBORTokenTag::STRING8,
                   static_cast<size_t>(token_byte_length));
          return;
        }
        case MajorType::BYTE_STRING: {  // STRING16.
          // Length must be divisible by 2 since UTF16 is 2 bytes per
          // character, hence the &1 check.
          if (!bytes_read || token_start_internal_value_ > kMaxValidLength ||
              token_start_internal_value_ & 1) {
            SetError(Error::CBOR_INVALID_STRING16);
            return;
          }
          uint64_t token_byte_length = token_start_internal_value_ + bytes_read;
          if (token_byte_length > remaining_bytes) {
            SetError(Error::CBOR_INVALID_STRING16);
            return;
          }
          SetToken(CBORTokenTag::STRING16,
                   static_cast<size_t>(token_byte_length));
          return;
        }
        case MajorType::ARRAY:
        case MajorType::MAP:
        case MajorType::TAG:
        case MajorType::SIMPLE_VALUE:
          SetError(Error::CBOR_UNSUPPORTED_VALUE);
          return;
      }
    }
  }
}

void CBORTokenizer::SetToken(CBORTokenTag token_tag, size_t token_byte_length) {
  token_tag_ = token_tag;
  token_byte_length_ = token_byte_length;
}

void CBORTokenizer::SetError(Error error) {
  token_tag_ = CBORTokenTag::ERROR_VALUE;
  status_.error = error;
}

// =============================================================================
// cbor::ParseCBOR - for receiving streaming parser events for CBOR messages
// =============================================================================

namespace {
// When parsing CBOR, we limit recursion depth for objects and arrays
// to this constant.
static constexpr int kStackLimit = 300;

// Below are three parsing routines for CBOR, which cover enough
// to roundtrip JSON messages.
bool ParseMap(int32_t stack_depth,
              CBORTokenizer* tokenizer,
              StreamingParserHandler* out);
bool ParseArray(int32_t stack_depth,
                CBORTokenizer* tokenizer,
                StreamingParserHandler* out);
bool ParseValue(int32_t stack_depth,
                CBORTokenizer* tokenizer,
                StreamingParserHandler* out);
bool ParseEnvelope(int32_t stack_depth,
                   CBORTokenizer* tokenizer,
                   StreamingParserHandler* out);

void ParseUTF16String(CBORTokenizer* tokenizer, StreamingParserHandler* out) {
  std::vector<uint16_t> value;
  span<uint8_t> rep = tokenizer->GetString16WireRep();
  for (size_t ii = 0; ii < rep.size(); ii += 2)
    value.push_back((rep[ii + 1] << 8) | rep[ii]);
  out->HandleString16(span<uint16_t>(value.data(), value.size()));
  tokenizer->Next();
}

bool ParseUTF8String(CBORTokenizer* tokenizer, StreamingParserHandler* out) {
  assert(tokenizer->TokenTag() == CBORTokenTag::STRING8);
  out->HandleString8(tokenizer->GetString8());
  tokenizer->Next();
  return true;
}

bool ParseEnvelope(int32_t stack_depth,
                   CBORTokenizer* tokenizer,
                   StreamingParserHandler* out) {
  assert(tokenizer->TokenTag() == CBORTokenTag::ENVELOPE);
  // Before we enter the envelope, we save the position that we
  // expect to see after we're done parsing the envelope contents.
  // This way we can compare and produce an error if the contents
  // didn't fit exactly into the envelope length.
  size_t pos_past_envelope = tokenizer->Status().pos +
                             kEncodedEnvelopeHeaderSize +
                             tokenizer->GetEnvelopeContents().size();
  tokenizer->EnterEnvelope();
  switch (tokenizer->TokenTag()) {
    case CBORTokenTag::ERROR_VALUE:
      out->HandleError(tokenizer->Status());
      return false;
    case CBORTokenTag::MAP_START:
      if (!ParseMap(stack_depth + 1, tokenizer, out))
        return false;
      break;  // Continue to check pos_past_envelope below.
    case CBORTokenTag::ARRAY_START:
      if (stack_depth == 0) {  // Not allowed at the top level.
        out->HandleError(
            Status{Error::CBOR_MAP_START_EXPECTED, tokenizer->Status().pos});
        return false;
      }
      if (!ParseArray(stack_depth + 1, tokenizer, out))
        return false;
      break;  // Continue to check pos_past_envelope below.
    default:
      out->HandleError(Status{
          stack_depth == 0 ? Error::CBOR_MAP_START_EXPECTED
                           : Error::CBOR_MAP_OR_ARRAY_EXPECTED_IN_ENVELOPE,
          tokenizer->Status().pos});
      return false;
  }
  // The contents of the envelope parsed OK, now check that we're at
  // the expected position.
  if (pos_past_envelope != tokenizer->Status().pos) {
    out->HandleError(Status{Error::CBOR_ENVELOPE_CONTENTS_LENGTH_MISMATCH,
                            tokenizer->Status().pos});
    return false;
  }
  return true;
}

bool ParseValue(int32_t stack_depth,
                CBORTokenizer* tokenizer,
                StreamingParserHandler* out) {
  if (stack_depth > kStackLimit) {
    out->HandleError(
        Status{Error::CBOR_STACK_LIMIT_EXCEEDED, tokenizer->Status().pos});
    return false;
  }
  switch (tokenizer->TokenTag()) {
    case CBORTokenTag::ERROR_VALUE:
      out->HandleError(tokenizer->Status());
      return false;
    case CBORTokenTag::DONE:
      out->HandleError(Status{Error::CBOR_UNEXPECTED_EOF_EXPECTED_VALUE,
                              tokenizer->Status().pos});
      return false;
    case CBORTokenTag::ENVELOPE:
      return ParseEnvelope(stack_depth, tokenizer, out);
    case CBORTokenTag::TRUE_VALUE:
      out->HandleBool(true);
      tokenizer->Next();
      return true;
    case CBORTokenTag::FALSE_VALUE:
      out->HandleBool(false);
      tokenizer->Next();
      return true;
    case CBORTokenTag::NULL_VALUE:
      out->HandleNull();
      tokenizer->Next();
      return true;
    case CBORTokenTag::INT32:
      out->HandleInt32(tokenizer->GetInt32());
      tokenizer->Next();
      return true;
    case CBORTokenTag::DOUBLE:
      out->HandleDouble(tokenizer->GetDouble());
      tokenizer->Next();
      return true;
    case CBORTokenTag::STRING8:
      return ParseUTF8String(tokenizer, out);
    case CBORTokenTag::STRING16:
      ParseUTF16String(tokenizer, out);
      return true;
    case CBORTokenTag::BINARY: {
      out->HandleBinary(tokenizer->GetBinary());
      tokenizer->Next();
      return true;
    }
    case CBORTokenTag::MAP_START:
      return ParseMap(stack_depth + 1, tokenizer, out);
    case CBORTokenTag::ARRAY_START:
      return ParseArray(stack_depth + 1, tokenizer, out);
    default:
      out->HandleError(
          Status{Error::CBOR_UNSUPPORTED_VALUE, tokenizer->Status().pos});
      return false;
  }
}

// |bytes| must start with the indefinite length array byte, so basically,
// ParseArray may only be called after an indefinite length array has been
// detected.
bool ParseArray(int32_t stack_depth,
                CBORTokenizer* tokenizer,
                StreamingParserHandler* out) {
  assert(tokenizer->TokenTag() == CBORTokenTag::ARRAY_START);
  tokenizer->Next();
  out->HandleArrayBegin();
  while (tokenizer->TokenTag() != CBORTokenTag::STOP) {
    if (tokenizer->TokenTag() == CBORTokenTag::DONE) {
      out->HandleError(
          Status{Error::CBOR_UNEXPECTED_EOF_IN_ARRAY, tokenizer->Status().pos});
      return false;
    }
    if (tokenizer->TokenTag() == CBORTokenTag::ERROR_VALUE) {
      out->HandleError(tokenizer->Status());
      return false;
    }
    // Parse value.
    if (!ParseValue(stack_depth, tokenizer, out))
      return false;
  }
  out->HandleArrayEnd();
  tokenizer->Next();
  return true;
}

// |bytes| must start with the indefinite length array byte, so basically,
// ParseArray may only be called after an indefinite length array has been
// detected.
bool ParseMap(int32_t stack_depth,
              CBORTokenizer* tokenizer,
              StreamingParserHandler* out) {
  assert(tokenizer->TokenTag() == CBORTokenTag::MAP_START);
  out->HandleMapBegin();
  tokenizer->Next();
  while (tokenizer->TokenTag() != CBORTokenTag::STOP) {
    if (tokenizer->TokenTag() == CBORTokenTag::DONE) {
      out->HandleError(
          Status{Error::CBOR_UNEXPECTED_EOF_IN_MAP, tokenizer->Status().pos});
      return false;
    }
    if (tokenizer->TokenTag() == CBORTokenTag::ERROR_VALUE) {
      out->HandleError(tokenizer->Status());
      return false;
    }
    // Parse key.
    if (tokenizer->TokenTag() == CBORTokenTag::STRING8) {
      if (!ParseUTF8String(tokenizer, out))
        return false;
    } else if (tokenizer->TokenTag() == CBORTokenTag::STRING16) {
      ParseUTF16String(tokenizer, out);
    } else {
      out->HandleError(
          Status{Error::CBOR_INVALID_MAP_KEY, tokenizer->Status().pos});
      return false;
    }
    // Parse value.
    if (!ParseValue(stack_depth, tokenizer, out))
      return false;
  }
  out->HandleMapEnd();
  tokenizer->Next();
  return true;
}
}  // namespace

void ParseCBOR(span<uint8_t> bytes, StreamingParserHandler* out) {
  if (bytes.empty()) {
    out->HandleError(Status{Error::CBOR_NO_INPUT, 0});
    return;
  }
  if (bytes[0] != kInitialByteForEnvelope) {
    out->HandleError(Status{Error::CBOR_INVALID_START_BYTE, 0});
    return;
  }
  CBORTokenizer tokenizer(bytes);
  if (tokenizer.TokenTag() == CBORTokenTag::ERROR_VALUE) {
    out->HandleError(tokenizer.Status());
    return;
  }
  // We checked for the envelope start byte above, so the tokenizer
  // must agree here, since it's not an error.
  assert(tokenizer.TokenTag() == CBORTokenTag::ENVELOPE);
  if (!ParseEnvelope(/*stack_depth=*/0, &tokenizer, out))
    return;
  if (tokenizer.TokenTag() == CBORTokenTag::DONE)
    return;
  if (tokenizer.TokenTag() == CBORTokenTag::ERROR_VALUE) {
    out->HandleError(tokenizer.Status());
    return;
  }
  out->HandleError(Status{Error::CBOR_TRAILING_JUNK, tokenizer.Status().pos});
}

// =============================================================================
// cbor::AppendString8EntryToMap - for limited in-place editing of messages
// =============================================================================

template <typename C>
Status AppendString8EntryToCBORMapTmpl(span<uint8_t> string8_key,
                                       span<uint8_t> string8_value,
                                       C* cbor) {
  // Careful below: Don't compare (*cbor)[idx] with a uint8_t, since
  // it could be a char (signed!). Instead, use bytes.
  span<uint8_t> bytes(reinterpret_cast<const uint8_t*>(cbor->data()),
                      cbor->size());
  CBORTokenizer tokenizer(bytes);
  if (tokenizer.TokenTag() == CBORTokenTag::ERROR_VALUE)
    return tokenizer.Status();
  if (tokenizer.TokenTag() != CBORTokenTag::ENVELOPE)
    return Status(Error::CBOR_INVALID_ENVELOPE, 0);
  size_t envelope_size = tokenizer.GetEnvelopeContents().size();
  size_t old_size = cbor->size();
  if (old_size != envelope_size + kEncodedEnvelopeHeaderSize)
    return Status(Error::CBOR_INVALID_ENVELOPE, 0);
  if (envelope_size == 0 ||
      (tokenizer.GetEnvelopeContents()[0] != EncodeIndefiniteLengthMapStart()))
    return Status(Error::CBOR_MAP_START_EXPECTED, kEncodedEnvelopeHeaderSize);
  if (bytes[bytes.size() - 1] != EncodeStop())
    return Status(Error::CBOR_MAP_STOP_EXPECTED, cbor->size() - 1);
  cbor->pop_back();
  EncodeString8(string8_key, cbor);
  EncodeString8(string8_value, cbor);
  cbor->push_back(EncodeStop());
  size_t new_envelope_size = envelope_size + (cbor->size() - old_size);
  if (new_envelope_size > std::numeric_limits<uint32_t>::max())
    return Status(Error::CBOR_ENVELOPE_SIZE_LIMIT_EXCEEDED, 0);
  size_t size_pos = cbor->size() - new_envelope_size - sizeof(uint32_t);
  uint8_t* out = reinterpret_cast<uint8_t*>(&cbor->at(size_pos));
  *(out++) = (new_envelope_size >> 24) & 0xff;
  *(out++) = (new_envelope_size >> 16) & 0xff;
  *(out++) = (new_envelope_size >> 8) & 0xff;
  *(out) = new_envelope_size & 0xff;
  return Status();
}
Status AppendString8EntryToCBORMap(span<uint8_t> string8_key,
                                   span<uint8_t> string8_value,
                                   std::vector<uint8_t>* cbor) {
  return AppendString8EntryToCBORMapTmpl(string8_key, string8_value, cbor);
}
Status AppendString8EntryToCBORMap(span<uint8_t> string8_key,
                                   span<uint8_t> string8_value,
                                   std::string* cbor) {
  return AppendString8EntryToCBORMapTmpl(string8_key, string8_value, cbor);
}
}  // namespace cbor

namespace json {

// =============================================================================
// json::NewJSONEncoder - for encoding streaming parser events as JSON
// =============================================================================

namespace {
// Prints |value| to |out| with 4 hex digits, most significant chunk first.
template <typename C>
void PrintHex(uint16_t value, C* out) {
  for (int ii = 3; ii >= 0; --ii) {
    int four_bits = 0xf & (value >> (4 * ii));
    out->push_back(four_bits + ((four_bits <= 9) ? '0' : ('a' - 10)));
  }
}

// In the writer below, we maintain a stack of State instances.
// It is just enough to emit the appropriate delimiters and brackets
// in JSON.
enum class Container {
  // Used for the top-level, initial state.
  NONE,
  // Inside a JSON object.
  MAP,
  // Inside a JSON array.
  ARRAY
};
class State {
 public:
  explicit State(Container container) : container_(container) {}
  void StartElement(std::vector<uint8_t>* out) { StartElementTmpl(out); }
  void StartElement(std::string* out) { StartElementTmpl(out); }
  Container container() const { return container_; }

 private:
  template <typename C>
  void StartElementTmpl(C* out) {
    assert(container_ != Container::NONE || size_ == 0);
    if (size_ != 0) {
      char delim = (!(size_ & 1) || container_ == Container::ARRAY) ? ',' : ':';
      out->push_back(delim);
    }
    ++size_;
  }

  Container container_ = Container::NONE;
  int size_ = 0;
};

constexpr char kBase64Table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz0123456789+/";

template <typename C>
void Base64Encode(const span<uint8_t>& in, C* out) {
  // The following three cases are based on the tables in the example
  // section in https://en.wikipedia.org/wiki/Base64. We process three
  // input bytes at a time, emitting 4 output bytes at a time.
  size_t ii = 0;

  // While possible, process three input bytes.
  for (; ii + 3 <= in.size(); ii += 3) {
    uint32_t twentyfour_bits = (in[ii] << 16) | (in[ii + 1] << 8) | in[ii + 2];
    out->push_back(kBase64Table[(twentyfour_bits >> 18)]);
    out->push_back(kBase64Table[(twentyfour_bits >> 12) & 0x3f]);
    out->push_back(kBase64Table[(twentyfour_bits >> 6) & 0x3f]);
    out->push_back(kBase64Table[twentyfour_bits & 0x3f]);
  }
  if (ii + 2 <= in.size()) {  // Process two input bytes.
    uint32_t twentyfour_bits = (in[ii] << 16) | (in[ii + 1] << 8);
    out->push_back(kBase64Table[(twentyfour_bits >> 18)]);
    out->push_back(kBase64Table[(twentyfour_bits >> 12) & 0x3f]);
    out->push_back(kBase64Table[(twentyfour_bits >> 6) & 0x3f]);
    out->push_back('=');  // Emit padding.
    return;
  }
  if (ii + 1 <= in.size()) {  // Process a single input byte.
    uint32_t twentyfour_bits = (in[ii] << 16);
    out->push_back(kBase64Table[(twentyfour_bits >> 18)]);
    out->push_back(kBase64Table[(twentyfour_bits >> 12) & 0x3f]);
    out->push_back('=');  // Emit padding.
    out->push_back('=');  // Emit padding.
  }
}

// Implements a handler for JSON parser events to emit a JSON string.
template <typename C>
class JSONEncoder : public StreamingParserHandler {
 public:
  JSONEncoder(const Platform* platform, C* out, Status* status)
      : platform_(platform), out_(out), status_(status) {
    *status_ = Status();
    state_.emplace(Container::NONE);
  }

  void HandleMapBegin() override {
    if (!status_->ok())
      return;
    assert(!state_.empty());
    state_.top().StartElement(out_);
    state_.emplace(Container::MAP);
    Emit('{');
  }

  void HandleMapEnd() override {
    if (!status_->ok())
      return;
    assert(state_.size() >= 2 && state_.top().container() == Container::MAP);
    state_.pop();
    Emit('}');
  }

  void HandleArrayBegin() override {
    if (!status_->ok())
      return;
    state_.top().StartElement(out_);
    state_.emplace(Container::ARRAY);
    Emit('[');
  }

  void HandleArrayEnd() override {
    if (!status_->ok())
      return;
    assert(state_.size() >= 2 && state_.top().container() == Container::ARRAY);
    state_.pop();
    Emit(']');
  }

  void HandleString16(span<uint16_t> chars) override {
    if (!status_->ok())
      return;
    state_.top().StartElement(out_);
    Emit('"');
    for (const uint16_t ch : chars) {
      if (ch == '"') {
        Emit("\\\"");
      } else if (ch == '\\') {
        Emit("\\\\");
      } else if (ch == '\b') {
        Emit("\\b");
      } else if (ch == '\f') {
        Emit("\\f");
      } else if (ch == '\n') {
        Emit("\\n");
      } else if (ch == '\r') {
        Emit("\\r");
      } else if (ch == '\t') {
        Emit("\\t");
      } else if (ch >= 32 && ch <= 126) {
        Emit(ch);
      } else {
        Emit("\\u");
        PrintHex(ch, out_);
      }
    }
    Emit('"');
  }

  void HandleString8(span<uint8_t> chars) override {
    if (!status_->ok())
      return;
    state_.top().StartElement(out_);
    Emit('"');
    for (size_t ii = 0; ii < chars.size(); ++ii) {
      uint8_t c = chars[ii];
      if (c == '"') {
        Emit("\\\"");
      } else if (c == '\\') {
        Emit("\\\\");
      } else if (c == '\b') {
        Emit("\\b");
      } else if (c == '\f') {
        Emit("\\f");
      } else if (c == '\n') {
        Emit("\\n");
      } else if (c == '\r') {
        Emit("\\r");
      } else if (c == '\t') {
        Emit("\\t");
      } else if (c >= 32 && c <= 126) {
        Emit(c);
      } else if (c < 32) {
        Emit("\\u");
        PrintHex(static_cast<uint16_t>(c), out_);
      } else {
        // Inspect the leading byte to figure out how long the utf8
        // byte sequence is; while doing this initialize |codepoint|
        // with the first few bits.
        // See table in: https://en.wikipedia.org/wiki/UTF-8
        // byte one is 110x xxxx -> 2 byte utf8 sequence
        // byte one is 1110 xxxx -> 3 byte utf8 sequence
        // byte one is 1111 0xxx -> 4 byte utf8 sequence
        uint32_t codepoint;
        int num_bytes_left;
        if ((c & 0xe0) == 0xc0) {  // 2 byte utf8 sequence
          num_bytes_left = 1;
          codepoint = c & 0x1f;
        } else if ((c & 0xf0) == 0xe0) {  // 3 byte utf8 sequence
          num_bytes_left = 2;
          codepoint = c & 0x0f;
        } else if ((c & 0xf8) == 0xf0) {  // 4 byte utf8 sequence
          codepoint = c & 0x07;
          num_bytes_left = 3;
        } else {
          continue;  // invalid leading byte
        }

        // If we have enough bytes in our input, decode the remaining ones
        // belonging to this Unicode character into |codepoint|.
        if (ii + num_bytes_left >= chars.size())
          continue;
        while (num_bytes_left > 0) {
          c = chars[++ii];
          --num_bytes_left;
          // Check the next byte is a continuation byte, that is 10xx xxxx.
          if ((c & 0xc0) != 0x80)
            continue;
          codepoint = (codepoint << 6) | (c & 0x3f);
        }

        // Disallow overlong encodings for ascii characters, as these
        // would include " and other characters significant to JSON
        // string termination / control.
        if (codepoint <= 0x7f)
          continue;
        // Invalid in UTF8, and can't be represented in UTF16 anyway.
        if (codepoint > 0x10ffff)
          continue;

        // So, now we transcode to UTF16,
        // using the math described at https://en.wikipedia.org/wiki/UTF-16,
        // for either one or two 16 bit characters.
        if (codepoint < 0xffff) {
          Emit("\\u");
          PrintHex(static_cast<uint16_t>(codepoint), out_);
          continue;
        }
        codepoint -= 0x10000;
        // high surrogate
        Emit("\\u");
        PrintHex(static_cast<uint16_t>((codepoint >> 10) + 0xd800), out_);
        // low surrogate
        Emit("\\u");
        PrintHex(static_cast<uint16_t>((codepoint & 0x3ff) + 0xdc00), out_);
      }
    }
    Emit('"');
  }

  void HandleBinary(span<uint8_t> bytes) override {
    if (!status_->ok())
      return;
    state_.top().StartElement(out_);
    Emit('"');
    Base64Encode(bytes, out_);
    Emit('"');
  }

  void HandleDouble(double value) override {
    if (!status_->ok())
      return;
    state_.top().StartElement(out_);
    // JSON cannot represent NaN or Infinity. So, for compatibility,
    // we behave like the JSON object in web browsers: emit 'null'.
    if (!std::isfinite(value)) {
      Emit("null");
      return;
    }
    std::unique_ptr<char[]> str_value = platform_->DToStr(value);

    // DToStr may fail to emit a 0 before the decimal dot. E.g. this is
    // the case in base::NumberToString in Chromium (which is based on
    // dmg_fp). So, much like
    // https://cs.chromium.org/chromium/src/base/json/json_writer.cc
    // we probe for this and emit the leading 0 anyway if necessary.
    const char* chars = str_value.get();
    if (chars[0] == '.') {
      Emit('0');
    } else if (chars[0] == '-' && chars[1] == '.') {
      Emit("-0");
      ++chars;
    }
    Emit(chars);
  }

  void HandleInt32(int32_t value) override {
    if (!status_->ok())
      return;
    state_.top().StartElement(out_);
    Emit(std::to_string(value));
  }

  void HandleBool(bool value) override {
    if (!status_->ok())
      return;
    state_.top().StartElement(out_);
    Emit(value ? "true" : "false");
  }

  void HandleNull() override {
    if (!status_->ok())
      return;
    state_.top().StartElement(out_);
    Emit("null");
  }

  void HandleError(Status error) override {
    assert(!error.ok());
    *status_ = error;
    out_->clear();
  }

 private:
  void Emit(char c) { out_->push_back(c); }
  void Emit(const char* str) {
    out_->insert(out_->end(), str, str + strlen(str));
  }
  void Emit(const std::string& str) {
    out_->insert(out_->end(), str.begin(), str.end());
  }

  const Platform* platform_;
  C* out_;
  Status* status_;
  std::stack<State> state_;
};
}  // namespace

std::unique_ptr<StreamingParserHandler> NewJSONEncoder(
    const Platform* platform,
    std::vector<uint8_t>* out,
    Status* status) {
  return std::unique_ptr<StreamingParserHandler>(
      new JSONEncoder<std::vector<uint8_t>>(platform, out, status));
}
std::unique_ptr<StreamingParserHandler> NewJSONEncoder(const Platform* platform,
                                                       std::string* out,
                                                       Status* status) {
  return std::unique_ptr<StreamingParserHandler>(
      new JSONEncoder<std::string>(platform, out, status));
}

// =============================================================================
// json::ParseJSON - for receiving streaming parser events for JSON.
// =============================================================================

namespace {
const int kStackLimit = 300;

enum Token {
  ObjectBegin,
  ObjectEnd,
  ArrayBegin,
  ArrayEnd,
  StringLiteral,
  Number,
  BoolTrue,
  BoolFalse,
  NullToken,
  ListSeparator,
  ObjectPairSeparator,
  InvalidToken,
  NoInput
};

const char* const kNullString = "null";
const char* const kTrueString = "true";
const char* const kFalseString = "false";

template <typename Char>
class JsonParser {
 public:
  JsonParser(const Platform* platform, StreamingParserHandler* handler)
      : platform_(platform), handler_(handler) {}

  void Parse(const Char* start, size_t length) {
    start_pos_ = start;
    const Char* end = start + length;
    const Char* tokenEnd = nullptr;
    ParseValue(start, end, &tokenEnd, 0);
    if (error_)
      return;
    if (tokenEnd != end) {
      HandleError(Error::JSON_PARSER_UNPROCESSED_INPUT_REMAINS, tokenEnd);
    }
  }

 private:
  bool CharsToDouble(const uint16_t* chars, size_t length, double* result) {
    std::string buffer;
    buffer.reserve(length + 1);
    for (size_t ii = 0; ii < length; ++ii) {
      bool is_ascii = !(chars[ii] & ~0x7F);
      if (!is_ascii)
        return false;
      buffer.push_back(static_cast<char>(chars[ii]));
    }
    return platform_->StrToD(buffer.c_str(), result);
  }

  bool CharsToDouble(const uint8_t* chars, size_t length, double* result) {
    std::string buffer(reinterpret_cast<const char*>(chars), length);
    return platform_->StrToD(buffer.c_str(), result);
  }

  static bool ParseConstToken(const Char* start,
                              const Char* end,
                              const Char** token_end,
                              const char* token) {
    // |token| is \0 terminated, it's one of the constants at top of the file.
    while (start < end && *token != '\0' && *start++ == *token++) {
    }
    if (*token != '\0')
      return false;
    *token_end = start;
    return true;
  }

  static bool ReadInt(const Char* start,
                      const Char* end,
                      const Char** token_end,
                      bool allow_leading_zeros) {
    if (start == end)
      return false;
    bool has_leading_zero = '0' == *start;
    int length = 0;
    while (start < end && '0' <= *start && *start <= '9') {
      ++start;
      ++length;
    }
    if (!length)
      return false;
    if (!allow_leading_zeros && length > 1 && has_leading_zero)
      return false;
    *token_end = start;
    return true;
  }

  static bool ParseNumberToken(const Char* start,
                               const Char* end,
                               const Char** token_end) {
    // We just grab the number here. We validate the size in DecodeNumber.
    // According to RFC4627, a valid number is: [minus] int [frac] [exp]
    if (start == end)
      return false;
    Char c = *start;
    if ('-' == c)
      ++start;

    if (!ReadInt(start, end, &start, /*allow_leading_zeros=*/false))
      return false;
    if (start == end) {
      *token_end = start;
      return true;
    }

    // Optional fraction part
    c = *start;
    if ('.' == c) {
      ++start;
      if (!ReadInt(start, end, &start, /*allow_leading_zeros=*/true))
        return false;
      if (start == end) {
        *token_end = start;
        return true;
      }
      c = *start;
    }

    // Optional exponent part
    if ('e' == c || 'E' == c) {
      ++start;
      if (start == end)
        return false;
      c = *start;
      if ('-' == c || '+' == c) {
        ++start;
        if (start == end)
          return false;
      }
      if (!ReadInt(start, end, &start, /*allow_leading_zeros=*/true))
        return false;
    }

    *token_end = start;
    return true;
  }

  static bool ReadHexDigits(const Char* start,
                            const Char* end,
                            const Char** token_end,
                            int digits) {
    if (end - start < digits)
      return false;
    for (int i = 0; i < digits; ++i) {
      Char c = *start++;
      if (!(('0' <= c && c <= '9') || ('a' <= c && c <= 'f') ||
            ('A' <= c && c <= 'F')))
        return false;
    }
    *token_end = start;
    return true;
  }

  static bool ParseStringToken(const Char* start,
                               const Char* end,
                               const Char** token_end) {
    while (start < end) {
      Char c = *start++;
      if ('\\' == c) {
        if (start == end)
          return false;
        c = *start++;
        // Make sure the escaped char is valid.
        switch (c) {
          case 'x':
            if (!ReadHexDigits(start, end, &start, 2))
              return false;
            break;
          case 'u':
            if (!ReadHexDigits(start, end, &start, 4))
              return false;
            break;
          case '\\':
          case '/':
          case 'b':
          case 'f':
          case 'n':
          case 'r':
          case 't':
          case 'v':
          case '"':
            break;
          default:
            return false;
        }
      } else if ('"' == c) {
        *token_end = start;
        return true;
      }
    }
    return false;
  }

  static bool SkipComment(const Char* start,
                          const Char* end,
                          const Char** comment_end) {
    if (start == end)
      return false;

    if (*start != '/' || start + 1 >= end)
      return false;
    ++start;

    if (*start == '/') {
      // Single line comment, read to newline.
      for (++start; start < end; ++start) {
        if (*start == '\n' || *start == '\r') {
          *comment_end = start + 1;
          return true;
        }
      }
      *comment_end = end;
      // Comment reaches end-of-input, which is fine.
      return true;
    }

    if (*start == '*') {
      Char previous = '\0';
      // Block comment, read until end marker.
      for (++start; start < end; previous = *start++) {
        if (previous == '*' && *start == '/') {
          *comment_end = start + 1;
          return true;
        }
      }
      // Block comment must close before end-of-input.
      return false;
    }

    return false;
  }

  static bool IsSpaceOrNewLine(Char c) {
    // \v = vertial tab; \f = form feed page break.
    return c == ' ' || c == '\n' || c == '\v' || c == '\f' || c == '\r' ||
           c == '\t';
  }

  static void SkipWhitespaceAndComments(const Char* start,
                                        const Char* end,
                                        const Char** whitespace_end) {
    while (start < end) {
      if (IsSpaceOrNewLine(*start)) {
        ++start;
      } else if (*start == '/') {
        const Char* comment_end = nullptr;
        if (!SkipComment(start, end, &comment_end))
          break;
        start = comment_end;
      } else {
        break;
      }
    }
    *whitespace_end = start;
  }

  static Token ParseToken(const Char* start,
                          const Char* end,
                          const Char** tokenStart,
                          const Char** token_end) {
    SkipWhitespaceAndComments(start, end, tokenStart);
    start = *tokenStart;

    if (start == end)
      return NoInput;

    switch (*start) {
      case 'n':
        if (ParseConstToken(start, end, token_end, kNullString))
          return NullToken;
        break;
      case 't':
        if (ParseConstToken(start, end, token_end, kTrueString))
          return BoolTrue;
        break;
      case 'f':
        if (ParseConstToken(start, end, token_end, kFalseString))
          return BoolFalse;
        break;
      case '[':
        *token_end = start + 1;
        return ArrayBegin;
      case ']':
        *token_end = start + 1;
        return ArrayEnd;
      case ',':
        *token_end = start + 1;
        return ListSeparator;
      case '{':
        *token_end = start + 1;
        return ObjectBegin;
      case '}':
        *token_end = start + 1;
        return ObjectEnd;
      case ':':
        *token_end = start + 1;
        return ObjectPairSeparator;
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
      case '-':
        if (ParseNumberToken(start, end, token_end))
          return Number;
        break;
      case '"':
        if (ParseStringToken(start + 1, end, token_end))
          return StringLiteral;
        break;
    }
    return InvalidToken;
  }

  static int HexToInt(Char c) {
    if ('0' <= c && c <= '9')
      return c - '0';
    if ('A' <= c && c <= 'F')
      return c - 'A' + 10;
    if ('a' <= c && c <= 'f')
      return c - 'a' + 10;
    assert(false);  // Unreachable.
    return 0;
  }

  static bool DecodeString(const Char* start,
                           const Char* end,
                           std::vector<uint16_t>* output) {
    if (start == end)
      return true;
    if (start > end)
      return false;
    output->reserve(end - start);
    while (start < end) {
      uint16_t c = *start++;
      // If the |Char| we're dealing with is really a byte, then
      // we have utf8 here, and we need to check for multibyte characters
      // and transcode them to utf16 (either one or two utf16 chars).
      if (sizeof(Char) == sizeof(uint8_t) && c > 0x7f) {
        // Inspect the leading byte to figure out how long the utf8
        // byte sequence is; while doing this initialize |codepoint|
        // with the first few bits.
        // See table in: https://en.wikipedia.org/wiki/UTF-8
        // byte one is 110x xxxx -> 2 byte utf8 sequence
        // byte one is 1110 xxxx -> 3 byte utf8 sequence
        // byte one is 1111 0xxx -> 4 byte utf8 sequence
        uint32_t codepoint;
        int num_bytes_left;
        if ((c & 0xe0) == 0xc0) {  // 2 byte utf8 sequence
          num_bytes_left = 1;
          codepoint = c & 0x1f;
        } else if ((c & 0xf0) == 0xe0) {  // 3 byte utf8 sequence
          num_bytes_left = 2;
          codepoint = c & 0x0f;
        } else if ((c & 0xf8) == 0xf0) {  // 4 byte utf8 sequence
          codepoint = c & 0x07;
          num_bytes_left = 3;
        } else {
          return false;  // invalid leading byte
        }

        // If we have enough bytes in our inpput, decode the remaining ones
        // belonging to this Unicode character into |codepoint|.
        if (start + num_bytes_left > end)
          return false;
        while (num_bytes_left > 0) {
          c = *start++;
          --num_bytes_left;
          // Check the next byte is a continuation byte, that is 10xx xxxx.
          if ((c & 0xc0) != 0x80)
            return false;
          codepoint = (codepoint << 6) | (c & 0x3f);
        }

        // Disallow overlong encodings for ascii characters, as these
        // would include " and other characters significant to JSON
        // string termination / control.
        if (codepoint <= 0x7f)
          return false;
        // Invalid in UTF8, and can't be represented in UTF16 anyway.
        if (codepoint > 0x10ffff)
          return false;

        // So, now we transcode to UTF16,
        // using the math described at https://en.wikipedia.org/wiki/UTF-16,
        // for either one or two 16 bit characters.
        if (codepoint < 0xffff) {
          output->push_back(codepoint);
          continue;
        }
        codepoint -= 0x10000;
        output->push_back((codepoint >> 10) + 0xd800);    // high surrogate
        output->push_back((codepoint & 0x3ff) + 0xdc00);  // low surrogate
        continue;
      }
      if ('\\' != c) {
        output->push_back(c);
        continue;
      }
      if (start == end)
        return false;
      c = *start++;

      if (c == 'x') {
        // \x is not supported.
        return false;
      }

      switch (c) {
        case '"':
        case '/':
        case '\\':
          break;
        case 'b':
          c = '\b';
          break;
        case 'f':
          c = '\f';
          break;
        case 'n':
          c = '\n';
          break;
        case 'r':
          c = '\r';
          break;
        case 't':
          c = '\t';
          break;
        case 'v':
          c = '\v';
          break;
        case 'u':
          c = (HexToInt(*start) << 12) + (HexToInt(*(start + 1)) << 8) +
              (HexToInt(*(start + 2)) << 4) + HexToInt(*(start + 3));
          start += 4;
          break;
        default:
          return false;
      }
      output->push_back(c);
    }
    return true;
  }

  void ParseValue(const Char* start,
                  const Char* end,
                  const Char** value_token_end,
                  int depth) {
    if (depth > kStackLimit) {
      HandleError(Error::JSON_PARSER_STACK_LIMIT_EXCEEDED, start);
      return;
    }
    const Char* token_start = nullptr;
    const Char* token_end = nullptr;
    Token token = ParseToken(start, end, &token_start, &token_end);
    switch (token) {
      case NoInput:
        HandleError(Error::JSON_PARSER_NO_INPUT, token_start);
        return;
      case InvalidToken:
        HandleError(Error::JSON_PARSER_INVALID_TOKEN, token_start);
        return;
      case NullToken:
        handler_->HandleNull();
        break;
      case BoolTrue:
        handler_->HandleBool(true);
        break;
      case BoolFalse:
        handler_->HandleBool(false);
        break;
      case Number: {
        double value;
        if (!CharsToDouble(token_start, token_end - token_start, &value)) {
          HandleError(Error::JSON_PARSER_INVALID_NUMBER, token_start);
          return;
        }
        if (value >= std::numeric_limits<int32_t>::min() &&
            value <= std::numeric_limits<int32_t>::max() &&
            static_cast<int32_t>(value) == value)
          handler_->HandleInt32(static_cast<int32_t>(value));
        else
          handler_->HandleDouble(value);
        break;
      }
      case StringLiteral: {
        std::vector<uint16_t> value;
        bool ok = DecodeString(token_start + 1, token_end - 1, &value);
        if (!ok) {
          HandleError(Error::JSON_PARSER_INVALID_STRING, token_start);
          return;
        }
        handler_->HandleString16(span<uint16_t>(value.data(), value.size()));
        break;
      }
      case ArrayBegin: {
        handler_->HandleArrayBegin();
        start = token_end;
        token = ParseToken(start, end, &token_start, &token_end);
        while (token != ArrayEnd) {
          ParseValue(start, end, &token_end, depth + 1);
          if (error_)
            return;

          // After a list value, we expect a comma or the end of the list.
          start = token_end;
          token = ParseToken(start, end, &token_start, &token_end);
          if (token == ListSeparator) {
            start = token_end;
            token = ParseToken(start, end, &token_start, &token_end);
            if (token == ArrayEnd) {
              HandleError(Error::JSON_PARSER_UNEXPECTED_ARRAY_END, token_start);
              return;
            }
          } else if (token != ArrayEnd) {
            // Unexpected value after list value. Bail out.
            HandleError(Error::JSON_PARSER_COMMA_OR_ARRAY_END_EXPECTED,
                        token_start);
            return;
          }
        }
        handler_->HandleArrayEnd();
        break;
      }
      case ObjectBegin: {
        handler_->HandleMapBegin();
        start = token_end;
        token = ParseToken(start, end, &token_start, &token_end);
        while (token != ObjectEnd) {
          if (token != StringLiteral) {
            HandleError(Error::JSON_PARSER_STRING_LITERAL_EXPECTED,
                        token_start);
            return;
          }
          std::vector<uint16_t> key;
          if (!DecodeString(token_start + 1, token_end - 1, &key)) {
            HandleError(Error::JSON_PARSER_INVALID_STRING, token_start);
            return;
          }
          handler_->HandleString16(span<uint16_t>(key.data(), key.size()));
          start = token_end;

          token = ParseToken(start, end, &token_start, &token_end);
          if (token != ObjectPairSeparator) {
            HandleError(Error::JSON_PARSER_COLON_EXPECTED, token_start);
            return;
          }
          start = token_end;

          ParseValue(start, end, &token_end, depth + 1);
          if (error_)
            return;
          start = token_end;

          // After a key/value pair, we expect a comma or the end of the
          // object.
          token = ParseToken(start, end, &token_start, &token_end);
          if (token == ListSeparator) {
            start = token_end;
            token = ParseToken(start, end, &token_start, &token_end);
            if (token == ObjectEnd) {
              HandleError(Error::JSON_PARSER_UNEXPECTED_MAP_END, token_start);
              return;
            }
          } else if (token != ObjectEnd) {
            // Unexpected value after last object value. Bail out.
            HandleError(Error::JSON_PARSER_COMMA_OR_MAP_END_EXPECTED,
                        token_start);
            return;
          }
        }
        handler_->HandleMapEnd();
        break;
      }

      default:
        // We got a token that's not a value.
        HandleError(Error::JSON_PARSER_VALUE_EXPECTED, token_start);
        return;
    }

    SkipWhitespaceAndComments(token_end, end, value_token_end);
  }

  void HandleError(Error error, const Char* pos) {
    assert(error != Error::OK);
    if (!error_) {
      handler_->HandleError(
          Status{error, static_cast<size_t>(pos - start_pos_)});
      error_ = true;
    }
  }

  const Char* start_pos_ = nullptr;
  bool error_ = false;
  const Platform* platform_;
  StreamingParserHandler* handler_;
};
}  // namespace

void ParseJSON(const Platform& platform,
               span<uint8_t> chars,
               StreamingParserHandler* handler) {
  JsonParser<uint8_t> parser(&platform, handler);
  parser.Parse(chars.data(), chars.size());
}

void ParseJSON(const Platform& platform,
               span<uint16_t> chars,
               StreamingParserHandler* handler) {
  JsonParser<uint16_t> parser(&platform, handler);
  parser.Parse(chars.data(), chars.size());
}

// =============================================================================
// json::ConvertCBORToJSON, json::ConvertJSONToCBOR - for transcoding
// =============================================================================
template <typename C>
Status ConvertCBORToJSONTmpl(const Platform& platform,
                             span<uint8_t> cbor,
                             C* json) {
  Status status;
  std::unique_ptr<StreamingParserHandler> json_writer =
      NewJSONEncoder(&platform, json, &status);
  cbor::ParseCBOR(cbor, json_writer.get());
  return status;
}

Status ConvertCBORToJSON(const Platform& platform,
                         span<uint8_t> cbor,
                         std::vector<uint8_t>* json) {
  return ConvertCBORToJSONTmpl(platform, cbor, json);
}
Status ConvertCBORToJSON(const Platform& platform,
                         span<uint8_t> cbor,
                         std::string* json) {
  return ConvertCBORToJSONTmpl(platform, cbor, json);
}

template <typename T, typename C>
Status ConvertJSONToCBORTmpl(const Platform& platform, span<T> json, C* cbor) {
  Status status;
  std::unique_ptr<StreamingParserHandler> encoder =
      cbor::NewCBOREncoder(cbor, &status);
  ParseJSON(platform, json, encoder.get());
  return status;
}
Status ConvertJSONToCBOR(const Platform& platform,
                         span<uint8_t> json,
                         std::string* cbor) {
  return ConvertJSONToCBORTmpl(platform, json, cbor);
}
Status ConvertJSONToCBOR(const Platform& platform,
                         span<uint16_t> json,
                         std::string* cbor) {
  return ConvertJSONToCBORTmpl(platform, json, cbor);
}
Status ConvertJSONToCBOR(const Platform& platform,
                         span<uint8_t> json,
                         std::vector<uint8_t>* cbor) {
  return ConvertJSONToCBORTmpl(platform, json, cbor);
}
Status ConvertJSONToCBOR(const Platform& platform,
                         span<uint16_t> json,
                         std::vector<uint8_t>* cbor) {
  return ConvertJSONToCBORTmpl(platform, json, cbor);
}
}  // namespace json
}  // namespace v8_inspector_protocol_encoding
