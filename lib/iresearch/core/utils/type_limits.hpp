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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "types.hpp"

// TODO(MBkkt) Make constant instead of function
// TODO(MBkkt) rename to irs::limits::some
namespace irs {
namespace address_limits {

constexpr uint64_t invalid() noexcept {
  return std::numeric_limits<uint64_t>::max();
}
constexpr bool valid(uint64_t addr) noexcept { return invalid() != addr; }

}  // namespace address_limits
namespace doc_limits {

constexpr doc_id_t eof() noexcept {
  return std::numeric_limits<doc_id_t>::max();
}
constexpr bool eof(doc_id_t id) noexcept { return eof() == id; }
constexpr doc_id_t invalid() noexcept { return 0; }
constexpr doc_id_t(min)() noexcept { return 1; }
constexpr bool valid(doc_id_t id) noexcept { return invalid() != id; }

}  // namespace doc_limits
namespace field_limits {

constexpr field_id invalid() noexcept {
  return std::numeric_limits<field_id>::max();
}
constexpr bool valid(field_id id) noexcept { return invalid() != id; }

}  // namespace field_limits
namespace index_gen_limits {

constexpr uint64_t invalid() noexcept { return 0; }
constexpr bool valid(uint64_t id) noexcept { return invalid() != id; }

}  // namespace index_gen_limits
namespace pos_limits {

constexpr uint32_t invalid() noexcept { return 0; }
constexpr bool valid(uint32_t pos) noexcept { return invalid() != pos; }
constexpr uint32_t eof() noexcept {
  return std::numeric_limits<uint32_t>::max();
}
constexpr bool eof(uint32_t pos) noexcept { return eof() == pos; }
constexpr uint32_t(min)() noexcept { return 1; }

}  // namespace pos_limits
namespace writer_limits {

inline constexpr uint64_t kMinTick = 0;
inline constexpr uint64_t kMaxTick = std::numeric_limits<uint64_t>::max();
inline constexpr size_t kInvalidOffset = std::numeric_limits<size_t>::max();

}  // namespace writer_limits
}  // namespace irs
