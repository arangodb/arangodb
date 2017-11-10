// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_CODE_SERIALIZER_H_
#define V8_SNAPSHOT_CODE_SERIALIZER_H_

#include "src/parsing/preparse-data.h"
#include "src/snapshot/serializer.h"

namespace v8 {
namespace internal {

class CodeSerializer : public Serializer {
 public:
  static ScriptData* Serialize(Isolate* isolate,
                               Handle<SharedFunctionInfo> info,
                               Handle<String> source);

  ScriptData* Serialize(Handle<HeapObject> obj);

  MUST_USE_RESULT static MaybeHandle<SharedFunctionInfo> Deserialize(
      Isolate* isolate, ScriptData* cached_data, Handle<String> source);

  const List<uint32_t>* stub_keys() const { return &stub_keys_; }

  uint32_t source_hash() const { return source_hash_; }

 protected:
  explicit CodeSerializer(Isolate* isolate, uint32_t source_hash)
      : Serializer(isolate), source_hash_(source_hash) {}
  ~CodeSerializer() override { OutputStatistics("CodeSerializer"); }

  virtual void SerializeCodeObject(Code* code_object, HowToCode how_to_code,
                                   WhereToPoint where_to_point) {
    UNREACHABLE();
  }

  virtual bool ElideObject(Object* obj) { return false; }
  void SerializeGeneric(HeapObject* heap_object, HowToCode how_to_code,
                        WhereToPoint where_to_point);

 private:
  void SerializeObject(HeapObject* o, HowToCode how_to_code,
                       WhereToPoint where_to_point, int skip) override;

  void SerializeBuiltin(int builtin_index, HowToCode how_to_code,
                        WhereToPoint where_to_point);
  void SerializeCodeStub(Code* code_stub, HowToCode how_to_code,
                         WhereToPoint where_to_point);

  DisallowHeapAllocation no_gc_;
  uint32_t source_hash_;
  List<uint32_t> stub_keys_;
  DISALLOW_COPY_AND_ASSIGN(CodeSerializer);
};

class WasmCompiledModuleSerializer : public CodeSerializer {
 public:
  static std::unique_ptr<ScriptData> SerializeWasmModule(
      Isolate* isolate, Handle<FixedArray> compiled_module);
  static MaybeHandle<FixedArray> DeserializeWasmModule(
      Isolate* isolate, ScriptData* data, Vector<const byte> wire_bytes);

 protected:
  void SerializeCodeObject(Code* code_object, HowToCode how_to_code,
                           WhereToPoint where_to_point) override {
    Code::Kind kind = code_object->kind();
    if (kind == Code::WASM_FUNCTION || kind == Code::WASM_TO_JS_FUNCTION ||
        kind == Code::JS_TO_WASM_FUNCTION) {
      SerializeGeneric(code_object, how_to_code, where_to_point);
    } else {
      UNREACHABLE();
    }
  }

  bool ElideObject(Object* obj) override {
    return obj->IsWeakCell() || obj->IsForeign();
  };

 private:
  WasmCompiledModuleSerializer(Isolate* isolate, uint32_t source_hash)
      : CodeSerializer(isolate, source_hash) {}
  DISALLOW_COPY_AND_ASSIGN(WasmCompiledModuleSerializer);
};

// Wrapper around ScriptData to provide code-serializer-specific functionality.
class SerializedCodeData : public SerializedData {
 public:
  enum SanityCheckResult {
    CHECK_SUCCESS = 0,
    MAGIC_NUMBER_MISMATCH = 1,
    VERSION_MISMATCH = 2,
    SOURCE_MISMATCH = 3,
    CPU_FEATURES_MISMATCH = 4,
    FLAGS_MISMATCH = 5,
    CHECKSUM_MISMATCH = 6,
    INVALID_HEADER = 7
  };

  // The data header consists of uint32_t-sized entries:
  // [0] magic number and external reference count
  // [1] version hash
  // [2] source hash
  // [3] cpu features
  // [4] flag hash
  // [5] number of code stub keys
  // [6] number of reservation size entries
  // [7] payload length
  // [8] payload checksum part 1
  // [9] payload checksum part 2
  // ...  reservations
  // ...  code stub keys
  // ...  serialized payload
  static const int kVersionHashOffset = kMagicNumberOffset + kInt32Size;
  static const int kSourceHashOffset = kVersionHashOffset + kInt32Size;
  static const int kCpuFeaturesOffset = kSourceHashOffset + kInt32Size;
  static const int kFlagHashOffset = kCpuFeaturesOffset + kInt32Size;
  static const int kNumReservationsOffset = kFlagHashOffset + kInt32Size;
  static const int kNumCodeStubKeysOffset = kNumReservationsOffset + kInt32Size;
  static const int kPayloadLengthOffset = kNumCodeStubKeysOffset + kInt32Size;
  static const int kChecksum1Offset = kPayloadLengthOffset + kInt32Size;
  static const int kChecksum2Offset = kChecksum1Offset + kInt32Size;
  static const int kHeaderSize = kChecksum2Offset + kInt32Size;

  // Used when consuming.
  static const SerializedCodeData FromCachedData(
      Isolate* isolate, ScriptData* cached_data, uint32_t expected_source_hash,
      SanityCheckResult* rejection_result);

  // Used when producing.
  SerializedCodeData(const List<byte>* payload, const CodeSerializer* cs);

  // Return ScriptData object and relinquish ownership over it to the caller.
  ScriptData* GetScriptData();

  Vector<const Reservation> Reservations() const;
  Vector<const byte> Payload() const;

  Vector<const uint32_t> CodeStubKeys() const;

  static uint32_t SourceHash(Handle<String> source);

 private:
  explicit SerializedCodeData(ScriptData* data);
  SerializedCodeData(const byte* data, int size)
      : SerializedData(const_cast<byte*>(data), size) {}

  Vector<const byte> DataWithoutHeader() const {
    return Vector<const byte>(data_ + kHeaderSize, size_ - kHeaderSize);
  }

  SanityCheckResult SanityCheck(Isolate* isolate,
                                uint32_t expected_source_hash) const;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_CODE_SERIALIZER_H_
