////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include "formats.hpp"

// list of statically loaded formats via init()
#include "analysis/token_attributes.hpp"
#include "formats_10.hpp"
#include "utils/hash_utils.hpp"
#include "utils/register.hpp"
#include "utils/type_limits.hpp"

namespace {

constexpr std::string_view kFileNamePrefix{"libformat-"};

// first - format name
// second - module name, nullptr => matches format name
typedef std::pair<std::string_view, std::string_view> key_t;

struct equal_to {
  bool operator()(const key_t& lhs, const key_t& rhs) const noexcept {
    return lhs.first == rhs.first;
  }
};

struct hash {
  size_t operator()(const key_t& lhs) const noexcept {
    return irs::hash_utils::Hash(lhs.first);
  }
};

class format_register
  : public irs::tagged_generic_register<key_t, irs::format::ptr (*)(),
                                        std::string_view, format_register, hash,
                                        equal_to> {
 protected:
  std::string key_to_filename(const key_type& key) const final {
    const auto& module = irs::IsNull(key.second) ? key.first : key.second;

    std::string filename(kFileNamePrefix.size() + module.size(), 0);

    std::memcpy(filename.data(), kFileNamePrefix.data(),
                kFileNamePrefix.size());

    std::memcpy(filename.data() + kFileNamePrefix.size(), module.data(),
                module.size());

    return filename;
  }
};

}  // namespace

namespace irs {

bool formats::exists(std::string_view name, bool load_library /*= true*/) {
  const auto key = std::make_pair(name, std::string_view{});
  return nullptr != format_register::instance().get(key, load_library);
}

format::ptr formats::get(std::string_view name,
                         std::string_view module /*= {} */,
                         bool load_library /*= true*/) noexcept {
  try {
    const auto key = std::make_pair(name, module);
    auto* factory = format_register::instance().get(key, load_library);

    return factory ? factory() : nullptr;
  } catch (...) {
    IRS_LOG_ERROR("Caught exception while getting a format instance");
  }

  return nullptr;
}

void formats::init() { irs::version10::init(); }

void formats::load_all(std::string_view path) {
  load_libraries(path, kFileNamePrefix, "");
}

bool formats::visit(const std::function<bool(std::string_view)>& visitor) {
  auto visit_all = [&visitor](const format_register::key_type& key) {
    if (!visitor(key.first)) {
      return false;
    }
    return true;
  };

  return format_register::instance().visit(visit_all);
}

format_registrar::format_registrar(const type_info& type,
                                   std::string_view module,
                                   format::ptr (*factory)(),
                                   const char* source /*= nullptr*/) {
  const auto source_view =
    source ? std::string_view{source} : std::string_view{};

  auto entry = format_register::instance().set(
    std::make_pair(type.name(), module), factory,
    IsNull(source_view) ? nullptr : &source_view);

  registered_ = entry.second;

  if (!registered_ && factory != entry.first) {
    const auto key = std::make_pair(type.name(), std::string_view{});
    auto* registered_source = format_register::instance().tag(key);

    if (source && registered_source) {
      IRS_LOG_WARN(
        absl::StrCat("type name collision detected while registering format, "
                     "ignoring: type '",
                     type.name(), "' from ", source, ", previously from ",
                     *registered_source));
    } else if (source) {
      IRS_LOG_WARN(
        absl::StrCat("type name collision detected while registering format, "
                     "ignoring: type '",
                     type.name(), "' from ", source));
    } else if (registered_source) {
      IRS_LOG_WARN(
        absl::StrCat("type name collision detected while registering format, "
                     "ignoring: type '",
                     type.name(), "', previously from ", *registered_source));
    } else {
      IRS_LOG_WARN(
        absl::StrCat("type name collision detected while registering format, "
                     "ignoring: type '",
                     type.name(), "'"));
    }
  }
}

}  // namespace irs
