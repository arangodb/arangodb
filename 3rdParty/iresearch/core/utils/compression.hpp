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

#include "memory.hpp"
#include "type_id.hpp"
#include "noncopyable.hpp"

#include <map>

// -----------------------------------------------------------------------------
// --SECTION--                                          compression registration
// -----------------------------------------------------------------------------

#define REGISTER_COMPRESSION__(compression_name, compressor_factory, decompressor_factory, line, source) \
  static iresearch::compression::compression_registrar compression_registrar ## _ ## line(::iresearch::type<compression_name>::get(), compressor_factory, decompressor_factory, source)
#define REGISTER_COMPRESSION_EXPANDER__(compression_name, compressor_factory, decompressor_factory, file, line) \
  REGISTER_COMPRESSION__(compression_name, compressor_factory, decompressor_factory, line, file ":" TOSTRING(line))
#define REGISTER_COMPRESSION(compression_name, compressor_factory, decompressor_factory) \
  REGISTER_COMPRESSION_EXPANDER__(compression_name, compressor_factory, decompressor_factory, __FILE__, __LINE__)

namespace iresearch {

struct data_output;
struct data_input;

namespace compression {

struct options {
  enum class Hint : byte_type {
    /// @brief use default compressor parameters
    DEFAULT = 0,

    /// @brief prefer speed over compression ratio
    SPEED,

    /// @brief prefer compression ratio over speed
    COMPRESSION
  };

  /// @brief
  Hint hint{ Hint::DEFAULT };

  options(Hint hint = Hint::DEFAULT)
    : hint(hint) {
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @class compressor
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API compressor {
  using ptr = std::shared_ptr<compressor>;

  virtual ~compressor() = default;

  /// @note caller is allowed to modify data pointed by 'in' up to 'size'
  virtual bytes_ref compress(byte_type* in, size_t size, bstring& buf) = 0;

  /// @brief flush arbitrary payload relevant to compression
  virtual void flush(data_output& /*out*/) { /*NOOP*/ }
};

////////////////////////////////////////////////////////////////////////////////
/// @class compressor
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API decompressor {
  using ptr = std::shared_ptr<decompressor>;

  virtual ~decompressor() = default;

  /// @note caller is allowed to modify data pointed by 'dst' up to 'dst_size'
  virtual bytes_ref decompress(const byte_type* src, size_t src_size,
                               byte_type* dst, size_t dst_size) = 0;

  virtual bool prepare(data_input& /*in*/) {
    // NOOP
    return true;
  }
};

typedef irs::compression::compressor::ptr(*compressor_factory_f)(const options&);
typedef irs::compression::decompressor::ptr(*decompressor_factory_f)();

// -----------------------------------------------------------------------------
// --SECTION--                                          compression registration
// -----------------------------------------------------------------------------

class IRESEARCH_API compression_registrar {
 public:
   compression_registrar(const type_info& type,
                         compressor_factory_f compressor_factory,
                         decompressor_factory_f decompressor_factory,
                         const char* source = nullptr);

  operator bool() const noexcept {
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
    const options& opts,
    bool load_library = true) noexcept;

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a compressor by name, or nullptr if not found
////////////////////////////////////////////////////////////////////////////////
inline compressor::ptr get_compressor(
    const type_info& type,
    const options& opts,
    bool load_library = true) noexcept {
  return get_compressor(type.name(), opts, load_library);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a decompressor by name, or nullptr if not found
////////////////////////////////////////////////////////////////////////////////
IRESEARCH_API decompressor::ptr get_decompressor(
    const string_ref& name,
    bool load_library = true) noexcept;

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a decompressor by name, or nullptr if not found
////////////////////////////////////////////////////////////////////////////////
inline decompressor::ptr get_decompressor(
    const type_info& type,
    bool load_library = true) noexcept {
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
struct IRESEARCH_API none {
  static constexpr string_ref type_name() noexcept {
    return "iresearch::compression::none";
  }

  static void init();

  static compression::compressor::ptr compressor(const options& /*opts*/) {
    return nullptr;
  }

  static compression::decompressor::ptr decompressor() {
    return nullptr;
  }
}; // raw

} // compression
}

#endif // IRESEARCH_COMPRESSION_H
