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

#include "shared.hpp"
#include "skip_list.hpp"

#include "store/store_utils.hpp"

#include "index/iterators.hpp"

#include "utils/math_utils.hpp"
#include "utils/std.hpp"

namespace {

// returns maximum number of skip levels needed to store specified
// count of objects for skip list with
// step skip_0 for 0 level, step skip_n for other levels
inline size_t max_levels(size_t skip_0, size_t skip_n, size_t count) {
  size_t levels = 0;
  if (skip_0 < count) {
    levels = 1 + irs::math::log(count/skip_0, skip_n);
  }
  return levels;
}

const size_t UNDEFINED = irs::integer_traits<size_t>::const_max;

} // LOCAL

namespace iresearch {

// ----------------------------------------------------------------------------
// --SECTION--                                       skip_writer implementation
// ----------------------------------------------------------------------------

skip_writer::skip_writer(size_t skip_0, size_t skip_n) noexcept
  : skip_0_(skip_0), skip_n_(skip_n) {
}

void skip_writer::prepare(
    size_t max_levels, 
    size_t count,
    const skip_writer::write_f& write, /* = nop */
    const memory_allocator& alloc /* = memory_allocator::global() */
) {
  max_levels = std::max(size_t(1), max_levels);
  max_levels = std::min(max_levels, ::max_levels(skip_0_, skip_n_, count));
  levels_.reserve(max_levels);
  max_levels = std::max(max_levels, levels_.capacity());

  // reset existing skip levels
  for (auto& level : levels_) {
    level.reset(alloc);
  }

  // add new skip levels
  for (auto size = levels_.size(); size < max_levels; ++size) {
    levels_.emplace_back(alloc);
  }

  write_ = write;
}

void skip_writer::skip(size_t count) {
  assert(!levels_.empty());

  if (0 != count % skip_0_) {
    return;
  }

  uint64_t child = 0;

  // write 0 level
  {
    auto& stream = levels_.front().stream;
    write_(0, stream);
    count /= skip_0_;
    child = stream.file_pointer();
  }

  // write levels from 1 to n
  size_t num = 0;
  for (auto level = levels_.begin()+1, end = levels_.end();
       0 == count % skip_n_ && level != end;
       ++level, count /= skip_n_) {
    auto& stream = level->stream;
    write_(++num, stream);

    uint64_t next_child = stream.file_pointer();
    stream.write_vlong(child);
    child = next_child;
  }
}

void skip_writer::flush(index_output& out) {
  const auto rend = levels_.rend();

  // find first filled level
  auto level = std::find_if(
    levels_.rbegin(), rend,
    [](const memory_output& level) {
      return level.stream.file_pointer();
  });

  // write number of levels
  out.write_vint(uint32_t(std::distance(level, rend)));

  // write levels from n downto 0
  std::for_each(
    level, rend,
    [&out](memory_output& level) {
      auto& stream = level.stream;
      stream.flush(); // update length of each buffer

      const uint64_t length = stream.file_pointer();
      assert(length);
      out.write_vlong(length);
      stream >> out;
  });
}

void skip_writer::reset() noexcept {
  for(auto& level : levels_) {
    level.stream.reset();
  }
}

// ----------------------------------------------------------------------------
// --SECTION--                                       skip_reader implementation
// ----------------------------------------------------------------------------

skip_reader::level::level(
    index_input::ptr&& stream,
    size_t step,
    uint64_t begin, 
    uint64_t end,
    uint64_t child /*= 0*/,
    size_t skipped /*= 0*/,
    doc_id_t doc /*= doc_limits::invalid()*/
) noexcept
  : stream(std::move(stream)), // thread-safe input
    begin(begin), 
    end(end),
    child(child),
    step(step),
    skipped(skipped),
    doc(doc) {
}

index_input::ptr skip_reader::level::dup() const {
  auto clone = stream->dup(); // non thread-safe clone

  if (!clone) {
    // implementation returned wrong pointer
    IR_FRMT_ERROR("Failed to duplicate document input in: %s", __FUNCTION__);

    throw io_error("failed to duplicate document input");
  }

  return memory::make_unique<skip_reader::level>(
    std::move(clone), step, begin, end, child, skipped, doc);
}

byte_type skip_reader::level::read_byte() {
  return stream->read_byte();
}

size_t skip_reader::level::read_bytes(byte_type* b, size_t count) {
  static_assert(sizeof(size_t) >= sizeof(uint64_t), "sizeof(size_t) < sizeof(uint64_t)");
  return stream->read_bytes(b, std::min(size_t(end) - file_pointer(), count));
}

index_input::ptr skip_reader::level::reopen() const {
  auto clone = stream->reopen(); // tread-safe clone

  if (!clone) {
    // implementation returned wrong pointer
    IR_FRMT_ERROR("Failed to reopen document input in: %s", __FUNCTION__);

    throw io_error("failed to reopen document input");
  }

  return memory::make_unique<skip_reader::level>(
    std::move(clone), step, begin, end, child, skipped, doc);
}

size_t skip_reader::level::file_pointer() const {
  return stream->file_pointer() - begin;
}

size_t skip_reader::level::length() const {
  return end - begin;
}

bool skip_reader::level::eof() const {
  return stream->file_pointer() >= end;
}

void skip_reader::level::seek(size_t pos) {
  return stream->seek(begin + pos);
}

int64_t skip_reader::level::checksum(size_t offset) const {
  static_assert(sizeof(decltype(end)) == sizeof(size_t), "sizeof(decltype(end)) != sizeof(size_t)");
  return stream->checksum(
    std::min(offset, size_t(end) - stream->file_pointer())
  );
}

skip_reader::skip_reader(
    size_t skip_0, 
    size_t skip_n) noexcept
  : skip_0_(skip_0), skip_n_(skip_n) {
}

void skip_reader::read_skip(skip_reader::level& level) {
  // read_ should return NO_MORE_DOCS when stream is exhausted
  const auto doc = read_(size_t(std::distance(&level, &levels_.back())), level);

  // read pointer to child level if needed
  if (!doc_limits::eof(doc) && level.child != UNDEFINED) {
    level.child = level.stream->read_vlong();
  }

  level.doc = doc;
  level.skipped += level.step;
}

/* static */ void skip_reader::seek_skip(
    skip_reader::level& level, 
    uint64_t ptr,
    size_t skipped) {
  auto &stream = *level.stream;
  const auto absolute_ptr = level.begin + ptr;
  if (absolute_ptr > stream.file_pointer()) {
    stream.seek(absolute_ptr);
    level.skipped = skipped;
    if (level.child != UNDEFINED) {
      level.child = stream.read_vlong();
    }
  }
}

// returns highest level with the value not less than target 
skip_reader::levels_t::iterator skip_reader::find_level(doc_id_t target) {
  assert(std::is_sorted(
    levels_.rbegin(), levels_.rend(),
    [](level& lhs, level& rhs) { return lhs.doc < rhs.doc; }
  ));

  auto level = std::upper_bound(
    levels_.rbegin(),
    levels_.rend(),
    target,
    [](doc_id_t target, const skip_reader::level& level) {
      return target < level.doc;
  });

  if (level == levels_.rend()) {
    return levels_.begin(); // the highest
  }

  // check if we have already found the lowest possible level
  if (level != levels_.rbegin()) {
    --level;
  }

  // convert reverse iterator to forward
  return irstd::to_forward(level);
}

size_t skip_reader::seek(doc_id_t target) {
  assert(!levels_.empty());

  auto level = find_level(target); // the highest level for the specified target
  uint64_t child = 0; // pointer to child skip
  size_t skipped = 0; // number of skipped documents

  std::for_each(
    level, levels_.end(),
    [this, &child, &skipped, &target](skip_reader::level& level) {
      if (level.doc < target) {
        // seek to child
        seek_skip(level, child, skipped);

        // seek to skip
        child = level.child;
        read_skip(level);

        for (; level.doc < target; read_skip(level)) {
          child = level.child;
        }

        skipped = level.skipped - level.step;
      }
  });

  skipped = levels_.back().skipped;
  return skipped ? skipped - skip_0_ : 0;
}

void skip_reader::reset() {
  static auto reset = [](skip_reader::level& level) {
    level.stream->seek(level.begin);
    if (level.child != UNDEFINED) {
      level.child = 0;
    }
    level.skipped = 0;
    level.doc = doc_limits::invalid();
  };

  std::for_each(levels_.begin(), levels_.end(), reset);
}

void skip_reader::load_level(levels_t& levels, index_input::ptr&& stream, size_t step) {
  assert(stream);

  // read level length
  const auto length = stream->read_vlong();

  if (!length) {
    throw index_error("while loading level, error: zero length");
  }

  const auto begin = stream->file_pointer();
  const auto end = begin + length;

  levels.emplace_back(std::move(stream), step, begin, end); // load level
}

void skip_reader::prepare(index_input::ptr&& in, const read_f& read /* = nop */) {
  assert(in && read);

  // read number of levels in a skip-list
  size_t max_levels = in->read_vint();

  if (max_levels) {
    std::vector<level> levels;
    levels.reserve(max_levels);

    size_t step = skip_0_ * size_t(pow(skip_n_, --max_levels)); // skip step of the level

    // load levels from n down to 1
    for (; max_levels; --max_levels) {
      load_level(levels, in->dup(), step);

      // seek to the next level
      in->seek(levels.back().end);

      step /= skip_n_;
    }

    // load 0 level
    load_level(levels, std::move(in), skip_0_);
    levels.back().child = UNDEFINED;

    levels_ = std::move(levels);
  }

  read_ = read;
}

}
