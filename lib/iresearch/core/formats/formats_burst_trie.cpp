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

#include "formats_burst_trie.hpp"

#include <variant>

#include "utils/assert.hpp"

// clang-format off

#include "utils/fstext/fst_utils.hpp"

#if defined(_MSC_VER)
#pragma warning(disable : 4291)
#elif defined(__GNUC__)
// NOOP
#endif

#include "fst/matcher.h"

#if defined(_MSC_VER)
#pragma warning(default : 4291)
#elif defined(__GNUC__)
// NOOP
#endif

#if defined(_MSC_VER)
#pragma warning(disable : 4244)
#pragma warning(disable : 4245)
#elif defined(__GNUC__)
// NOOP
#endif

#if defined(_MSC_VER)
#pragma warning(default : 4244)
#pragma warning(default : 4245)
#elif defined(__GNUC__)
// NOOP
#endif

#include "format_utils.hpp"
#include "analysis/token_attributes.hpp"
#include "formats/formats_10_attributes.hpp"
#include "index/index_meta.hpp"
#include "index/field_meta.hpp"
#include "index/file_names.hpp"
#include "index/iterators.hpp"
#include "index/index_meta.hpp"
#include "index/norm.hpp"
#include "store/memory_directory.hpp"
#include "search/scorer.hpp"
#include "store/store_utils.hpp"
#include "utils/automaton.hpp"
#include "utils/encryption.hpp"
#include "utils/hash_utils.hpp"
#include "utils/memory.hpp"
#include "utils/noncopyable.hpp"
#include "utils/directory_utils.hpp"
#include "utils/fstext/fst_string_weight.hpp"
#include "utils/fstext/fst_builder.hpp"
#include "utils/fstext/fst_decl.hpp"
#include "utils/fstext/fst_matcher.hpp"
#include "utils/fstext/fst_string_ref_weight.hpp"
#include "utils/fstext/fst_table_matcher.hpp"
#include "utils/fstext/immutable_fst.hpp"
#include "utils/timer_utils.hpp"
#include "utils/bit_utils.hpp"
#include "utils/bitset.hpp"
#include "utils/attribute_helper.hpp"
#include "utils/string.hpp"
#include "utils/log.hpp"

#include <absl/container/flat_hash_map.h>
#include <absl/strings/internal/resize_uninitialized.h>
#include <absl/strings/str_cat.h>

// clang-format on

namespace {

using namespace irs;

template<typename Char>
class volatile_ref : util::noncopyable {
 public:
  using ref_t = basic_string_view<Char>;
  using str_t = basic_string<Char>;

  volatile_ref() = default;

  volatile_ref(volatile_ref&& rhs) noexcept
    : str_(std::move(rhs.str_)), ref_(str_.empty() ? rhs.ref_ : ref_t(str_)) {
    rhs.ref_ = ref_;
  }

  volatile_ref& operator=(volatile_ref&& rhs) noexcept {
    if (this != &rhs) {
      str_ = std::move(rhs.str_);
      ref_ = (str_.empty() ? rhs.ref_ : ref_t(str_));
      rhs.ref_ = ref_;
    }
    return *this;
  }

  void clear() {
    str_.clear();
    ref_ = {};
  }

  template<bool Volatile>
  void assign(ref_t str) {
    if constexpr (Volatile) {
      str_.assign(str.data(), str.size());
      ref_ = str_;
    } else {
      ref_ = str;
      str_.clear();
    }
  }

  IRS_FORCE_INLINE void assign(const ref_t& str, bool Volatile) {
    (Volatile ? volatile_ref<Char>::assign<true>(str)
              : volatile_ref<Char>::assign<false>(str));
  }

  void assign(ref_t str, Char label) {
    str_.resize(str.size() + 1);
    std::memcpy(str_.data(), str.data(), str.size() * sizeof(Char));
    str_[str.size()] = label;
    ref_ = str_;
  }

  ref_t view() const noexcept { return ref_; }

  operator ref_t() const noexcept { return ref_; }

 private:
  str_t str_;
  ref_t ref_{};
};

using volatile_byte_ref = volatile_ref<byte_type>;

using feature_map_t = std::vector<irs::type_info::type_id>;

template<typename T>
struct Node {
  T* next = nullptr;
};

template<typename T>
struct IntrusiveList {
 public:
  IntrusiveList& operator=(const IntrusiveList&) = delete;
  IntrusiveList(const IntrusiveList&) = delete;

  IntrusiveList() noexcept = default;

  IntrusiveList(IntrusiveList&& other) noexcept
    : tail_{std::exchange(other.tail_, nullptr)} {}

  IntrusiveList& operator=(IntrusiveList&& other) noexcept {
    std::swap(tail_, other.tail_);
    return *this;
  }

  void Append(IntrusiveList&& rhs) noexcept {
    IRS_ASSERT(this != &rhs);
    if (rhs.tail_ == nullptr) {
      return;
    }
    if (tail_ == nullptr) {
      tail_ = rhs.tail_;
      rhs.tail_ = nullptr;
      return;
    }
    // h1->t1->h1 h2->t2->h2
    auto* head = tail_->next;
    // h1->t1->h2->t2->h2
    tail_->next = rhs.tail_->next;
    // h1->t1->h2->t2->h1
    rhs.tail_->next = head;
    // h1->**->h2->t2/t1->h1
    tail_ = rhs.tail_;
    // h1->**->h2->t1->h1
    rhs.tail_ = nullptr;
  }

  void PushFront(T& front) noexcept {
    IRS_ASSERT(front.next == &front);
    if (IRS_LIKELY(tail_ == nullptr)) {
      tail_ = &front;
      return;
    }
    front.next = tail_->next;
    tail_->next = &front;
  }

  template<typename Func>
  IRS_FORCE_INLINE void Visit(Func&& func) const {
    if (IRS_LIKELY(tail_ == nullptr)) {
      return;
    }
    auto* head = tail_->next;
    auto* it = head;
    do {
      func(*std::exchange(it, it->next));
    } while (it != head);
  }

  T* tail_ = nullptr;
};

// Block of terms
struct block_t : private util::noncopyable {
  struct prefixed_output final : data_output,
                                 Node<prefixed_output>,
                                 private util::noncopyable {
    explicit prefixed_output(volatile_byte_ref&& prefix) noexcept
      : Node<prefixed_output>{this}, prefix(std::move(prefix)) {}

    void write_byte(byte_type b) final { weight.PushBack(b); }

    void write_bytes(const byte_type* b, size_t len) final {
      weight.PushBack(b, b + len);
    }

    byte_weight weight;
    volatile_byte_ref prefix;
  };

  static constexpr uint16_t INVALID_LABEL{std::numeric_limits<uint16_t>::max()};

  using block_index_t = IntrusiveList<prefixed_output>;

  block_t(block_index_t&& other, uint64_t block_start, uint8_t meta,
          uint16_t label) noexcept
    : index{std::move(other)}, start{block_start}, label{label}, meta{meta} {}

  block_t(block_t&& rhs) noexcept
    : index(std::move(rhs.index)),
      start(rhs.start),
      label(rhs.label),
      meta(rhs.meta) {}

  ~block_t() {
    index.Visit([](prefixed_output& output) {  //
      output.~prefixed_output();
    });
  }

  block_index_t index;  // fst index data
  uint64_t start;       // file pointer
  uint16_t label;       // block lead label
  uint8_t meta;         // block metadata
};

template<typename T>
class MonotonicBuffer {
  static constexpr size_t kAlign =
    (alignof(T) * alignof(void*)) / std::gcd(alignof(T), alignof(void*));

  struct alignas(kAlign) Block {
    Block* prev = nullptr;
  };

  static_assert(std::is_trivially_destructible_v<Block>);

 public:
  MonotonicBuffer(IResourceManager& resource_manager,
                  size_t initial_size) noexcept
    : resource_manager_{resource_manager}, next_size_{initial_size} {
    IRS_ASSERT(initial_size > 1);
  }

  MonotonicBuffer(const MonotonicBuffer&) = delete;
  MonotonicBuffer& operator=(const MonotonicBuffer&) = delete;
  ~MonotonicBuffer() { Reset(); }

  template<typename... Args>
  T* Allocate(Args&&... args) {
    if (IRS_UNLIKELY(available_ == 0)) {
      AllocateMemory();
    }
    IRS_ASSERT(reinterpret_cast<uintptr_t>(current_) % alignof(T) == 0);
    auto* p = new (current_) T{std::forward<Args>(args)...};
    current_ += sizeof(T);
    --available_;
    return p;
  }

  // Release all memory, except current_ and keep next_size_
  void Clear() noexcept {
    if (IRS_UNLIKELY(head_ == nullptr)) {
      return;
    }

    Release(std::exchange(head_->prev, nullptr));
    // TODO(MBkkt) Don't be lazy, call Decrease eager
    // resource_manager_.Decrease(blocks_memory_ - size of head block);

    auto* initial_current = reinterpret_cast<byte_type*>(head_) + sizeof(Block);
    available_ += (current_ - initial_current) / sizeof(T);
    current_ = initial_current;
  }

  // Release all memory, but keep next_size_
  void Reset() noexcept {
    if (IRS_UNLIKELY(head_ == nullptr)) {
      return;
    }

    Release(std::exchange(head_, nullptr));
    resource_manager_.Decrease(std::exchange(blocks_memory_, 0));

    // otherwise we always increasing size!
    // TODO(MBkkt) we could compute current size, but it's
    next_size_ = (next_size_ * 2 + 2) / 3;

    available_ = 0;
    current_ = nullptr;
  }

 private:
  void Release(Block* it) noexcept {
    while (it != nullptr) {
      operator delete(std::exchange(it, it->prev), std::align_val_t{kAlign});
    }
  }

  void AllocateMemory() {
    const auto size = sizeof(Block) + next_size_ * sizeof(T);
    // TODO(MBkkt) use allocate_at_least but it's only C++23 :(
    resource_manager_.Increase(size);
    blocks_memory_ += size;
    auto* p =
      static_cast<byte_type*>(operator new(size, std::align_val_t{kAlign}));
    head_ = new (p) Block{head_};
    IRS_ASSERT(p == reinterpret_cast<byte_type*>(head_));
    current_ = p + sizeof(Block);
    available_ = next_size_;
    next_size_ = (next_size_ * 3) / 2;
    IRS_ASSERT(available_ < next_size_);
  }

  // TODO(MBkkt) Do we really want to measure this?
  IResourceManager& resource_manager_;
  size_t blocks_memory_ = 0;

  size_t next_size_;
  Block* head_ = nullptr;

  size_t available_ = 0;
  byte_type* current_ = nullptr;
};

using OutputBuffer = MonotonicBuffer<block_t::prefixed_output>;

enum EntryType : uint8_t { ET_TERM = 0, ET_BLOCK, ET_INVALID };

// Block or term
class entry : private util::noncopyable {
 public:
  entry(irs::bytes_view term, version10::term_meta&& attrs, bool volatile_term);

  entry(irs::bytes_view prefix, block_t::block_index_t&& index,
        uint64_t block_start, uint8_t meta, uint16_t label, bool volatile_term);
  entry(entry&& rhs) noexcept;
  entry& operator=(entry&& rhs) noexcept;
  ~entry() noexcept;

  version10::term_meta& term() noexcept {
    return *mem_.as<version10::term_meta>();
  }

  const block_t& block() const noexcept { return *mem_.as<block_t>(); }
  block_t& block() noexcept { return *mem_.as<block_t>(); }

  const volatile_byte_ref& data() const noexcept { return data_; }
  volatile_byte_ref& data() noexcept { return data_; }

  EntryType type() const noexcept { return type_; }

 private:
  void destroy() noexcept;
  void move_union(entry&& rhs) noexcept;

  volatile_byte_ref data_;  // block prefix or term
  memory::aligned_type<version10::term_meta, block_t> mem_;  // storage
  EntryType type_;                                           // entry type
};

entry::entry(irs::bytes_view term, version10::term_meta&& attrs,
             bool volatile_term)
  : type_(ET_TERM) {
  data_.assign(term, volatile_term);

  mem_.construct<version10::term_meta>(std::move(attrs));
}

entry::entry(irs::bytes_view prefix, block_t::block_index_t&& index,
             uint64_t block_start, uint8_t meta, uint16_t label,
             bool volatile_term)
  : type_(ET_BLOCK) {
  if (block_t::INVALID_LABEL != label) {
    data_.assign(prefix, static_cast<byte_type>(label & 0xFF));
  } else {
    data_.assign(prefix, volatile_term);
  }
  mem_.construct<block_t>(std::move(index), block_start, meta, label);
}

entry::entry(entry&& rhs) noexcept : data_{std::move(rhs.data_)} {
  move_union(std::move(rhs));
}

entry& entry::operator=(entry&& rhs) noexcept {
  if (this != &rhs) {
    data_ = std::move(rhs.data_);
    destroy();
    move_union(std::move(rhs));
  }

  return *this;
}

void entry::move_union(entry&& rhs) noexcept {
  type_ = rhs.type_;
  switch (type_) {
    case ET_TERM:
      mem_.construct<version10::term_meta>(std::move(rhs.term()));
      rhs.mem_.destroy<version10::term_meta>();
      break;
    case ET_BLOCK:
      mem_.construct<block_t>(std::move(rhs.block()));
      rhs.mem_.destroy<block_t>();
      break;
    default:
      break;
  }
  rhs.type_ = ET_INVALID;
}

void entry::destroy() noexcept {
  switch (type_) {
    case ET_TERM:
      mem_.destroy<version10::term_meta>();
      break;
    case ET_BLOCK:
      mem_.destroy<block_t>();
      break;
    default:
      break;
  }
  type_ = ET_INVALID;
}

entry::~entry() noexcept { destroy(); }

// Provides set of helper functions to work with block metadata
struct block_meta {
  // mask bit layout:
  // 0 - has terms
  // 1 - has sub blocks
  // 2 - is floor block

  // block has terms
  static bool terms(uint8_t mask) noexcept { return check_bit<ET_TERM>(mask); }

  // block has sub-blocks
  static bool blocks(uint8_t mask) noexcept {
    return check_bit<ET_BLOCK>(mask);
  }

  static void type(uint8_t& mask, EntryType type) noexcept {
    set_bit(mask, type);
  }

  // block is floor block
  static bool floor(uint8_t mask) noexcept {
    return check_bit<ET_INVALID>(mask);
  }
  static void floor(uint8_t& mask, bool b) noexcept {
    set_bit<ET_INVALID>(b, mask);
  }

  // resets block meta
  static void reset(uint8_t mask) noexcept {
    unset_bit<ET_TERM>(mask);
    unset_bit<ET_BLOCK>(mask);
  }
};

template<typename FeatureMap>
void write_segment_features_legacy(FeatureMap& feature_map, data_output& out,
                                   const flush_state& state) {
  const auto* features = state.features;
  const auto index_features = state.index_features;

  const size_t count = (features ? features->size() : 0) +
                       std::popcount(static_cast<uint32_t>(index_features));

  feature_map.clear();
  feature_map.reserve(count);

  out.write_vlong(count);
  if (IndexFeatures::NONE != (index_features & IndexFeatures::FREQ)) {
    write_string(out, irs::type<frequency>::name());
    feature_map.emplace(irs::type<frequency>::id(), feature_map.size());
  }
  if (IndexFeatures::NONE != (index_features & IndexFeatures::POS)) {
    write_string(out, irs::type<position>::name());
    feature_map.emplace(irs::type<position>::id(), feature_map.size());
  }
  if (IndexFeatures::NONE != (index_features & IndexFeatures::OFFS)) {
    write_string(out, irs::type<offset>::name());
    feature_map.emplace(irs::type<offset>::id(), feature_map.size());
  }
  if (IndexFeatures::NONE != (index_features & IndexFeatures::PAY)) {
    write_string(out, irs::type<payload>::name());
    feature_map.emplace(irs::type<payload>::id(), feature_map.size());
  }

  if (features) {
    for (const irs::type_info::type_id feature : *features) {
      write_string(out, feature().name());
      feature_map.emplace(feature, feature_map.size());
    }
  }
}

void read_segment_features_legacy(data_input& in, IndexFeatures& features,
                                  feature_map_t& feature_map) {
  feature_map.clear();
  feature_map.reserve(in.read_vlong());

  for (size_t count = feature_map.capacity(); count; --count) {
    const auto name = read_string<std::string>(in);  // read feature name
    const irs::type_info feature = attributes::get(name);

    if (!feature) {
      throw irs::index_error{absl::StrCat("Unknown feature name '", name, "'")};
    }

    feature_map.emplace_back(feature.id());

    if (feature.id() == type<frequency>::id()) {
      features |= IndexFeatures::FREQ;
    } else if (feature.id() == type<position>::id()) {
      features |= IndexFeatures::POS;
    } else if (feature.id() == type<offset>::id()) {
      features |= IndexFeatures::OFFS;
    } else if (feature.id() == type<payload>::id()) {
      features |= IndexFeatures::PAY;
    }
  }
}

template<typename FeatureMap>
void write_field_features_legacy(FeatureMap& feature_map, data_output& out,
                                 IndexFeatures index_features,
                                 const irs::feature_map_t& features) {
  auto write_feature = [&out, &feature_map](irs::type_info::type_id feature) {
    const auto it = feature_map.find(feature);
    IRS_ASSERT(it != feature_map.end());

    if (feature_map.end() == it) {
      // should not happen in reality
      throw irs::index_error{absl::StrCat(
        "Feature '", feature().name(), "' is not listed in segment features")};
    }

    out.write_vlong(it->second);
  };

  const size_t count =
    features.size() + std::popcount(static_cast<uint32_t>(index_features));

  out.write_vlong(count);

  if (IndexFeatures::NONE != (index_features & IndexFeatures::FREQ)) {
    write_feature(irs::type<frequency>::id());
  }
  if (IndexFeatures::NONE != (index_features & IndexFeatures::POS)) {
    write_feature(irs::type<position>::id());
  }
  if (IndexFeatures::NONE != (index_features & IndexFeatures::OFFS)) {
    write_feature(irs::type<offset>::id());
  }
  if (IndexFeatures::NONE != (index_features & IndexFeatures::PAY)) {
    write_feature(irs::type<payload>::id());
  }
  for (const auto& feature : features) {
    write_feature(feature.first);
  }

  const auto norm = features.find(irs::type<irs::Norm>::id());
  write_zvlong(out,
               norm == features.end() ? field_limits::invalid() : norm->second);
}

void read_field_features_legacy(data_input& in,
                                const feature_map_t& feature_map,
                                field_meta& field) {
  auto& index_features = field.index_features;
  auto& features = field.features;

  for (size_t count = in.read_vlong(); count; --count) {
    const size_t id = in.read_vlong();  // feature id

    if (id < feature_map.size()) {
      const auto feature = feature_map[id];

      if (feature == irs::type<frequency>::id()) {
        index_features |= IndexFeatures::FREQ;
      } else if (feature == irs::type<position>::id()) {
        index_features |= IndexFeatures::POS;
      } else if (feature == irs::type<offset>::id()) {
        index_features |= IndexFeatures::OFFS;
      } else if (feature == irs::type<payload>::id()) {
        index_features |= IndexFeatures::PAY;
      } else {
        features[feature] = field_limits::invalid();
      }
    } else {
      throw index_error{absl::StrCat("Unknown feature id '", id, "'")};
    }
  }

  const field_id norm = static_cast<field_id>(read_zvlong(in));

  if (field_limits::valid(norm)) {
    const auto it = features.find(irs::type<irs::Norm>::id());
    if (IRS_LIKELY(it != features.end())) {
      it->second = norm;
    } else {
      throw index_error{absl::StrCat(
        "'norm' feature is not registered with the field '", field.name, "'")};
    }
  }
}

template<typename FeatureMap>
void write_segment_features(FeatureMap& feature_map, data_output& out,
                            const flush_state& state) {
  auto* features = state.features;

  const size_t count = (features ? features->size() : 0);

  feature_map.clear();
  feature_map.reserve(count);

  out.write_int(static_cast<uint32_t>(state.index_features));
  out.write_vlong(count);

  if (features) {
    for (const irs::type_info::type_id feature : *features) {
      write_string(out, feature().name());
      feature_map.emplace(feature, feature_map.size());
    }
  }
}

template<typename FeatureMap>
void write_field_features(FeatureMap& feature_map, data_output& out,
                          IndexFeatures index_features,
                          const irs::feature_map_t& features) {
  auto write_feature = [&out, feature_map](irs::type_info::type_id feature) {
    const auto it = feature_map.find(feature);
    IRS_ASSERT(it != feature_map.end());

    if (feature_map.end() == it) {
      // should not happen in reality
      throw irs::index_error(absl::StrCat(
        "Feature '", feature().name(), "' is not listed in segment features"));
    }

    out.write_vlong(it->second);
  };

  const size_t count = features.size();

  out.write_int(static_cast<uint32_t>(index_features));
  out.write_vlong(count);
  for (const auto& feature : features) {
    write_feature(feature.first);
    out.write_vlong(feature.second + 1);
  }
}

IndexFeatures read_index_features(data_input& in) {
  const uint32_t index_features = in.read_int();

  if (index_features > static_cast<uint32_t>(IndexFeatures::ALL)) {
    throw index_error{
      absl::StrCat("Invalid segment index features ", index_features)};
  }

  return static_cast<IndexFeatures>(index_features);
}

void read_segment_features(data_input& in, IndexFeatures& features,
                           feature_map_t& feature_map) {
  features = read_index_features(in);

  feature_map.clear();
  feature_map.reserve(in.read_vlong());

  for (size_t count = feature_map.capacity(); count; --count) {
    const auto name = read_string<std::string>(in);  // read feature name
    const irs::type_info feature = attributes::get(name);

    if (!feature) {
      throw irs::index_error{absl::StrCat("Unknown feature name '", name, "'")};
    }

    feature_map.emplace_back(feature.id());
  }
}

void read_field_features(data_input& in, const feature_map_t& feature_map,
                         field_meta& field) {
  field.index_features = read_index_features(in);

  auto& features = field.features;
  for (size_t count = in.read_vlong(); count; --count) {
    const size_t id = in.read_vlong();  // feature id

    if (id < feature_map.size()) {
      const auto feature = feature_map[id];
      const field_id id = in.read_vlong() - 1;

      const auto [it, is_new] = features.emplace(feature, id);
      IRS_IGNORE(it);

      if (!is_new) {
        throw irs::index_error(
          absl::StrCat("duplicate feature '", feature().name(), "'"));
      }
    } else {
      throw irs::index_error(absl::StrCat("unknown feature id '", id, "'"));
    }
  }
}

inline void prepare_output(std::string& str, index_output::ptr& out,
                           const flush_state& state, std::string_view ext,
                           std::string_view format, const int32_t version) {
  IRS_ASSERT(!out);

  file_name(str, state.name, ext);
  out = state.dir->create(str);

  if (!out) {
    throw io_error{absl::StrCat("failed to create file, path: ", str)};
  }

  format_utils::write_header(*out, format, version);
}

inline int32_t prepare_input(std::string& str, index_input::ptr& in,
                             irs::IOAdvice advice, const ReaderState& state,
                             std::string_view ext, std::string_view format,
                             const int32_t min_ver, const int32_t max_ver,
                             int64_t* checksum = nullptr) {
  IRS_ASSERT(!in);

  file_name(str, state.meta->name, ext);
  in = state.dir->open(str, advice);

  if (!in) {
    throw io_error{absl::StrCat("Failed to open file, path: ", str)};
  }

  if (checksum) {
    *checksum = format_utils::checksum(*in);
  }

  return format_utils::check_header(*in, format, min_ver, max_ver);
}

struct cookie final : seek_cookie {
  explicit cookie(const version10::term_meta& meta) noexcept : meta(meta) {}

  attribute* get_mutable(irs::type_info::type_id type) final {
    if (IRS_LIKELY(type == irs::type<term_meta>::id())) {
      return &meta;
    }

    return nullptr;
  }

  bool IsEqual(const irs::seek_cookie& rhs) const noexcept final {
    // We intentionally don't check `rhs` cookie type.
    const auto& rhs_meta = DownCast<cookie>(rhs).meta;
    return meta.doc_start == rhs_meta.doc_start &&
           meta.pos_start == rhs_meta.pos_start;
  }

  size_t Hash() const noexcept final {
    return hash_combine(std::hash<size_t>{}(meta.doc_start),
                        std::hash<size_t>{}(meta.pos_start));
  }

  version10::term_meta meta;
};

const fst::FstWriteOptions& fst_write_options() {
  static const auto INSTANCE = []() {
    fst::FstWriteOptions options;
    options.write_osymbols = false;  // we don't need output symbols
    options.stream_write = true;

    return options;
  }();

  return INSTANCE;
}

const fst::FstReadOptions& fst_read_options() {
  static const auto INSTANCE = []() {
    fst::FstReadOptions options;
    options.read_osymbols = false;  // we don't need output symbols

    return options;
  }();

  return INSTANCE;
}

// mininum size of string weight we store in FST
[[maybe_unused]] constexpr const size_t MIN_WEIGHT_SIZE = 2;

using Blocks = ManagedVector<entry>;

void MergeBlocks(Blocks& blocks, OutputBuffer& buffer) {
  IRS_ASSERT(!blocks.empty());

  auto it = blocks.begin();

  auto& root = *it;
  auto& root_block = root.block();
  auto& root_index = root_block.index;

  auto& out = *buffer.Allocate(std::move(root.data()));
  root_index.PushFront(out);

  // First byte in block header must not be equal to fst::kStringInfinity
  // Consider the following:
  //   StringWeight0 -> { fst::kStringInfinity 0x11 ... }
  //   StringWeight1 -> { fst::KStringInfinity 0x22 ... }
  //   CommonPrefix = fst::Plus(StringWeight0, StringWeight1) -> {
  //   fst::kStringInfinity } Suffix = fst::Divide(StringWeight1, CommonPrefix)
  //   -> { fst::kStringBad }
  // But actually Suffix should be equal to { 0x22 ... }
  IRS_ASSERT(char(root_block.meta) != fst::kStringInfinity);

  // will store just several bytes here
  out.write_byte(static_cast<byte_type>(root_block.meta));  // block metadata
  out.write_vlong(root_block.start);  // start pointer of the block

  if (block_meta::floor(root_block.meta)) {
    IRS_ASSERT(blocks.size() - 1 < std::numeric_limits<uint32_t>::max());
    out.write_vint(static_cast<uint32_t>(blocks.size() - 1));
    for (++it; it != blocks.end(); ++it) {
      const auto* block = &it->block();
      IRS_ASSERT(block->label != block_t::INVALID_LABEL);
      IRS_ASSERT(block->start > root_block.start);

      const uint64_t start_delta = it->block().start - root_block.start;
      out.write_byte(static_cast<byte_type>(block->label & 0xFF));
      out.write_vlong(start_delta);
      out.write_byte(static_cast<byte_type>(block->meta));

      root_index.Append(std::move(it->block().index));
    }
  } else {
    for (++it; it != blocks.end(); ++it) {
      root_index.Append(std::move(it->block().index));
    }
  }

  // ensure weight we've written doesn't interfere
  // with semiring members and other constants
  IRS_ASSERT(out.weight != byte_weight::One() &&
             out.weight != byte_weight::Zero() &&
             out.weight != byte_weight::NoWeight() &&
             out.weight.Size() >= MIN_WEIGHT_SIZE &&
             byte_weight::One().Size() < MIN_WEIGHT_SIZE &&
             byte_weight::Zero().Size() < MIN_WEIGHT_SIZE &&
             byte_weight::NoWeight().Size() < MIN_WEIGHT_SIZE);
}

// Resetable FST buffer
class fst_buffer : public vector_byte_fst {
 public:
  // Fst builder stats
  struct fst_stats : irs::fst_stats {
    size_t total_weight_size{};

    void operator()(const byte_weight& w) noexcept {
      total_weight_size += w.Size();
    }

    [[maybe_unused]] bool operator==(const fst_stats& rhs) const noexcept {
      return num_states == rhs.num_states && num_arcs == rhs.num_arcs &&
             total_weight_size == rhs.total_weight_size;
    }
  };

  fst_buffer(IResourceManager& rm)
    : vector_byte_fst(ManagedTypedAllocator<byte_arc>(rm)) {}

  using fst_byte_builder = fst_builder<byte_type, vector_byte_fst, fst_stats>;

  fst_stats reset(const block_t::block_index_t& index) {
    builder.reset();

    index.Visit([&](block_t::prefixed_output& output) {
      builder.add(output.prefix, output.weight);
      // TODO(MBkkt) Call dtor here?
    });

    return builder.finish();
  }

 private:
  fst_byte_builder builder{*this};
};

class field_writer final : public irs::field_writer {
 public:
  static constexpr uint32_t DEFAULT_MIN_BLOCK_SIZE = 25;
  static constexpr uint32_t DEFAULT_MAX_BLOCK_SIZE = 48;

  static constexpr std::string_view FORMAT_TERMS = "block_tree_terms_dict";
  static constexpr std::string_view TERMS_EXT = "tm";
  static constexpr std::string_view FORMAT_TERMS_INDEX =
    "block_tree_terms_index";
  static constexpr std::string_view TERMS_INDEX_EXT = "ti";

  field_writer(irs::postings_writer::ptr&& pw, bool consolidation,
               IResourceManager& rm,
               burst_trie::Version version = burst_trie::Version::MAX,
               uint32_t min_block_size = DEFAULT_MIN_BLOCK_SIZE,
               uint32_t max_block_size = DEFAULT_MAX_BLOCK_SIZE);

  ~field_writer() final;

  void prepare(const irs::flush_state& state) final;

  void end() final;

  void write(const basic_term_reader& reader,
             const irs::feature_map_t& features) final;

 private:
  static constexpr size_t DEFAULT_SIZE = 8;

  void BeginField(IndexFeatures index_features,
                  const irs::feature_map_t& features);

  void EndField(std::string_view name, IndexFeatures index_features,
                const irs::feature_map_t& features, bytes_view min_term,
                bytes_view max_term, uint64_t total_doc_freq,
                uint64_t total_term_freq, uint64_t doc_count);

  // prefix - prefix length (in last_term)
  // begin - index of the first entry in the block
  // end - index of the last entry in the block
  // meta - block metadata
  // label - block lead label (if present)
  void WriteBlock(size_t prefix, size_t begin, size_t end, uint8_t meta,
                  uint16_t label);

  // prefix - prefix length ( in last_term
  // count - number of entries to write into block
  void WriteBlocks(size_t prefix, size_t count);

  void Push(bytes_view term);

  absl::flat_hash_map<irs::type_info::type_id, size_t> feature_map_;
  OutputBuffer output_buffer_;
  Blocks blocks_;
  memory_output suffix_;  // term suffix column
  memory_output stats_;   // term stats column
  encryption::stream::ptr terms_out_cipher_;
  index_output::ptr terms_out_;  // output stream for terms
  encryption::stream::ptr index_out_cipher_;
  index_output::ptr index_out_;  // output stream for indexes
  postings_writer::ptr pw_;      // postings writer
  ManagedVector<entry> stack_;
  fst_buffer* fst_buf_;  // pimpl buffer used for building FST for fields
  volatile_byte_ref last_term_;  // last pushed term
  std::vector<size_t> prefixes_;
  size_t fields_count_{};
  const burst_trie::Version version_;
  const uint32_t min_block_size_;
  const uint32_t max_block_size_;
  const bool consolidation_;
};

void field_writer::WriteBlock(size_t prefix, size_t begin, size_t end,
                              uint8_t meta, uint16_t label) {
  IRS_ASSERT(end > begin);

  // begin of the block
  const uint64_t block_start = terms_out_->file_pointer();

  // write block header
  terms_out_->write_vint(
    shift_pack_32(static_cast<uint32_t>(end - begin), end == stack_.size()));

  // write block entries
  const bool leaf = !block_meta::blocks(meta);

  block_t::block_index_t index;

  pw_->begin_block();

  for (; begin < end; ++begin) {
    auto& e = stack_[begin];
    const irs::bytes_view data = e.data();
    const EntryType type = e.type();
    IRS_ASSERT(data.starts_with({last_term_.view().data(), prefix}));

    // only terms under 32k are allowed
    IRS_ASSERT(data.size() - prefix <= UINT32_C(0x7FFFFFFF));
    const uint32_t suf_size = static_cast<uint32_t>(data.size() - prefix);

    suffix_.stream.write_vint(
      leaf ? suf_size : ((suf_size << 1) | static_cast<uint32_t>(type)));
    suffix_.stream.write_bytes(data.data() + prefix, suf_size);

    if (ET_TERM == type) {
      pw_->encode(stats_.stream, e.term());
    } else {
      IRS_ASSERT(ET_BLOCK == type);

      // current block start pointer should be greater
      IRS_ASSERT(block_start > e.block().start);
      suffix_.stream.write_vlong(block_start - e.block().start);
      index.Append(std::move(e.block().index));
    }
  }

  const size_t block_size = suffix_.stream.file_pointer();

  suffix_.stream.flush();
  stats_.stream.flush();

  terms_out_->write_vlong(
    shift_pack_64(static_cast<uint64_t>(block_size), leaf));

  auto copy = [this](const byte_type* b, size_t len) {
    terms_out_->write_bytes(b, len);
    return true;
  };

  if (terms_out_cipher_) {
    auto offset = block_start;

    auto encrypt_and_copy = [this, &offset](byte_type* b, size_t len) {
      IRS_ASSERT(terms_out_cipher_);

      if (!terms_out_cipher_->encrypt(offset, b, len)) {
        return false;
      }

      terms_out_->write_bytes(b, len);
      offset += len;
      return true;
    };

    if (!suffix_.file.visit(encrypt_and_copy)) {
      throw io_error("failed to encrypt term dictionary");
    }
  } else {
    suffix_.file.visit(copy);
  }

  terms_out_->write_vlong(static_cast<uint64_t>(stats_.stream.file_pointer()));
  stats_.file.visit(copy);

  suffix_.stream.reset();
  stats_.stream.reset();

  // add new block to the list of created blocks
  blocks_.emplace_back(bytes_view{last_term_.view().data(), prefix},
                       std::move(index), block_start, meta, label,
                       consolidation_);
}

void field_writer::WriteBlocks(size_t prefix, size_t count) {
  // only root node able to write whole stack
  IRS_ASSERT(prefix || count == stack_.size());
  IRS_ASSERT(blocks_.empty());

  // block metadata
  uint8_t meta{};

  const size_t end = stack_.size();
  const size_t begin = end - count;
  size_t block_start = begin;  // begin of current block to write

  size_t min_suffix = std::numeric_limits<size_t>::max();
  size_t max_suffix = 0;

  uint16_t last_label{block_t::INVALID_LABEL};  // last lead suffix label
  uint16_t next_label{
    block_t::INVALID_LABEL};  // next lead suffix label in current block
  for (size_t i = begin; i < end; ++i) {
    const entry& e = stack_[i];
    const irs::bytes_view data = e.data();

    const size_t suffix = data.size() - prefix;
    min_suffix = std::min(suffix, min_suffix);
    max_suffix = std::max(suffix, max_suffix);

    const uint16_t label =
      data.size() == prefix ? block_t::INVALID_LABEL : data[prefix];

    if (last_label != label) {
      const size_t block_size = i - block_start;

      if (block_size >= min_block_size_ &&
          end - block_start > max_block_size_) {
        block_meta::floor(meta, block_size < count);
        WriteBlock(prefix, block_start, i, meta, next_label);
        next_label = label;
        block_meta::reset(meta);
        block_start = i;
        min_suffix = std::numeric_limits<size_t>::max();
        max_suffix = 0;
      }

      last_label = label;
    }

    block_meta::type(meta, e.type());
  }

  // write remaining block
  if (block_start < end) {
    block_meta::floor(meta, end - block_start < count);
    WriteBlock(prefix, block_start, end, meta, next_label);
  }

  // merge blocks into 1st block
  ::MergeBlocks(blocks_, output_buffer_);

  // remove processed entries from the
  // top of the stack
  stack_.erase(stack_.begin() + begin, stack_.end());

  // move root block from temporary storage
  // to the top of the stack
  if (!blocks_.empty()) {
    stack_.emplace_back(std::move(blocks_.front()));
    blocks_.clear();
  }
}

void field_writer::Push(bytes_view term) {
  const irs::bytes_view last = last_term_;
  const size_t limit = std::min(last.size(), term.size());

  // find common prefix
  size_t pos = 0;
  while (pos < limit && term[pos] == last[pos]) {
    ++pos;
  }

  for (size_t i = last.empty() ? 0 : last.size() - 1; i > pos;) {
    --i;  // should use it here as we use size_t
    const size_t top = stack_.size() - prefixes_[i];
    if (top > min_block_size_) {
      WriteBlocks(i + 1, top);
      prefixes_[i] -= (top - 1);
    }
  }

  prefixes_.resize(term.size());
  std::fill(prefixes_.begin() + pos, prefixes_.end(), stack_.size());
  last_term_.assign(term, consolidation_);
}

field_writer::field_writer(
  irs::postings_writer::ptr&& pw, bool consolidation, IResourceManager& rm,
  burst_trie::Version version /* = Format::MAX */,
  uint32_t min_block_size /* = DEFAULT_MIN_BLOCK_SIZE */,
  uint32_t max_block_size /* = DEFAULT_MAX_BLOCK_SIZE */)
  : output_buffer_(rm, 32),
    blocks_(ManagedTypedAllocator<entry>(rm)),
    suffix_(rm),
    stats_(rm),
    pw_(std::move(pw)),
    stack_(ManagedTypedAllocator<entry>(rm)),
    fst_buf_(new fst_buffer(rm)),
    prefixes_(DEFAULT_SIZE, 0),
    version_(version),
    min_block_size_(min_block_size),
    max_block_size_(max_block_size),
    consolidation_(consolidation) {
  IRS_ASSERT(this->pw_);
  IRS_ASSERT(min_block_size > 1);
  IRS_ASSERT(min_block_size <= max_block_size);
  IRS_ASSERT(2 * (min_block_size - 1) <= max_block_size);
  IRS_ASSERT(version_ >= burst_trie::Version::MIN &&
             version_ <= burst_trie::Version::MAX);
}

field_writer::~field_writer() { delete fst_buf_; }

void field_writer::prepare(const flush_state& state) {
  IRS_ASSERT(state.dir);

  // reset writer state
  last_term_.clear();
  prefixes_.assign(DEFAULT_SIZE, 0);
  stack_.clear();
  stats_.reset();
  suffix_.reset();
  fields_count_ = 0;

  std::string filename;
  bstring enc_header;
  auto* enc = state.dir->attributes().encryption();

  // prepare term dictionary
  prepare_output(filename, terms_out_, state, TERMS_EXT, FORMAT_TERMS,
                 static_cast<int32_t>(version_));

  if (version_ > burst_trie::Version::MIN) {
    // encrypt term dictionary
    const auto encrypt =
      irs::encrypt(filename, *terms_out_, enc, enc_header, terms_out_cipher_);
    IRS_ASSERT(!encrypt ||
               (terms_out_cipher_ && terms_out_cipher_->block_size()));
    IRS_IGNORE(encrypt);
  }

  // prepare term index
  prepare_output(filename, index_out_, state, TERMS_INDEX_EXT,
                 FORMAT_TERMS_INDEX, static_cast<int32_t>(version_));

  if (version_ > burst_trie::Version::MIN) {
    // encrypt term index
    if (irs::encrypt(filename, *index_out_, enc, enc_header,
                     index_out_cipher_)) {
      IRS_ASSERT(index_out_cipher_ && index_out_cipher_->block_size());

      const auto blocks_in_buffer = math::div_ceil64(
        DEFAULT_ENCRYPTION_BUFFER_SIZE, index_out_cipher_->block_size());

      index_out_ = index_output::make<encrypted_output>(
        std::move(index_out_), *index_out_cipher_, blocks_in_buffer);
    }
  }

  if (IRS_LIKELY(version_ >= burst_trie::Version::IMMUTABLE_FST)) {
    write_segment_features(feature_map_, *index_out_, state);
  } else {
    write_segment_features_legacy(feature_map_, *index_out_, state);
  }

  // prepare postings writer
  pw_->prepare(*terms_out_, state);

  suffix_.reset();
  stats_.reset();
}

void field_writer::write(const basic_term_reader& reader,
                         const irs::feature_map_t& features) {
  REGISTER_TIMER_DETAILED();
  const auto& reader_meta = reader.meta();
  const auto index_features = reader_meta.index_features;
  BeginField(index_features, features);

  uint64_t term_count = 0;
  uint64_t sum_dfreq = 0;
  uint64_t sum_tfreq = 0;

  const bool freq_exists =
    IndexFeatures::NONE != (index_features & IndexFeatures::FREQ);

  auto terms = reader.iterator();
  IRS_ASSERT(terms != nullptr);
  while (terms->next()) {
    auto postings = terms->postings(index_features);
    version10::term_meta meta;
    pw_->write(*postings, meta);

    if (freq_exists) {
      sum_tfreq += meta.freq;
    }

    if (meta.docs_count != 0) {
      sum_dfreq += meta.docs_count;

      const bytes_view term = terms->value();
      Push(term);

      // push term to the top of the stack
      stack_.emplace_back(term, std::move(meta), consolidation_);

      ++term_count;
    }
  }

  EndField(reader_meta.name, index_features, features, reader.min(),
           reader.max(), sum_dfreq, sum_tfreq, term_count);
}

void field_writer::BeginField(IndexFeatures index_features,
                              const irs::feature_map_t& features) {
  IRS_ASSERT(terms_out_);
  IRS_ASSERT(index_out_);

  // At the beginning of the field there should be no pending entries at all
  IRS_ASSERT(stack_.empty());

  pw_->begin_field(index_features, features);
}

void field_writer::EndField(std::string_view name, IndexFeatures index_features,
                            const irs::feature_map_t& features,
                            bytes_view min_term, bytes_view max_term,
                            uint64_t total_doc_freq, uint64_t total_term_freq,
                            uint64_t term_count) {
  IRS_ASSERT(terms_out_);
  IRS_ASSERT(index_out_);

  if (!term_count) {
    // nothing to write
    return;
  }

  const auto [wand_mask, doc_count] = pw_->end_field();

  // cause creation of all final blocks
  Push(kEmptyStringView<byte_type>);

  // write root block with empty prefix
  WriteBlocks(0, stack_.size());
  IRS_ASSERT(1 == stack_.size());

  // write field meta
  write_string(*index_out_, name);
  if (IRS_LIKELY(version_ >= burst_trie::Version::IMMUTABLE_FST)) {
    write_field_features(feature_map_, *index_out_, index_features, features);
  } else {
    write_field_features_legacy(feature_map_, *index_out_, index_features,
                                features);
  }
  index_out_->write_vlong(term_count);
  index_out_->write_vlong(doc_count);
  index_out_->write_vlong(total_doc_freq);
  write_string<irs::bytes_view>(*index_out_, min_term);
  write_string<irs::bytes_view>(*index_out_, max_term);
  if (IndexFeatures::NONE != (index_features & IndexFeatures::FREQ)) {
    index_out_->write_vlong(total_term_freq);
  }
  if (IRS_LIKELY(version_ >= burst_trie::Version::WAND)) {
    index_out_->write_long(wand_mask);
  }

  // build fst
  const entry& root = *stack_.begin();
  IRS_ASSERT(fst_buf_);
  [[maybe_unused]] const auto fst_stats = fst_buf_->reset(root.block().index);
  stack_.clear();
  output_buffer_.Clear();

  const vector_byte_fst& fst = *fst_buf_;

#ifdef IRESEARCH_DEBUG
  // ensure evaluated stats are correct
  struct fst_buffer::fst_stats stats {};
  for (fst::StateIterator<vector_byte_fst> states(fst); !states.Done();
       states.Next()) {
    const auto stateid = states.Value();
    ++stats.num_states;
    stats.num_arcs += fst.NumArcs(stateid);
    stats(fst.Final(stateid));
    for (fst::ArcIterator<vector_byte_fst> arcs(fst, stateid); !arcs.Done();
         arcs.Next()) {
      stats(arcs.Value().weight);
    }
  }
  IRS_ASSERT(stats == fst_stats);
#endif

  // write FST
  bool ok;
  if (version_ > burst_trie::Version::ENCRYPTION_MIN) {
    ok = immutable_byte_fst::Write(fst, *index_out_, fst_stats);
  } else {
    // wrap stream to be OpenFST compliant
    output_buf isb(index_out_.get());
    std::ostream os(&isb);
    ok = fst.Write(os, fst_write_options());
  }

  if (IRS_UNLIKELY(!ok)) {
    throw irs::index_error{
      absl::StrCat("Failed to write term index for field: ", name)};
  }

  ++fields_count_;
}

void field_writer::end() {
  output_buffer_.Reset();

  IRS_ASSERT(terms_out_);
  IRS_ASSERT(index_out_);

  // finish postings
  pw_->end();

  format_utils::write_footer(*terms_out_);
  terms_out_.reset();  // ensure stream is closed

  if (index_out_cipher_) {
    auto& out = static_cast<encrypted_output&>(*index_out_);
    out.flush();
    index_out_ = out.release();
  }

  index_out_->write_long(fields_count_);
  format_utils::write_footer(*index_out_);
  index_out_.reset();  // ensure stream is closed
}

class term_reader_base : public irs::term_reader, private util::noncopyable {
 public:
  term_reader_base() = default;
  term_reader_base(term_reader_base&& rhs) = default;
  term_reader_base& operator=(term_reader_base&&) = delete;

  const field_meta& meta() const noexcept final { return field_; }
  size_t size() const noexcept final { return terms_count_; }
  uint64_t docs_count() const noexcept final { return doc_count_; }
  bytes_view min() const noexcept final { return min_term_; }
  bytes_view max() const noexcept final { return max_term_; }
  attribute* get_mutable(irs::type_info::type_id type) noexcept final;
  bool has_scorer(uint8_t index) const noexcept final;

  virtual void prepare(burst_trie::Version version, index_input& in,
                       const feature_map_t& features);

  uint8_t WandCount() const noexcept { return wand_count_; }

 protected:
  uint8_t WandIndex(uint8_t i) const noexcept;

 private:
  field_meta field_;
  bstring min_term_;
  bstring max_term_;
  uint64_t terms_count_;
  uint64_t doc_count_;
  uint64_t doc_freq_;
  uint64_t wand_mask_{};
  uint32_t wand_count_{};
  frequency freq_;  // total term freq
};

uint8_t term_reader_base::WandIndex(uint8_t i) const noexcept {
  if (i >= kMaxScorers) {
    return WandContext::kDisable;
  }

  const uint64_t mask{uint64_t{1} << i};

  if (0 == (wand_mask_ & mask)) {
    return WandContext::kDisable;
  }

  return static_cast<uint8_t>(std::popcount(wand_mask_ & (mask - 1)));
}

bool term_reader_base::has_scorer(uint8_t index) const noexcept {
  return WandIndex(index) != WandContext::kDisable;
}

void term_reader_base::prepare(burst_trie::Version version, index_input& in,
                               const feature_map_t& feature_map) {
  // read field metadata
  field_.name = read_string<std::string>(in);

  if (IRS_LIKELY(version >= burst_trie::Version::IMMUTABLE_FST)) {
    read_field_features(in, feature_map, field_);
  } else {
    read_field_features_legacy(in, feature_map, field_);
  }

  terms_count_ = in.read_vlong();
  doc_count_ = in.read_vlong();
  doc_freq_ = in.read_vlong();
  min_term_ = read_string<bstring>(in);
  max_term_ = read_string<bstring>(in);

  if (IndexFeatures::NONE != (field_.index_features & IndexFeatures::FREQ)) {
    // TODO(MBkkt) for what reason we store uint64_t if we read to uint32_t
    freq_.value = static_cast<uint32_t>(in.read_vlong());
  }

  if (IRS_LIKELY(version >= burst_trie::Version::WAND)) {
    wand_mask_ = in.read_long();
    wand_count_ = static_cast<uint8_t>(std::popcount(wand_mask_));
  }
}

attribute* term_reader_base::get_mutable(
  irs::type_info::type_id type) noexcept {
  if (IndexFeatures::NONE != (field_.index_features & IndexFeatures::FREQ) &&
      irs::type<irs::frequency>::id() == type) {
    return &freq_;
  }

  return nullptr;
}

class block_iterator : util::noncopyable {
 public:
  static constexpr uint32_t UNDEFINED_COUNT{
    std::numeric_limits<uint32_t>::max()};
  static constexpr uint64_t UNDEFINED_ADDRESS{
    std::numeric_limits<uint64_t>::max()};

  block_iterator(byte_weight&& header, size_t prefix) noexcept;

  block_iterator(bytes_view header, size_t prefix)
    : block_iterator{byte_weight{header}, prefix} {}

  block_iterator(uint64_t start, size_t prefix) noexcept
    : start_{start},
      cur_start_{start},
      cur_end_{start},
      prefix_{static_cast<uint32_t>(prefix)},
      sub_count_{UNDEFINED_COUNT} {
    IRS_ASSERT(prefix <= std::numeric_limits<uint32_t>::max());
  }

  void load(index_input& in, encryption::stream* cipher);

  template<bool ReadHeader>
  bool next_sub_block() noexcept {
    if (!sub_count_) {
      return false;
    }

    cur_start_ = cur_end_;
    if (sub_count_ != UNDEFINED_COUNT) {
      --sub_count_;
      if constexpr (ReadHeader) {
        vskip<uint64_t>(header_.begin);
        cur_meta_ = *header_.begin++;
        if (sub_count_) {
          next_label_ = *header_.begin++;
        }
      }
    }
    dirty_ = true;
    header_.assert_block_boundaries();
    return true;
  }

  template<typename Reader>
  void next(Reader&& reader) {
    IRS_ASSERT(!dirty_ && cur_ent_ < ent_count_);
    if (leaf_) {
      read_entry_leaf(std::forward<Reader>(reader));
    } else {
      read_entry_nonleaf(std::forward<Reader>(reader));
    }
    ++cur_ent_;
  }

  void reset();

  const version10::term_meta& state() const noexcept { return state_; }
  bool dirty() const noexcept { return dirty_; }
  uint8_t meta() const noexcept { return cur_meta_; }
  size_t prefix() const noexcept { return prefix_; }
  EntryType type() const noexcept { return cur_type_; }
  uint64_t block_start() const noexcept { return cur_block_start_; }
  uint16_t next_label() const noexcept { return next_label_; }
  uint32_t sub_count() const noexcept { return sub_count_; }
  uint64_t start() const noexcept { return start_; }
  bool done() const noexcept { return cur_ent_ == ent_count_; }
  bool no_terms() const noexcept {
    // FIXME(gnusi): add term mark to block entry?
    //
    // Block was loaded using address and doesn't have metadata,
    // assume such blocks have terms
    return sub_count_ != UNDEFINED_COUNT && !block_meta::terms(meta());
  }

  template<typename Reader>
  SeekResult scan_to_term(bytes_view term, Reader&& reader) {
    IRS_ASSERT(term.size() >= prefix_);
    IRS_ASSERT(!dirty_);

    return leaf_ ? scan_to_term_leaf(term, std::forward<Reader>(reader))
                 : scan_to_term_nonleaf(term, std::forward<Reader>(reader));
  }

  template<typename Reader>
  SeekResult scan(Reader&& reader) {
    IRS_ASSERT(!dirty_);

    return leaf_ ? scan_leaf(std::forward<Reader>(reader))
                 : scan_nonleaf(std::forward<Reader>(reader));
  }

  // scan to floor block
  void scan_to_sub_block(byte_type label);

  // scan to entry with the following start address
  void scan_to_block(uint64_t ptr);

  // read attributes
  void load_data(const field_meta& meta, version10::term_meta& state,
                 irs::postings_reader& pr);

 private:
  struct data_block : util::noncopyable {
    using block_type = bstring;

    data_block() = default;
    data_block(block_type&& block) noexcept
      : block{std::move(block)}, begin{this->block.c_str()} {
#ifdef IRESEARCH_DEBUG
      end = begin + this->block.size();
      assert_block_boundaries();
#endif
    }
    data_block(data_block&& rhs) noexcept { *this = std::move(rhs); }
    data_block& operator=(data_block&& rhs) noexcept {
      if (this != &rhs) {
        if (rhs.block.empty()) {
          begin = rhs.begin;
#ifdef IRESEARCH_DEBUG
          end = rhs.end;
#endif
        } else {
          const size_t offset = std::distance(rhs.block.c_str(), rhs.begin);
          block = std::move(rhs.block);
          begin = block.c_str() + offset;
#ifdef IRESEARCH_DEBUG
          end = block.c_str() + block.size();
#endif
        }
      }
      assert_block_boundaries();
      return *this;
    }

    [[maybe_unused]] void assert_block_boundaries() {
#ifdef IRESEARCH_DEBUG
      IRS_ASSERT(begin <= end);
      if (!block.empty()) {
        IRS_ASSERT(end <= (block.c_str() + block.size()));
        IRS_ASSERT(block.c_str() <= begin);
      }
#endif
    }

    block_type block;
    const byte_type* begin{block.c_str()};
#ifdef IRESEARCH_DEBUG
    const byte_type* end{begin};
#endif
  };

  template<typename Reader>
  void read_entry_nonleaf(Reader&& reader);

  template<typename Reader>
  void read_entry_leaf(Reader&& reader) {
    IRS_ASSERT(leaf_ && cur_ent_ < ent_count_);
    cur_type_ = ET_TERM;  // always term
    ++term_count_;
    suffix_length_ = vread<uint32_t>(suffix_.begin);
    reader(suffix_.begin, suffix_length_);
    suffix_begin_ = suffix_.begin;
    suffix_.begin += suffix_length_;
    suffix_.assert_block_boundaries();
  }

  template<typename Reader>
  SeekResult scan_to_term_nonleaf(bytes_view term, Reader&& reader);
  template<typename Reader>
  SeekResult scan_to_term_leaf(bytes_view term, Reader&& reader);

  template<typename Reader>
  SeekResult scan_nonleaf(Reader&& reader);
  template<typename Reader>
  SeekResult scan_leaf(Reader&& reader);

  data_block header_;  // suffix block header
  data_block suffix_;  // suffix data block
  data_block stats_;   // stats data block
  version10::term_meta state_;
  size_t suffix_length_{};  // last matched suffix length
  const byte_type* suffix_begin_{};
  uint64_t start_;      // initial block start pointer
  uint64_t cur_start_;  // current block start pointer
  uint64_t cur_end_;    // block end pointer
  // start pointer of the current sub-block entry
  uint64_t cur_block_start_{UNDEFINED_ADDRESS};
  uint32_t prefix_;           // block prefix length, 32k at most
  uint32_t cur_ent_{};        // current entry in a block
  uint32_t ent_count_{};      // number of entries in a current block
  uint32_t term_count_{};     // number terms in a block we have seen
  uint32_t cur_stats_ent_{};  // current position of loaded stats
  uint32_t sub_count_;        // number of sub-blocks
  // next label (of the next sub-block)
  uint16_t next_label_{block_t::INVALID_LABEL};
  EntryType cur_type_{ET_INVALID};  // term or block
  byte_type meta_{};                // initial block metadata
  byte_type cur_meta_{};            // current block metadata
  bool dirty_{true};                // current block is dirty
  bool leaf_{false};                // current block is leaf block
};

block_iterator::block_iterator(byte_weight&& header, size_t prefix) noexcept
  : header_{std::move(header)},
    prefix_{static_cast<uint32_t>(prefix)},
    sub_count_{0} {
  IRS_ASSERT(prefix <= std::numeric_limits<uint32_t>::max());

  cur_meta_ = meta_ = *header_.begin++;
  cur_end_ = cur_start_ = start_ = vread<uint64_t>(header_.begin);
  if (block_meta::floor(meta_)) {
    sub_count_ = vread<uint32_t>(header_.begin);
    next_label_ = *header_.begin++;
  }
  header_.assert_block_boundaries();
}

void block_iterator::load(index_input& in, irs::encryption::stream* cipher) {
  if (!dirty_) {
    return;
  }

  in.seek(cur_start_);
  if (shift_unpack_32(in.read_vint(), ent_count_)) {
    sub_count_ = 0;  // no sub-blocks
  }

  // read suffix block
  uint64_t block_size;
  leaf_ = shift_unpack_64(in.read_vlong(), block_size);

  // for non-encrypted index try direct buffer access first
  suffix_.begin =
    cipher ? nullptr : in.read_buffer(block_size, BufferHint::PERSISTENT);
  suffix_.block.clear();

  if (!suffix_.begin) {
    suffix_.block.resize(block_size);
    [[maybe_unused]] const auto read =
      in.read_bytes(suffix_.block.data(), block_size);
    IRS_ASSERT(read == block_size);
    suffix_.begin = suffix_.block.c_str();

    if (cipher) {
      cipher->decrypt(cur_start_, suffix_.block.data(), block_size);
    }
  }
#ifdef IRESEARCH_DEBUG
  suffix_.end = suffix_.begin + block_size;
#endif
  suffix_.assert_block_boundaries();

  // read stats block
  block_size = in.read_vlong();

  // try direct buffer access first
  stats_.begin = in.read_buffer(block_size, BufferHint::PERSISTENT);
  stats_.block.clear();

  if (!stats_.begin) {
    stats_.block.resize(block_size);
    [[maybe_unused]] const auto read =
      in.read_bytes(stats_.block.data(), block_size);
    IRS_ASSERT(read == block_size);
    stats_.begin = stats_.block.c_str();
  }
#ifdef IRESEARCH_DEBUG
  stats_.end = stats_.begin + block_size;
#endif
  stats_.assert_block_boundaries();

  cur_end_ = in.file_pointer();
  cur_ent_ = 0;
  cur_block_start_ = UNDEFINED_ADDRESS;
  term_count_ = 0;
  cur_stats_ent_ = 0;
  dirty_ = false;
}

template<typename Reader>
void block_iterator::read_entry_nonleaf(Reader&& reader) {
  IRS_ASSERT(!leaf_ && cur_ent_ < ent_count_);

  cur_type_ = shift_unpack_32<EntryType, size_t>(vread<uint32_t>(suffix_.begin),
                                                 suffix_length_);
  suffix_begin_ = suffix_.begin;
  suffix_.begin += suffix_length_;
  suffix_.assert_block_boundaries();

  if (ET_TERM == cur_type_) {
    ++term_count_;
  } else {
    IRS_ASSERT(ET_BLOCK == cur_type_);
    cur_block_start_ = cur_start_ - vread<uint64_t>(suffix_.begin);
    suffix_.assert_block_boundaries();
  }

  // read after state is updated
  reader(suffix_begin_, suffix_length_);
}

template<typename Reader>
SeekResult block_iterator::scan_leaf(Reader&& reader) {
  IRS_ASSERT(leaf_);
  IRS_ASSERT(!dirty_);
  IRS_ASSERT(ent_count_ >= cur_ent_);

  SeekResult res = SeekResult::END;
  cur_type_ = ET_TERM;  // leaf block contains terms only

  size_t suffix_length = suffix_length_;
  size_t count = 0;

  for (const size_t left = ent_count_ - cur_ent_; count < left;) {
    ++count;
    suffix_length = vread<uint32_t>(suffix_.begin);
    res = reader(suffix_.begin, suffix_length);
    suffix_.begin += suffix_length;  // skip to the next term

    if (res != SeekResult::NOT_FOUND) {
      break;
    }
  }

  cur_ent_ += count;
  term_count_ = cur_ent_;

  suffix_begin_ = suffix_.begin - suffix_length;
  suffix_length_ = suffix_length;
  suffix_.assert_block_boundaries();

  return res;
}

template<typename Reader>
SeekResult block_iterator::scan_nonleaf(Reader&& reader) {
  IRS_ASSERT(!leaf_);
  IRS_ASSERT(!dirty_);

  SeekResult res = SeekResult::END;

  while (cur_ent_ < ent_count_) {
    ++cur_ent_;
    cur_type_ = shift_unpack_32<EntryType, size_t>(
      vread<uint32_t>(suffix_.begin), suffix_length_);
    const bool is_block = cur_type_ == ET_BLOCK;
    suffix_.assert_block_boundaries();

    suffix_begin_ = suffix_.begin;
    suffix_.begin += suffix_length_;  // skip to the next entry
    suffix_.assert_block_boundaries();

    if (ET_TERM == cur_type_) {
      ++term_count_;
    } else {
      IRS_ASSERT(cur_type_ == ET_BLOCK);
      cur_block_start_ = cur_start_ - vread<uint64_t>(suffix_.begin);
      suffix_.assert_block_boundaries();
    }

    // FIXME
    // we're not allowed to access/modify any block_iterator's
    // member as current instance might be already moved due
    // to a reallocation
    res = reader(suffix_begin_, suffix_length_);

    if (res != SeekResult::NOT_FOUND || is_block) {
      break;
    }
  }

  return res;
}

template<typename Reader>
SeekResult block_iterator::scan_to_term_leaf(bytes_view term, Reader&& reader) {
  IRS_ASSERT(leaf_);
  IRS_ASSERT(!dirty_);
  IRS_ASSERT(term.size() >= prefix_);

  const size_t term_suffix_length = term.size() - prefix_;
  const byte_type* term_suffix = term.data() + prefix_;
  size_t suffix_length = suffix_length_;
  cur_type_ = ET_TERM;  // leaf block contains terms only
  SeekResult res = SeekResult::END;

  uint32_t count = 0;
  for (uint32_t left = ent_count_ - cur_ent_; count < left;) {
    ++count;
    suffix_length = vread<uint32_t>(suffix_.begin);
    suffix_.assert_block_boundaries();

    ptrdiff_t cmp = std::memcmp(suffix_.begin, term_suffix,
                                std::min(suffix_length, term_suffix_length));

    if (cmp == 0) {
      cmp = suffix_length - term_suffix_length;
    }

    suffix_.begin += suffix_length;  // skip to the next term
    suffix_.assert_block_boundaries();

    if (cmp >= 0) {
      res = (cmp == 0 ? SeekResult::FOUND        // match!
                      : SeekResult::NOT_FOUND);  // after the target, not found
      break;
    }
  }

  cur_ent_ += count;
  term_count_ = cur_ent_;
  suffix_begin_ = suffix_.begin - suffix_length;
  suffix_length_ = suffix_length;
  reader(suffix_begin_, suffix_length);

  suffix_.assert_block_boundaries();
  return res;
}

template<typename Reader>
SeekResult block_iterator::scan_to_term_nonleaf(bytes_view term,
                                                Reader&& reader) {
  IRS_ASSERT(!leaf_);
  IRS_ASSERT(!dirty_);
  IRS_ASSERT(term.size() >= prefix_);

  const size_t term_suffix_length = term.size() - prefix_;
  const byte_type* term_suffix = term.data() + prefix_;
  const byte_type* suffix_begin = suffix_begin_;
  size_t suffix_length = suffix_length_;
  SeekResult res = SeekResult::END;

  while (cur_ent_ < ent_count_) {
    ++cur_ent_;
    cur_type_ = shift_unpack_32<EntryType, size_t>(
      vread<uint32_t>(suffix_.begin), suffix_length);
    suffix_.assert_block_boundaries();
    suffix_begin = suffix_.begin;
    suffix_.begin += suffix_length;  // skip to the next entry
    suffix_.assert_block_boundaries();

    if (ET_TERM == cur_type_) {
      ++term_count_;
    } else {
      IRS_ASSERT(ET_BLOCK == cur_type_);
      cur_block_start_ = cur_start_ - vread<uint64_t>(suffix_.begin);
      suffix_.assert_block_boundaries();
    }

    ptrdiff_t cmp = std::memcmp(suffix_begin, term_suffix,
                                std::min(suffix_length, term_suffix_length));

    if (cmp == 0) {
      cmp = suffix_length - term_suffix_length;
    }

    if (cmp >= 0) {
      res =
        (cmp == 0 ? SeekResult::FOUND        // match!
                  : SeekResult::NOT_FOUND);  // we after the target, not found
      break;
    }
  }

  suffix_begin_ = suffix_begin;
  suffix_length_ = suffix_length;
  reader(suffix_begin, suffix_length);

  suffix_.assert_block_boundaries();
  return res;
}

void block_iterator::scan_to_sub_block(byte_type label) {
  IRS_ASSERT(sub_count_ != UNDEFINED_COUNT);

  if (!sub_count_ || !block_meta::floor(meta_)) {
    // no sub-blocks, nothing to do
    return;
  }

  const uint16_t target = label;  // avoid byte_type vs uint16_t comparison

  if (target < next_label_) {
    // we don't need search
    return;
  }

  // FIXME: binary search???
  uint64_t start_delta = 0;
  for (;;) {
    start_delta = vread<uint64_t>(header_.begin);
    cur_meta_ = *header_.begin++;
    if (--sub_count_) {
      next_label_ = *header_.begin++;

      if (target < next_label_) {
        break;
      }
    } else {
      next_label_ = block_t::INVALID_LABEL;
      break;
    }
  }

  if (start_delta) {
    cur_start_ = start_ + start_delta;
    cur_ent_ = 0;
    dirty_ = true;
  }

  header_.assert_block_boundaries();
}

void block_iterator::scan_to_block(uint64_t start) {
  if (leaf_) {
    // must be a non leaf block
    return;
  }

  if (cur_block_start_ == start) {
    // nothing to do
    return;
  }

  const uint64_t target = cur_start_ - start;  // delta
  for (; cur_ent_ < ent_count_;) {
    ++cur_ent_;
    const auto type = shift_unpack_32<EntryType, size_t>(
      vread<uint32_t>(suffix_.begin), suffix_length_);
    suffix_.assert_block_boundaries();
    suffix_.begin += suffix_length_;
    suffix_.assert_block_boundaries();

    if (ET_TERM == type) {
      ++term_count_;
    } else {
      IRS_ASSERT(ET_BLOCK == type);
      if (vread<uint64_t>(suffix_.begin) == target) {
        suffix_.assert_block_boundaries();
        cur_block_start_ = target;
        return;
      }
      suffix_.assert_block_boundaries();
    }
  }

  IRS_ASSERT(false);
}

void block_iterator::load_data(const field_meta& meta,
                               version10::term_meta& state,
                               irs::postings_reader& pr) {
  IRS_ASSERT(ET_TERM == cur_type_);

  if (cur_stats_ent_ >= term_count_) {
    return;
  }

  if (0 == cur_stats_ent_) {
    // clear state at the beginning
    state.clear();
  } else {
    state = state_;
  }

  for (; cur_stats_ent_ < term_count_; ++cur_stats_ent_) {
    stats_.begin += pr.decode(stats_.begin, meta.index_features, state);
    stats_.assert_block_boundaries();
  }

  state_ = state;
}

void block_iterator::reset() {
  if (sub_count_ != UNDEFINED_COUNT) {
    sub_count_ = 0;
  }
  next_label_ = block_t::INVALID_LABEL;
  cur_start_ = start_;
  cur_meta_ = meta_;
  if (block_meta::floor(meta_)) {
    IRS_ASSERT(sub_count_ != UNDEFINED_COUNT);
    header_.begin = header_.block.c_str() + 1;  // +1 to skip meta
    vskip<uint64_t>(header_.begin);             // skip address
    sub_count_ = vread<uint32_t>(header_.begin);
    next_label_ = *header_.begin++;
  }
  dirty_ = true;
  header_.assert_block_boundaries();
}

// Base class for term_iterator and automaton_term_iterator
class term_iterator_base : public seek_term_iterator {
 public:
  term_iterator_base(const term_reader_base& field, postings_reader& postings,
                     irs::encryption::stream* terms_cipher,
                     payload* pay = nullptr)
    : field_{&field}, postings_{&postings}, terms_cipher_{terms_cipher} {
    std::get<attribute_ptr<payload>>(attrs_) = pay;
  }

  attribute* get_mutable(irs::type_info::type_id type) noexcept final {
    return irs::get_mutable(attrs_, type);
  }

  seek_cookie::ptr cookie() const final {
    return std::make_unique<::cookie>(std::get<version10::term_meta>(attrs_));
  }

  bytes_view value() const noexcept final {
    return std::get<term_attribute>(attrs_).value;
  }

  irs::encryption::stream* terms_cipher() const noexcept {
    return terms_cipher_;
  }

 protected:
  using attributes =
    std::tuple<version10::term_meta, term_attribute, attribute_ptr<payload>>;

  void read_impl(block_iterator& it) {
    it.load_data(field_->meta(), std::get<version10::term_meta>(attrs_),
                 *postings_);
  }

  doc_iterator::ptr postings_impl(block_iterator* it,
                                  IndexFeatures features) const {
    auto& meta = std::get<version10::term_meta>(attrs_);
    const auto& field_meta = field_->meta();
    if (it) {
      it->load_data(field_meta, meta, *postings_);
    }
    return postings_->iterator(field_meta.index_features, features, meta,
                               field_->WandCount());
  }

  void copy(const byte_type* suffix, size_t prefix_size, size_t suffix_size) {
    absl::strings_internal::STLStringResizeUninitializedAmortized(
      &term_buf_, prefix_size + suffix_size);
    std::memcpy(term_buf_.data() + prefix_size, suffix, suffix_size);
  }

  void refresh_value() noexcept {
    std::get<term_attribute>(attrs_).value = term_buf_;
  }
  void reset_value() noexcept { std::get<term_attribute>(attrs_).value = {}; }

  mutable attributes attrs_;
  const term_reader_base* field_;
  postings_reader* postings_;
  irs::encryption::stream* terms_cipher_;
  bstring term_buf_;
  byte_weight weight_;  // aggregated fst output
};

// use explicit matcher to avoid implicit loops
template<typename FST>
using explicit_matcher = fst::explicit_matcher<fst::SortedMatcher<FST>>;

template<typename FST>
class term_iterator : public term_iterator_base {
 public:
  using weight_t = typename FST::Weight;
  using stateid_t = typename FST::StateId;

  term_iterator(const term_reader_base& field, postings_reader& postings,
                const index_input& terms_in,
                irs::encryption::stream* terms_cipher, const FST& fst)
    : term_iterator_base{field, postings, terms_cipher, nullptr},
      terms_in_source_{&terms_in},
      fst_{&fst},
      matcher_{&fst, fst::MATCH_INPUT} {  // pass pointer to avoid copying FST
  }

  bool next() final;
  SeekResult seek_ge(bytes_view term) final;
  bool seek(bytes_view term) final {
    return SeekResult::FOUND == seek_equal(term, true);
  }

  void read() final {
    IRS_ASSERT(cur_block_);
    read_impl(*cur_block_);
  }

  doc_iterator::ptr postings(IndexFeatures features) const final {
    return postings_impl(cur_block_, features);
  }

 private:
  friend class block_iterator;

  struct arc {
    arc() = default;
    arc(stateid_t state, bytes_view weight, size_t block) noexcept
      : state(state), weight(weight), block(block) {}

    stateid_t state;
    bytes_view weight;
    size_t block;
  };

  static_assert(std::is_nothrow_move_constructible_v<arc>);

  ptrdiff_t seek_cached(size_t& prefix, stateid_t& state, size_t& block,
                        byte_weight& weight, bytes_view term);

  // Seek to the closest block which contain a specified term
  // prefix - size of the common term/block prefix
  // Returns true if we're already at a requested term
  bool seek_to_block(bytes_view term, size_t& prefix);

  // Seeks to the specified term using FST
  // There may be several sutuations:
  //   1. There is no term in a block (SeekResult::NOT_FOUND)
  //   2. There is no term in a block and we have
  //      reached the end of the block (SeekResult::END)
  //   3. We have found term in a block (SeekResult::FOUND)
  //
  // Note, that search may end up on a BLOCK entry. In all cases
  // "owner_->term_" will be refreshed with the valid number of
  // common bytes
  SeekResult seek_equal(bytes_view term, bool exact);

  block_iterator* pop_block() noexcept {
    block_stack_.pop_back();
    IRS_ASSERT(!block_stack_.empty());
    return &block_stack_.back();
  }

  block_iterator* push_block(bytes_view out, size_t prefix) {
    // ensure final weight correctess
    IRS_ASSERT(out.size() >= MIN_WEIGHT_SIZE);

    return &block_stack_.emplace_back(out, prefix);
  }

  block_iterator* push_block(byte_weight&& out, size_t prefix) {
    // ensure final weight correctess
    IRS_ASSERT(out.Size() >= MIN_WEIGHT_SIZE);

    return &block_stack_.emplace_back(std::move(out), prefix);
  }

  block_iterator* push_block(uint64_t start, size_t prefix) {
    return &block_stack_.emplace_back(start, prefix);
  }

  // as term_iterator is usually used by prepared queries
  // to access term metadata by stored cookie, we initialize
  // term dictionary stream in lazy fashion
  index_input& terms_input() const {
    if (!terms_in_) {
      terms_in_ = terms_in_source_->reopen();  // reopen thread-safe stream

      if (!terms_in_) {
        // implementation returned wrong pointer
        IRS_LOG_ERROR("Failed to reopen terms input");

        throw io_error("failed to reopen terms input");
      }
    }

    return *terms_in_;
  }

  const index_input* terms_in_source_;
  mutable index_input::ptr terms_in_;
  const FST* fst_;
  explicit_matcher<FST> matcher_;
  std::vector<arc> sstate_;
  std::vector<block_iterator> block_stack_;
  block_iterator* cur_block_{};
};

template<typename FST>
bool term_iterator<FST>::next() {
  // iterator at the beginning or seek to cached state was called
  if (!cur_block_) {
    if (value().empty()) {
      // iterator at the beginning
      cur_block_ = push_block(fst_->Final(fst_->Start()), 0);
      cur_block_->load(terms_input(), terms_cipher());
    } else {
      IRS_ASSERT(false);
      // FIXME(gnusi): consider removing this, as that seems to be impossible
      // anymore

      // seek to the term with the specified state was called from
      // term_iterator::seek(bytes_view, const attribute&),
      // need create temporary "bytes_view" here, since "seek" calls
      // term_.reset() internally,
      // note, that since we do not create extra copy of term_
      // make sure that it does not reallocate memory !!!
      [[maybe_unused]] const auto res = seek_equal(value(), true);
      IRS_ASSERT(SeekResult::FOUND == res);
    }
  }

  // pop finished blocks
  while (cur_block_->done()) {
    if (cur_block_->next_sub_block<false>()) {
      cur_block_->load(terms_input(), terms_cipher());
    } else if (&block_stack_.front() == cur_block_) {  // root
      reset_value();
      cur_block_->reset();
      sstate_.clear();
      return false;
    } else {
      const uint64_t start = cur_block_->start();
      cur_block_ = pop_block();
      std::get<version10::term_meta>(attrs_) = cur_block_->state();
      if (cur_block_->dirty() || cur_block_->block_start() != start) {
        // here we're currently at non block that was not loaded yet
        IRS_ASSERT(cur_block_->prefix() < term_buf_.size());
        // to sub-block
        cur_block_->scan_to_sub_block(term_buf_[cur_block_->prefix()]);
        cur_block_->load(terms_input(), terms_cipher());
        cur_block_->scan_to_block(start);
      }
    }
  }

  sstate_.resize(std::min(sstate_.size(), cur_block_->prefix()));

  auto copy_suffix = [this](const byte_type* suffix, size_t suffix_size) {
    copy(suffix, cur_block_->prefix(), suffix_size);
  };

  // push new block or next term
  for (cur_block_->next(copy_suffix); EntryType::ET_BLOCK == cur_block_->type();
       cur_block_->next(copy_suffix)) {
    cur_block_ = push_block(cur_block_->block_start(), term_buf_.size());
    cur_block_->load(terms_input(), terms_cipher());
  }

  refresh_value();
  return true;
}

#if defined(_MSC_VER)
#pragma warning(disable : 4706)
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wparentheses"
#endif

template<typename FST>
ptrdiff_t term_iterator<FST>::seek_cached(size_t& prefix, stateid_t& state,
                                          size_t& block, byte_weight& weight,
                                          bytes_view target) {
  IRS_ASSERT(!block_stack_.empty());
  const auto term = value();
  const byte_type* pterm = term.data();
  const byte_type* ptarget = target.data();

  // determine common prefix between target term and current
  {
    auto begin = sstate_.begin();
    auto end = begin + std::min(target.size(), sstate_.size());

    for (; begin != end && *pterm == *ptarget; ++begin, ++pterm, ++ptarget) {
      auto& state_weight = begin->weight;
      weight.PushBack(state_weight.begin(), state_weight.end());
      state = begin->state;
      block = begin->block;
    }

    prefix = size_t(pterm - term.data());
  }

  // inspect suffix and determine our current position
  // with respect to target term (before, after, equal)
  ptrdiff_t cmp = char_traits<byte_type>::compare(
    pterm, ptarget, std::min(target.size(), term.size()) - prefix);

  if (!cmp) {
    cmp = term.size() - target.size();
  }

  if (cmp) {
    // truncate block_stack_ to match path
    const auto begin = block_stack_.begin() + (block + 1);
    IRS_ASSERT(begin <= block_stack_.end());
    block_stack_.erase(begin, block_stack_.end());
  }

  // cmp < 0 : target term is after the current term
  // cmp == 0 : target term is current term
  // cmp > 0 : target term is before current term
  return cmp;
}

template<typename FST>
bool term_iterator<FST>::seek_to_block(bytes_view term, size_t& prefix) {
  IRS_ASSERT(fst_->GetImpl());
  auto& fst = *fst_->GetImpl();

  prefix = 0;                     // number of current symbol to process
  stateid_t state = fst.Start();  // start state
  weight_.Clear();                // clear aggregated fst output
  size_t block = 0;

  if (cur_block_) {
    const ptrdiff_t cmp = seek_cached(prefix, state, block, weight_, term);

    if (cmp > 0) {
      // target term is before the current term
      block_stack_[block].reset();
    } else if (0 == cmp) {
      if (cur_block_->type() == ET_BLOCK) {
        // we're at the block with matching prefix
        cur_block_ = push_block(cur_block_->block_start(), term_buf_.size());
        return false;
      } else {
        // we're already at current term
        return true;
      }
    }
  } else {
    push_block(fst.Final(state), prefix);
  }

  // reset to common seek prefix
  absl::strings_internal::STLStringReserveAmortized(&term_buf_, term.size());
  absl::strings_internal::STLStringResizeUninitialized(&term_buf_, prefix);
  sstate_.resize(prefix);  // remove invalid cached arcs

  while (fst_buffer::fst_byte_builder::final != state && prefix < term.size()) {
    matcher_.SetState(state);

    if (!matcher_.Find(term[prefix])) {
      break;
    }

    const auto& arc = matcher_.Value();

    term_buf_ += byte_type(arc.ilabel);  // aggregate arc label
    weight_.PushBack(arc.weight.begin(),
                     arc.weight.end());  // aggregate arc weight
    ++prefix;

    const auto& weight = fst.FinalRef(arc.nextstate);

    if (!weight.Empty()) {
      push_block(fst::Times(weight_, weight), prefix);
      ++block;
    } else if (fst_buffer::fst_byte_builder::final == arc.nextstate) {
      // ensure final state has no weight assigned
      // the only case when it's wrong is degenerated FST composed of only
      // 'fst_byte_builder::final' state.
      // in that case we'll never get there due to the loop condition above.
      IRS_ASSERT(fst.FinalRef(fst_buffer::fst_byte_builder::final).Empty());

      push_block(std::move(weight_), prefix);
      ++block;
    }

    // cache found arcs, we can reuse it in further seeks
    // avoiding relatively expensive FST lookups
    sstate_.emplace_back(arc.nextstate, arc.weight, block);

    // proceed to the next state
    state = arc.nextstate;
  }

  cur_block_ = &block_stack_[block];
  prefix = cur_block_->prefix();
  sstate_.resize(prefix);

  if (prefix < term.size()) {
    cur_block_->scan_to_sub_block(term[prefix]);
  }

  return false;
}

template<typename FST>
SeekResult term_iterator<FST>::seek_equal(bytes_view term, bool exact) {
  [[maybe_unused]] size_t prefix;
  if (seek_to_block(term, prefix)) {
    IRS_ASSERT(ET_TERM == cur_block_->type());
    return SeekResult::FOUND;
  }

  IRS_ASSERT(cur_block_);

  if (exact && cur_block_->no_terms()) {
    // current block has no terms
    std::get<term_attribute>(attrs_).value = {term_buf_.c_str(), prefix};
    return SeekResult::NOT_FOUND;
  }

  auto append_suffix = [this](const byte_type* suffix, size_t suffix_size) {
    const auto prefix = cur_block_->prefix();
    absl::strings_internal::STLStringResizeUninitializedAmortized(
      &term_buf_, prefix + suffix_size);
    std::memcpy(term_buf_.data() + prefix, suffix, suffix_size);
  };

  cur_block_->load(terms_input(), terms_cipher());

  Finally refresh_value = [this]() noexcept { this->refresh_value(); };

  IRS_ASSERT(term.starts_with(term_buf_));
  return cur_block_->scan_to_term(term, append_suffix);
}

template<typename FST>
SeekResult term_iterator<FST>::seek_ge(bytes_view term) {
  switch (seek_equal(term, false)) {
    case SeekResult::FOUND:
      IRS_ASSERT(ET_TERM == cur_block_->type());
      return SeekResult::FOUND;
    case SeekResult::NOT_FOUND:
      switch (cur_block_->type()) {
        case ET_TERM:
          // we're already at greater term
          return SeekResult::NOT_FOUND;
        case ET_BLOCK:
          // we're at the greater block, load it and call next
          cur_block_ = push_block(cur_block_->block_start(), term_buf_.size());
          cur_block_->load(terms_input(), terms_cipher());
          break;
        default:
          IRS_ASSERT(false);
          return SeekResult::END;
      }
      [[fallthrough]];
    case SeekResult::END:
      return next() ? SeekResult::NOT_FOUND  // have moved to the next entry
                    : SeekResult::END;       // have no more terms
  }

  IRS_ASSERT(false);
  return SeekResult::END;
}

// An iterator optimized for performing exact single seeks
//
// WARNING: we intentionally do not copy term value to avoid
//          unnecessary allocations as this is mostly useless
//          in case of exact single seek
template<typename FST>
class single_term_iterator : public seek_term_iterator {
 public:
  explicit single_term_iterator(const term_reader_base& field,
                                postings_reader& postings,
                                index_input::ptr&& terms_in,
                                irs::encryption::stream* terms_cipher,
                                const FST& fst) noexcept
    : terms_in_{std::move(terms_in)},
      cipher_{terms_cipher},
      postings_{&postings},
      field_{&field},
      fst_{&fst} {
    IRS_ASSERT(terms_in_);
  }

  attribute* get_mutable(irs::type_info::type_id type) final {
    if (type == irs::type<term_meta>::id()) {
      return &meta_;
    }

    return type == irs::type<term_attribute>::id() ? &value_ : nullptr;
  }

  bytes_view value() const final { return value_.value; }

  bool next() final { throw not_supported(); }

  SeekResult seek_ge(bytes_view) final { throw not_supported(); }

  bool seek(bytes_view term) final;

  seek_cookie::ptr cookie() const final {
    return std::make_unique<::cookie>(meta_);
  }

  void read() final { /*NOOP*/
  }

  doc_iterator::ptr postings(IndexFeatures features) const final {
    return postings_->iterator(field_->meta().index_features, features, meta_,
                               field_->WandCount());
  }

  const version10::term_meta& meta() const noexcept { return meta_; }

 private:
  friend class block_iterator;

  version10::term_meta meta_;
  term_attribute value_;
  index_input::ptr terms_in_;
  irs::encryption::stream* cipher_;
  postings_reader* postings_;
  const term_reader_base* field_;
  const FST* fst_;
};

template<typename FST>
bool single_term_iterator<FST>::seek(bytes_view term) {
  IRS_ASSERT(fst_->GetImpl());
  auto& fst = *fst_->GetImpl();

  auto state = fst.Start();
  explicit_matcher<FST> matcher{fst_, fst::MATCH_INPUT};

  byte_weight weight_prefix;
  const auto* weight_suffix = &fst.FinalRef(state);
  size_t weight_prefix_length = 0;
  size_t block_prefix = 0;

  matcher.SetState(state);

  for (size_t prefix = 0; prefix < term.size() && matcher.Find(term[prefix]);
       matcher.SetState(state)) {
    const auto& arc = matcher.Value();
    state = arc.nextstate;
    weight_prefix.PushBack(arc.weight.begin(), arc.weight.end());
    ++prefix;

    auto& weight = fst.FinalRef(state);

    if (!weight.Empty() || fst_buffer::fst_byte_builder::final == state) {
      weight_prefix_length = weight_prefix.Size();
      weight_suffix = &weight;
      block_prefix = prefix;

      if (fst_buffer::fst_byte_builder::final == state) {
        break;
      }
    }
  }

  weight_prefix.Resize(weight_prefix_length);
  weight_prefix.PushBack(weight_suffix->begin(), weight_suffix->end());
  block_iterator cur_block{std::move(weight_prefix), block_prefix};

  if (block_prefix < term.size()) {
    cur_block.scan_to_sub_block(term[block_prefix]);
  }

  if (!block_meta::terms(cur_block.meta())) {
    return false;
  }

  cur_block.load(*terms_in_, cipher_);

  if (SeekResult::FOUND == cur_block.scan_to_term(term, [](auto, auto) {})) {
    cur_block.load_data(field_->meta(), meta_, *postings_);
    value_.value = term;
    return true;
  }

  value_ = {};
  return false;
}

class automaton_arc_matcher {
 public:
  automaton_arc_matcher(const automaton::Arc* arcs, size_t narcs) noexcept
    : begin_(arcs), end_(arcs + narcs) {}

  const automaton::Arc* seek(uint32_t label) noexcept {
    IRS_ASSERT(begin_ != end_ && begin_->max < label);
    // linear search is faster for a small number of arcs

    while (++begin_ != end_ && begin_->max < label) {
    }

    IRS_ASSERT(begin_ == end_ || label <= begin_->max);

    return begin_ != end_ && begin_->min <= label ? begin_ : nullptr;
  }

  const automaton::Arc* value() const noexcept { return begin_; }

  bool done() const noexcept { return begin_ == end_; }

 private:
  const automaton::Arc* begin_;  // current arc
  const automaton::Arc* end_;    // end of arcs range
};

template<typename FST>
class fst_arc_matcher {
 public:
  fst_arc_matcher(const FST& fst, typename FST::StateId state) noexcept {
    fst::ArcIteratorData<typename FST::Arc> data;
    fst.InitArcIterator(state, &data);
    begin_ = data.arcs;
    end_ = begin_ + data.narcs;
  }

  void seek(typename FST::Arc::Label label) noexcept {
    // linear search is faster for a small number of arcs
    for (; begin_ != end_; ++begin_) {
      if (label <= begin_->ilabel) {
        break;
      }
    }
  }

  const typename FST::Arc* value() const noexcept { return begin_; }

  bool done() const noexcept { return begin_ == end_; }

 private:
  const typename FST::Arc* begin_;  // current arc
  const typename FST::Arc* end_;    // end of arcs range
};

template<typename FST>
class automaton_term_iterator : public term_iterator_base {
 public:
  automaton_term_iterator(const term_reader_base& field,
                          postings_reader& postings,
                          index_input::ptr&& terms_in,
                          irs::encryption::stream* terms_cipher, const FST& fst,
                          automaton_table_matcher& matcher)
    : term_iterator_base{field, postings, terms_cipher, &payload_},
      terms_in_{std::move(terms_in)},
      fst_{&fst},
      acceptor_{&matcher.GetFst()},
      matcher_{&matcher},
      fst_matcher_{&fst, fst::MATCH_INPUT},
      sink_{matcher.sink()} {
    IRS_ASSERT(terms_in_);
    IRS_ASSERT(fst::kNoStateId != acceptor_->Start());
    IRS_ASSERT(acceptor_->NumArcs(acceptor_->Start()));

    // init payload value
    payload_.value = {&payload_value_, sizeof(payload_value_)};
  }

  bool next() final;

  SeekResult seek_ge(bytes_view term) final {
    if (!irs::seek(*this, term)) {
      return SeekResult::END;
    }

    return value() == term ? SeekResult::FOUND : SeekResult::NOT_FOUND;
  }

  bool seek(bytes_view term) final {
    return SeekResult::FOUND == seek_ge(term);
  }

  void read() final {
    IRS_ASSERT(cur_block_);
    read_impl(*cur_block_);
  }

  doc_iterator::ptr postings(IndexFeatures features) const final {
    return postings_impl(cur_block_, features);
  }

 private:
  class block_iterator : public ::block_iterator {
   public:
    block_iterator(bytes_view out, const FST& fst, size_t prefix,
                   size_t weight_prefix, automaton::StateId state,
                   typename FST::StateId fst_state, const automaton::Arc* arcs,
                   size_t narcs) noexcept
      : ::block_iterator(out, prefix),
        arcs_(arcs, narcs),
        fst_arcs_(fst, fst_state),
        weight_prefix_(weight_prefix),
        state_(state),
        fst_state_(fst_state) {}

   public:
    fst_arc_matcher<FST>& fst_arcs() noexcept { return fst_arcs_; }
    automaton_arc_matcher& arcs() noexcept { return arcs_; }
    automaton::StateId acceptor_state() const noexcept { return state_; }
    typename FST::StateId fst_state() const noexcept { return fst_state_; }
    size_t weight_prefix() const noexcept { return weight_prefix_; }

   private:
    automaton_arc_matcher arcs_;
    fst_arc_matcher<FST> fst_arcs_;
    size_t weight_prefix_;
    automaton::StateId state_;  // state to which current block belongs
    typename FST::StateId fst_state_;
  };

  block_iterator* pop_block() noexcept {
    block_stack_.pop_back();
    IRS_ASSERT(!block_stack_.empty());
    return &block_stack_.back();
  }

  block_iterator* push_block(bytes_view out, const FST& fst, size_t prefix,
                             size_t weight_prefix, automaton::StateId state,
                             typename FST::StateId fst_state) {
    // ensure final weight correctness
    IRS_ASSERT(out.size() >= MIN_WEIGHT_SIZE);

    fst::ArcIteratorData<automaton::Arc> data;
    acceptor_->InitArcIterator(state, &data);
    IRS_ASSERT(data.narcs);  // ensured by term_reader::iterator(...)

    return &block_stack_.emplace_back(out, fst, prefix, weight_prefix, state,
                                      fst_state, data.arcs, data.narcs);
  }

  // automaton_term_iterator usually accesses many term blocks and
  // isn't used by prepared statements for accessing term metadata,
  // therefore we prefer greedy strategy for term dictionary stream
  // initialization
  index_input& terms_input() const noexcept {
    IRS_ASSERT(terms_in_);
    return *terms_in_;
  }

  index_input::ptr terms_in_;
  const FST* fst_;
  const automaton* acceptor_;
  automaton_table_matcher* matcher_;
  explicit_matcher<FST> fst_matcher_;
  std::vector<block_iterator> block_stack_;
  block_iterator* cur_block_{};
  automaton::Weight::PayloadType payload_value_;
  payload payload_;  // payload of the matched automaton state
  automaton::StateId sink_;
};

template<typename FST>
bool automaton_term_iterator<FST>::next() {
  IRS_ASSERT(fst_matcher_.GetFst().GetImpl());
  auto& fst = *fst_matcher_.GetFst().GetImpl();

  // iterator at the beginning or seek to cached state was called
  if (!cur_block_) {
    if (value().empty()) {
      const auto fst_start = fst.Start();
      cur_block_ = push_block(fst.Final(fst_start), *fst_, 0, 0,
                              acceptor_->Start(), fst_start);
      cur_block_->load(terms_input(), terms_cipher());
    } else {
      IRS_ASSERT(false);
      // FIXME(gnusi): consider removing this, as that seems to be impossible
      // anymore

      // seek to the term with the specified state was called from
      // term_iterator::seek(bytes_view, const attribute&),
      const SeekResult res = seek_ge(value());
      IRS_ASSERT(SeekResult::FOUND == res);
      IRS_IGNORE(res);
    }
  }

  automaton::StateId state;

  auto read_suffix = [this, &state, &fst](const byte_type* suffix,
                                          size_t suffix_size) -> SeekResult {
    if (suffix_size) {
      auto& arcs = cur_block_->arcs();
      IRS_ASSERT(!arcs.done());

      const auto* arc = arcs.value();

      const uint32_t lead_label = *suffix;

      if (lead_label < arc->min) {
        return SeekResult::NOT_FOUND;
      }

      if (lead_label > arc->max) {
        arc = arcs.seek(lead_label);

        if (!arc) {
          if (arcs.done()) {
            return SeekResult::END;  // pop current block
          }

          return SeekResult::NOT_FOUND;
        }
      }

      IRS_ASSERT(*suffix >= arc->min && *suffix <= arc->max);
      state = arc->nextstate;

      if (state == sink_) {
        return SeekResult::NOT_FOUND;
      }

#ifdef IRESEARCH_DEBUG
      IRS_ASSERT(matcher_->Peek(cur_block_->acceptor_state(), *suffix) ==
                 state);
#endif

      const auto* end = suffix + suffix_size;
      const auto* begin = suffix + 1;  // already match first suffix label

      for (; begin < end; ++begin) {
        state = matcher_->Peek(state, *begin);

        if (fst::kNoStateId == state) {
          // suffix doesn't match
          return SeekResult::NOT_FOUND;
        }
      }
    } else {
      state = cur_block_->acceptor_state();
    }

    if (ET_TERM == cur_block_->type()) {
      const auto weight = acceptor_->Final(state);
      if (weight) {
        payload_value_ = weight.Payload();
        copy(suffix, cur_block_->prefix(), suffix_size);

        return SeekResult::FOUND;
      }
    } else {
      IRS_ASSERT(ET_BLOCK == cur_block_->type());
      fst::ArcIteratorData<automaton::Arc> data;
      acceptor_->InitArcIterator(state, &data);

      if (data.narcs) {
        copy(suffix, cur_block_->prefix(), suffix_size);

        weight_.Resize(cur_block_->weight_prefix());
        auto fst_state = cur_block_->fst_state();

        if (const auto* end = suffix + suffix_size; suffix < end) {
          auto& fst_arcs = cur_block_->fst_arcs();
          fst_arcs.seek(*suffix++);
          IRS_ASSERT(!fst_arcs.done());
          const auto* arc = fst_arcs.value();
          weight_.PushBack(arc->weight.begin(), arc->weight.end());

          fst_state = fst_arcs.value()->nextstate;
          for (fst_matcher_.SetState(fst_state); suffix < end; ++suffix) {
            [[maybe_unused]] const bool found = fst_matcher_.Find(*suffix);
            IRS_ASSERT(found);

            const auto& arc = fst_matcher_.Value();
            fst_state = arc.nextstate;
            fst_matcher_.SetState(fst_state);
            weight_.PushBack(arc.weight.begin(), arc.weight.end());
          }
        }

        const auto& weight = fst.FinalRef(fst_state);
        IRS_ASSERT(!weight.Empty() ||
                   fst_buffer::fst_byte_builder::final == fst_state);
        const auto weight_prefix = weight_.Size();
        weight_.PushBack(weight.begin(), weight.end());
        block_stack_.emplace_back(static_cast<bytes_view>(weight_), *fst_,
                                  term_buf_.size(), weight_prefix, state,
                                  fst_state, data.arcs, data.narcs);
        cur_block_ = &block_stack_.back();

        IRS_ASSERT(block_stack_.size() < 2 ||
                   (++block_stack_.rbegin())->block_start() ==
                     cur_block_->start());

        if (!acceptor_->Final(state)) {
          cur_block_->scan_to_sub_block(data.arcs->min);
        }

        cur_block_->load(terms_input(), terms_cipher());
      }
    }

    return SeekResult::NOT_FOUND;
  };

  for (;;) {
    // pop finished blocks
    while (cur_block_->done()) {
      if (cur_block_->sub_count()) {
        // we always instantiate block with header
        IRS_ASSERT(block_t::INVALID_LABEL != cur_block_->next_label());

        const uint32_t next_label = cur_block_->next_label();

        auto& arcs = cur_block_->arcs();
        IRS_ASSERT(!arcs.done());
        const auto* arc = arcs.value();

        if (next_label < arc->min) {
          IRS_ASSERT(arc->min <= std::numeric_limits<uint8_t>::max());
          cur_block_->scan_to_sub_block(byte_type(arc->min));
          IRS_ASSERT(cur_block_->next_label() == block_t::INVALID_LABEL ||
                     arc->min < uint32_t(cur_block_->next_label()));
        } else if (arc->max < next_label) {
          arc = arcs.seek(next_label);

          if (arcs.done()) {
            if (&block_stack_.front() == cur_block_) {
              // need to pop root block, we're done
              reset_value();
              cur_block_->reset();
              return false;
            }

            cur_block_ = pop_block();
            continue;
          }

          if (!arc) {
            IRS_ASSERT(arcs.value()->min <=
                       std::numeric_limits<uint8_t>::max());
            cur_block_->scan_to_sub_block(byte_type(arcs.value()->min));
            IRS_ASSERT(cur_block_->next_label() == block_t::INVALID_LABEL ||
                       arcs.value()->min < uint32_t(cur_block_->next_label()));
          } else {
            IRS_ASSERT(arc->min <= next_label && next_label <= arc->max);
            cur_block_->template next_sub_block<true>();
          }
        } else {
          IRS_ASSERT(arc->min <= next_label && next_label <= arc->max);
          cur_block_->template next_sub_block<true>();
        }

        cur_block_->load(terms_input(), terms_cipher());
      } else if (&block_stack_.front() == cur_block_) {  // root
        reset_value();
        cur_block_->reset();
        return false;
      } else {
        const uint64_t start = cur_block_->start();
        cur_block_ = pop_block();
        std::get<version10::term_meta>(attrs_) = cur_block_->state();
        if (cur_block_->dirty() || cur_block_->block_start() != start) {
          // here we're currently at non block that was not loaded yet
          IRS_ASSERT(cur_block_->prefix() < term_buf_.size());
          // to sub-block
          cur_block_->scan_to_sub_block(term_buf_[cur_block_->prefix()]);
          cur_block_->load(terms_input(), terms_cipher());
          cur_block_->scan_to_block(start);
        }
      }
    }

    const auto res = cur_block_->scan(read_suffix);

    if (SeekResult::FOUND == res) {
      refresh_value();
      return true;
    } else if (SeekResult::END == res) {
      if (&block_stack_.front() == cur_block_) {
        // need to pop root block, we're done
        reset_value();
        cur_block_->reset();
        return false;
      }

      // continue with popped block
      cur_block_ = pop_block();
    }
  }
}

#if defined(_MSC_VER)
#pragma warning(default : 4706)
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

class field_reader final : public irs::field_reader {
 public:
  explicit field_reader(irs::postings_reader::ptr&& pr, IResourceManager& rm);

  uint64_t CountMappedMemory() const final {
    uint64_t mapped{0};
    if (pr_ != nullptr) {
      mapped += pr_->CountMappedMemory();
    }
    if (terms_in_ != nullptr) {
      mapped += terms_in_->CountMappedMemory();
    }
    return mapped;
  }

  void prepare(const ReaderState& state) final;

  const irs::term_reader* field(std::string_view field) const final;
  irs::field_iterator::ptr iterator() const final;
  size_t size() const noexcept final { return name_to_field_.size(); }

 private:
  template<typename FST>
  class term_reader final : public term_reader_base {
   public:
    explicit term_reader(field_reader& owner) noexcept : owner_(&owner) {}
    term_reader(term_reader&& rhs) = default;
    term_reader& operator=(term_reader&& rhs) = delete;

    void prepare(burst_trie::Version version, index_input& in,
                 const feature_map_t& features) final {
      term_reader_base::prepare(version, in, features);

      // read FST
      input_buf isb(&in);
      std::istream input(&isb);  // wrap stream to be OpenFST compliant
      fst_.reset(
        FST::Read(input, fst_read_options(), {owner_->resource_manager_}));
      if (!fst_) {
        throw index_error{absl::StrCat("Failed to read term index for field '",
                                       meta().name, "'")};
      }
    }

    seek_term_iterator::ptr iterator(SeekMode mode) const final {
      if (mode == SeekMode::RANDOM_ONLY) {
        auto terms_in =
          owner_->terms_in_->reopen();  // reopen thread-safe stream

        if (!terms_in) {
          // implementation returned wrong pointer
          IRS_LOG_ERROR("Failed to reopen terms input");

          throw io_error("failed to reopen terms input");
        }

        return memory::make_managed<single_term_iterator<FST>>(
          *this, *owner_->pr_, std::move(terms_in),
          owner_->terms_in_cipher_.get(), *fst_);
      }

      return memory::make_managed<term_iterator<FST>>(
        *this, *owner_->pr_, *owner_->terms_in_, owner_->terms_in_cipher_.get(),
        *fst_);
    }

    term_meta term(bytes_view term) const final {
      single_term_iterator<FST> it{*this, *owner_->pr_,
                                   owner_->terms_in_->reopen(),
                                   owner_->terms_in_cipher_.get(), *fst_};

      it.seek(term);
      return it.meta();
    }

    size_t read_documents(bytes_view term,
                          std::span<doc_id_t> docs) const final {
      // Order is important here!
      if (max() < term || term < min() || docs.empty()) {
        return 0;
      }

      single_term_iterator<FST> it{*this, *owner_->pr_,
                                   owner_->terms_in_->reopen(),
                                   owner_->terms_in_cipher_.get(), *fst_};

      if (!it.seek(term)) {
        return 0;
      }

      if (const auto& meta = it.meta(); meta.docs_count == 1) {
        docs.front() = doc_limits::min() + meta.e_single_doc;
        return 1;
      }

      auto docs_it = it.postings(IndexFeatures::NONE);

      if (IRS_UNLIKELY(!docs_it)) {
        IRS_ASSERT(false);
        return 0;
      }

      const auto* doc = irs::get<document>(*docs_it);

      if (IRS_UNLIKELY(!doc)) {
        IRS_ASSERT(false);
        return 0;
      }

      auto begin = docs.begin();

      for (auto end = docs.end(); begin != end && docs_it->next(); ++begin) {
        *begin = doc->value;
      }

      return std::distance(docs.begin(), begin);
    }

    size_t bit_union(const cookie_provider& provider, size_t* set) const final {
      auto term_provider = [&provider]() mutable -> const term_meta* {
        if (auto* cookie = provider()) {
          return &DownCast<::cookie>(*cookie).meta;
        }

        return nullptr;
      };

      IRS_ASSERT(owner_ != nullptr);
      IRS_ASSERT(owner_->pr_ != nullptr);
      return owner_->pr_->bit_union(meta().index_features, term_provider, set,
                                    WandCount());
    }

    seek_term_iterator::ptr iterator(
      automaton_table_matcher& matcher) const final {
      auto& acceptor = matcher.GetFst();

      const auto start = acceptor.Start();

      if (fst::kNoStateId == start) {
        return seek_term_iterator::empty();
      }

      if (!acceptor.NumArcs(start)) {
        if (acceptor.Final(start)) {
          // match all
          return this->iterator(SeekMode::NORMAL);
        }

        return seek_term_iterator::empty();
      }

      auto terms_in = owner_->terms_in_->reopen();

      if (!terms_in) {
        // implementation returned wrong pointer
        IRS_LOG_ERROR("Failed to reopen terms input");

        throw io_error{"Failed to reopen terms input"};  // FIXME
      }

      return memory::make_managed<automaton_term_iterator<FST>>(
        *this, *owner_->pr_, std::move(terms_in),
        owner_->terms_in_cipher_.get(), *fst_, matcher);
    }

    doc_iterator::ptr postings(const seek_cookie& cookie,
                               IndexFeatures features) const final {
      return owner_->pr_->iterator(meta().index_features, features,
                                   DownCast<::cookie>(cookie).meta,
                                   WandCount());
    }

    doc_iterator::ptr wanderator(const seek_cookie& cookie,
                                 IndexFeatures features,
                                 const WanderatorOptions& options,
                                 WandContext ctx) const final {
      return owner_->pr_->wanderator(
        meta().index_features, features, DownCast<::cookie>(cookie).meta,
        options, ctx,
        {.mapped_index = WandIndex(ctx.index), .count = WandCount()});
    }

   private:
    field_reader* owner_;
    std::unique_ptr<FST> fst_;
  };

  using vector_fst_reader = term_reader<vector_byte_fst>;
  using immutable_fst_reader = term_reader<immutable_byte_fst>;

  using vector_fst_readers = std::vector<term_reader<vector_byte_fst>>;
  using immutable_fst_readers = std::vector<term_reader<immutable_byte_fst>>;

  std::variant<immutable_fst_readers, vector_fst_readers> fields_;
  absl::flat_hash_map<hashed_string_view, irs::term_reader*> name_to_field_;
  irs::postings_reader::ptr pr_;
  encryption::stream::ptr terms_in_cipher_;
  index_input::ptr terms_in_;
  IResourceManager& resource_manager_;
};

field_reader::field_reader(irs::postings_reader::ptr&& pr, IResourceManager& rm)
  : pr_(std::move(pr)), resource_manager_(rm) {
  IRS_ASSERT(pr_);
}

void field_reader::prepare(const ReaderState& state) {
  IRS_ASSERT(state.dir);
  IRS_ASSERT(state.meta);

  // check index header
  index_input::ptr index_in;

  int64_t checksum = 0;
  std::string filename;
  const auto term_index_version = burst_trie::Version(prepare_input(
    filename, index_in, irs::IOAdvice::SEQUENTIAL | irs::IOAdvice::READONCE,
    state, field_writer::TERMS_INDEX_EXT, field_writer::FORMAT_TERMS_INDEX,
    static_cast<int32_t>(burst_trie::Version::MIN),
    static_cast<int32_t>(burst_trie::Version::MAX), &checksum));

  constexpr const size_t FOOTER_LEN = sizeof(uint64_t)  // fields count
                                      + format_utils::kFooterLen;

  // read total number of indexed fields
  size_t fields_count{0};
  {
    const uint64_t ptr = index_in->file_pointer();

    index_in->seek(index_in->length() - FOOTER_LEN);

    fields_count = index_in->read_long();

    // check index checksum
    format_utils::check_footer(*index_in, checksum);

    index_in->seek(ptr);
  }

  auto* enc = state.dir->attributes().encryption();
  encryption::stream::ptr index_in_cipher;

  if (term_index_version > burst_trie::Version::MIN) {
    if (irs::decrypt(filename, *index_in, enc, index_in_cipher)) {
      IRS_ASSERT(index_in_cipher && index_in_cipher->block_size());

      const auto blocks_in_buffer = math::div_ceil64(
        DEFAULT_ENCRYPTION_BUFFER_SIZE, index_in_cipher->block_size());

      index_in = std::make_unique<encrypted_input>(
        std::move(index_in), *index_in_cipher, blocks_in_buffer, FOOTER_LEN);
    }
  }

  feature_map_t feature_map;
  IndexFeatures features{IndexFeatures::NONE};
  if (IRS_LIKELY(term_index_version >= burst_trie::Version::IMMUTABLE_FST)) {
    read_segment_features(*index_in, features, feature_map);
  } else {
    read_segment_features_legacy(*index_in, features, feature_map);
  }

  // read terms for each indexed field
  if (term_index_version <= burst_trie::Version::ENCRYPTION_MIN) {
    fields_ = vector_fst_readers{};
  }

  const auto& meta = *state.meta;

  std::visit(
    [&](auto& fields) {
      fields.reserve(fields_count);
      name_to_field_.reserve(fields_count);

      for (std::string_view previous_field_name{""}; fields_count;
           --fields_count) {
        auto& field = fields.emplace_back(*this);
        field.prepare(term_index_version, *index_in, feature_map);

        const auto& name = field.meta().name;

        // ensure that fields are sorted properly
        if (previous_field_name > name) {
          throw index_error{
            absl::StrCat("Invalid field order in segment '", meta.name, "'")};
        }

        const auto res =
          name_to_field_.emplace(hashed_string_view{name}, &field);

        if (!res.second) {
          throw index_error{absl::StrCat("Duplicated field: '", meta.name,
                                         "' found in segment: '", name, "'")};
        }

        previous_field_name = name;
      }
    },
    fields_);

  //-----------------------------------------------------------------
  // prepare terms input
  //-----------------------------------------------------------------

  // check term header
  const auto term_dict_version = burst_trie::Version(prepare_input(
    filename, terms_in_, irs::IOAdvice::RANDOM, state, field_writer::TERMS_EXT,
    field_writer::FORMAT_TERMS, static_cast<int32_t>(burst_trie::Version::MIN),
    static_cast<int32_t>(burst_trie::Version::MAX)));

  if (term_index_version != term_dict_version) {
    throw index_error(absl::StrCat("Term index version '", term_index_version,
                                   "' mismatches term dictionary version '",
                                   term_dict_version, "' in segment '",
                                   meta.name, "'"));
  }

  if (term_dict_version > burst_trie::Version::MIN) {
    if (irs::decrypt(filename, *terms_in_, enc, terms_in_cipher_)) {
      IRS_ASSERT(terms_in_cipher_ && terms_in_cipher_->block_size());
    }
  }

  // prepare postings reader
  pr_->prepare(*terms_in_, state, features);

  // Since terms dictionary are too large
  // it is too expensive to verify checksum of
  // the entire file. Here we perform cheap
  // error detection which could recognize
  // some forms of corruption.
  format_utils::read_checksum(*terms_in_);
}

const irs::term_reader* field_reader::field(std::string_view field) const {
  auto it = name_to_field_.find(hashed_string_view{field});
  return it == name_to_field_.end() ? nullptr : it->second;
}

irs::field_iterator::ptr field_reader::iterator() const {
  struct less {
    bool operator()(const irs::term_reader& lhs,
                    std::string_view rhs) const noexcept {
      return lhs.meta().name < rhs;
    }
  };

  return std::visit(
    [](const auto& fields) -> irs::field_iterator::ptr {
      using reader_type =
        typename std::remove_reference_t<decltype(fields)>::value_type;

      using iterator_t =
        iterator_adaptor<std::string_view, reader_type, decltype(fields.data()),
                         irs::field_iterator, less>;

      return memory::make_managed<iterator_t>(fields.data(),
                                              fields.data() + fields.size());
    },
    fields_);
}

/*

// Implements generalized visitation logic for term dictionary
template<typename FST>
class term_reader_visitor {
 public:
  explicit term_reader_visitor(const FST& field, index_input& terms_in,
                               encryption::stream* terms_in_cipher)
    : fst_(&field),
      terms_in_(terms_in.reopen()),
      terms_in_cipher_(terms_in_cipher) {}

  template<typename Visitor>
  void operator()(Visitor& visitor) {
    block_iterator* cur_block = push_block(fst_->Final(fst_->Start()), 0);
    cur_block->load(*terms_in_, terms_in_cipher_);
    visitor.push_block(term_, *cur_block);

    auto copy_suffix = [&cur_block, this](const byte_type* suffix,
                                          size_t suffix_size) {
      copy(suffix, cur_block->prefix(), suffix_size);
    };

    while (true) {
      while (cur_block->done()) {
        if (cur_block->next_sub_block<false>()) {
          cur_block->load(*terms_in_, terms_in_cipher_);
          visitor.sub_block(*cur_block);
        } else if (&block_stack_.front() == cur_block) {  // root
          cur_block->reset();
          return;
        } else {
          visitor.pop_block(*cur_block);
          cur_block = pop_block();
        }
      }

      while (!cur_block->done()) {
        cur_block->next(copy_suffix);
        if (ET_TERM == cur_block->type()) {
          visitor.push_term(term_);
        } else {
          IRS_ASSERT(ET_BLOCK == cur_block->type());
          cur_block = push_block(cur_block->block_start(), term_.size());
          cur_block->load(*terms_in_, terms_in_cipher_);
          visitor.push_block(term_, *cur_block);
        }
      }
    }
  }

 private:
  void copy(const byte_type* suffix, size_t prefix_size, size_t suffix_size) {
    absl::strings_internal::STLStringResizeUninitializedAmortized(
      &term_, prefix_size + suffix_size);
    std::memcpy(term_.data() + prefix_size, suffix, suffix_size);
  }

  block_iterator* pop_block() noexcept {
    block_stack_.pop_back();
    IRS_ASSERT(!block_stack_.empty());
    return &block_stack_.back();
  }

  block_iterator* push_block(byte_weight&& out, size_t prefix) {
    // ensure final weight correctess
    IRS_ASSERT(out.Size() >= MIN_WEIGHT_SIZE);

    block_stack_.emplace_back(std::move(out), prefix);
    return &block_stack_.back();
  }

  block_iterator* push_block(uint64_t start, size_t prefix) {
    block_stack_.emplace_back(start, prefix);
    return &block_stack_.back();
  }

  const FST* fst_;
  std::deque<block_iterator> block_stack_;
  bstring term_;
  index_input::ptr terms_in_;
  encryption::stream* terms_in_cipher_;
};

// "Dumper" visitor for term_reader_visitor. Suitable for debugging needs.
class dumper : util::noncopyable {
 public:
  explicit dumper(std::ostream& out) : out_(out) {}

  void push_term(bytes_view term) {
    indent();
    out_ << "TERM|" << suffix(term) << "\n";
  }

  void push_block(bytes_view term, const block_iterator& block) {
    indent();
    out_ << "BLOCK|" << block.size() << "|" << suffix(term) << "\n";
    indent_ += 2;
    prefix_ = block.prefix();
  }

  void sub_block(const block_iterator&) {
    indent();
    out_ << "|\n";
    indent();
    out_ << "V\n";
  }

  void pop_block(const block_iterator& block) {
    indent_ -= 2;
    prefix_ -= block.prefix();
  }

 private:
  void indent() {
    for (size_t i = 0; i < indent_; ++i) {
      out_ << " ";
    }
  }

  std::string_view suffix(bytes_view term) {
    return ViewCast<char>(
      bytes_view{term.data() + prefix_, term.size() - prefix_});
  }

  std::ostream& out_;
  size_t indent_ = 0;
  size_t prefix_ = 0;
};
*/

}  // namespace

namespace irs {
namespace burst_trie {

irs::field_writer::ptr make_writer(Version version,
                                   irs::postings_writer::ptr&& writer,
                                   IResourceManager& rm, bool consolidation) {
  // Here we can parametrize field_writer via version20::term_meta
  return std::make_unique<::field_writer>(std::move(writer), consolidation, rm,
                                          version);
}

irs::field_reader::ptr make_reader(irs::postings_reader::ptr&& reader,
                                   IResourceManager& rm) {
  return std::make_shared<::field_reader>(std::move(reader), rm);
}

}  // namespace burst_trie
}  // namespace irs
