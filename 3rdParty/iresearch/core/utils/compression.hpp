////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_COMPRESSION_H
#define IRESEARCH_COMPRESSION_H

#include "type_id.hpp"
#include "memory.hpp"
#include "noncopyable.hpp"

#include <map>

// -----------------------------------------------------------------------------
// --SECTION--                                            compression definition
// -----------------------------------------------------------------------------

#define DECLARE_COMPRESSION_TYPE() DECLARE_TYPE_ID(iresearch::compression::type_id)
#define DEFINE_COMPRESSION_TYPE_NAMED(class_type, class_name, scope) \
  DEFINE_TYPE_ID(class_type, iresearch::compression::type_id) { \
    static iresearch::compression::type_id type(class_name, scope); \
  return type; \
}
#define DEFINE_COMPRESSION_TYPE(class_type, scope) DEFINE_COMPRESSION_TYPE_NAMED(class_type, #class_type, scope)

// -----------------------------------------------------------------------------
// --SECTION--                                          compression registration
// -----------------------------------------------------------------------------

#define REGISTER_COMPRESSION__(compression_name, compressor_factory, decompressor_factory, line, source) \
  static iresearch::compression::compression_registrar compression_registrar ## _ ## line(compression_name::type(), compressor_factory, decompressor_factory, source)
#define REGISTER_COMPRESSION_EXPANDER__(compression_name, compressor_factory, decompressor_factory, file, line) \
  REGISTER_COMPRESSION__(compression_name, compressor_factory, decompressor_factory, line, file ":" TOSTRING(line))
#define REGISTER_COMPRESSION(compression_name, compressor_factory, decompressor_factory) \
  REGISTER_COMPRESSION_EXPANDER__(compression_name, compressor_factory, decompressor_factory, __FILE__, __LINE__)

NS_ROOT
NS_BEGIN(compression)

////////////////////////////////////////////////////////////////////////////////
/// @class compressor
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API compressor {
  DECLARE_SHARED_PTR(compressor);

  virtual ~compressor() = default;

  /// @note caller is allowed to modify data pointed by 'in' up to 'size'
  virtual bytes_ref compress(byte_type* in, size_t size, bstring& buf) = 0;
};

////////////////////////////////////////////////////////////////////////////////
/// @class compressor
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API decompressor {
  DECLARE_SHARED_PTR(decompressor);

  virtual ~decompressor() = default;

  /// @note caller is allowed to modify data pointed by 'src' up to 'src_size'
  /// @note caller is allowed to modify data pointed by 'dst' up to 'dst_size'
  virtual bytes_ref decompress(byte_type* src, size_t src_size,
                               byte_type* dst, size_t dst_size) = 0;
};

////////////////////////////////////////////////////////////////////////////////
/// @class type_id
////////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API type_id : public irs::type_id, private util::noncopyable {
 public:
  enum class Scope { GLOBAL, LOCAL };

  type_id(const string_ref& name, Scope scope) NOEXCEPT
    : name_(name), scope_(scope) {
  }
  operator const type_id*() const NOEXCEPT { return this; }
  const string_ref& name() const NOEXCEPT { return name_; }
  Scope scope() const NOEXCEPT { return scope_; }

 private:
  string_ref name_;
  Scope scope_;
};

typedef irs::compression::compressor::ptr(*compressor_factory_f)();
typedef irs::compression::decompressor::ptr(*decompressor_factory_f)();

// -----------------------------------------------------------------------------
// --SECTION--                                          compression registration
// -----------------------------------------------------------------------------

class IRESEARCH_API compression_registrar {
 public:
   compression_registrar(const compression::type_id& type,
                         compressor_factory_f compressor_factory,
                         decompressor_factory_f decompressor_factory,
                         const char* source = nullptr);

  operator bool() const NOEXCEPT {
    return registered_;
  }

 private:
  bool registered_;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether an comopression with the specified name is registered
////////////////////////////////////////////////////////////////////////////////
IRESEARCH_API bool exists(const string_ref& name, bool load_library = true);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a compressor by name, or nullptr if not found
////////////////////////////////////////////////////////////////////////////////
IRESEARCH_API compressor::ptr get_compressor(
    const string_ref& name,
    bool load_library = true) NOEXCEPT;

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a compressor by name, or nullptr if not found
////////////////////////////////////////////////////////////////////////////////
inline compressor::ptr get_compressor(
    const type_id& type,
    bool load_library = true) NOEXCEPT {
  return get_compressor(type.name(), load_library);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a decompressor by name, or nullptr if not found
////////////////////////////////////////////////////////////////////////////////
IRESEARCH_API decompressor::ptr get_decompressor(
    const string_ref& name,
    bool load_library = true) NOEXCEPT;

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a decompressor by name, or nullptr if not found
////////////////////////////////////////////////////////////////////////////////
inline decompressor::ptr get_decompressor(
    const type_id& type,
    bool load_library = true) NOEXCEPT {
  return get_decompressor(type.name(), load_library);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief for static lib reference all known compressions in lib
///        for shared lib NOOP
///        no explicit call of fn is required, existence of fn is sufficient
////////////////////////////////////////////////////////////////////////////////
IRESEARCH_API void init();

////////////////////////////////////////////////////////////////////////////////
/// @brief load all compressions from plugins directory
////////////////////////////////////////////////////////////////////////////////
IRESEARCH_API void load_all(const std::string& path);

////////////////////////////////////////////////////////////////////////////////
/// @brief visit all loaded compressions, terminate early if visitor returns false
////////////////////////////////////////////////////////////////////////////////
IRESEARCH_API bool visit(const std::function<bool(const string_ref&)>& visitor);

////////////////////////////////////////////////////////////////////////////////
/// @class raw
/// @brief no compression
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API raw {
  DECLARE_COMPRESSION_TYPE();

  static void init();
  static compression::compressor::ptr compressor() { return nullptr; }
  static compression::decompressor::ptr decompressor() { return nullptr; }
}; // raw

NS_END // compression
NS_END

#endif // IRESEARCH_COMPRESSION_H
