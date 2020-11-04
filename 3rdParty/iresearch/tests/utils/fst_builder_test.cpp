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

#ifndef IRESEARCH_DLL

#include "tests_shared.hpp"

#include <fstream>

#include "index/directory_reader.hpp"
#include "index/index_writer.hpp"
#include "store/mmap_directory.hpp"
#include "store/memory_directory.hpp"
#include "utils/fstext/fst_string_weight.h"
#include "utils/fstext/fst_string_ref_weight.h"
#include "utils/fstext/fst_builder.hpp"
#include "utils/fstext/fst_matcher.hpp"
#include "utils/fstext/immutable_fst.h"
#include "utils/fstext/fst_utils.hpp"
#include "utils/fstext/fst_decl.hpp"
#include "utils/numeric_utils.hpp"

#include <fst/vector-fst.h>
#include <fst/matcher.h>

namespace {

struct fst_stats : iresearch::fst_stats {
  size_t total_weight_size{};

  void operator()(const irs::vector_byte_fst::Weight& w) noexcept {
    total_weight_size += w.Size();
  }

  [[maybe_unused]] bool operator==(const fst_stats& rhs) const noexcept {
    return num_states == rhs.num_states &&
           num_arcs == rhs.num_arcs &&
           total_weight_size == rhs.total_weight_size;
  }
};

using fst_byte_builder = irs::fst_builder<irs::byte_type, irs::vector_byte_fst, fst_stats>;

// reads input data to build fst
// first - prefix
// second - payload
std::vector<std::pair<irs::bstring, irs::bstring>> read_fst_input(
    const std::string& filename) {
  auto read_size = [](std::istream& stream) {
    size_t size;
    stream.read(reinterpret_cast<char*>(&size), sizeof(size_t));
    if (irs::numeric_utils::is_big_endian()) {
      size = irs::numeric_utils::numeric_traits<size_t>::hton(size);
    }
    return size;
  };

  std::vector<std::pair<irs::bstring, irs::bstring>> data;

  std::ifstream in;
  in.open(filename, std::ios_base::in | std::ios_base::binary);

  data.resize(read_size(in));

  for (size_t i = 0; i < data.size(); ++i) {
    auto& entry = data[i];
    auto& prefix = entry.first;
    auto& payload = entry.second;

    prefix.resize(read_size(in));
    in.read(reinterpret_cast<char*>(&prefix[0]), prefix.size());

    payload.resize(read_size(in));
    in.read(reinterpret_cast<char*>(&payload[0]), payload.size());
  }

  return data;
}

TEST(fst_builder_test, static_const) {
  ASSERT_EQ(0, fst_byte_builder::stateid_t(fst_byte_builder::final));
}

TEST(fst_builder_test, build_fst) {
  auto expected_data = read_fst_input(test_base::resource("fst"));
  ASSERT_FALSE(expected_data.empty());

  ASSERT_TRUE(
    std::is_sorted(
      expected_data.begin(), expected_data.end(),
      [](const std::pair<irs::bstring, irs::bstring>& lhs,
         const std::pair<irs::bstring, irs::bstring>& rhs) {
        return lhs.first < rhs.first;
    })
  );

  irs::vector_byte_fst fst;
  fst_stats stats;

  // build fst
  {
    fst_byte_builder builder(fst);
    builder.reset();

    for (auto& data : expected_data) {
      builder.add(data.first, irs::byte_weight(data.second.begin(), data.second.end()));
    }

    stats = builder.finish();
  }

  fst_stats expected_stats;
  for (fst::StateIterator<irs::vector_byte_fst> states(fst); !states.Done(); states.Next()) {
    const auto stateid = states.Value();
    ++expected_stats.num_states;
    expected_stats.num_arcs += fst.NumArcs(stateid);
    expected_stats(fst.Final(stateid));
    for (fst::ArcIterator<irs::vector_byte_fst> arcs(fst, stateid); !arcs.Done(); arcs.Next()) {
      expected_stats(arcs.Value().weight);
    }
  }
  ASSERT_EQ(expected_stats, stats);

  // check fst
  {
    typedef fst::SortedMatcher<irs::vector_byte_fst> sorted_matcher_t;
    typedef fst::explicit_matcher<sorted_matcher_t> matcher_t; // avoid implicit loops

    ASSERT_EQ(fst::kILabelSorted, fst.Properties(fst::kILabelSorted, true));
    ASSERT_TRUE(fst.Final(fst_byte_builder::final).Empty());

    for (auto& data : expected_data) {
      irs::byte_weight actual_weight;

      auto state = fst.Start(); // root node

      matcher_t matcher(fst, fst::MATCH_INPUT);
      for (irs::byte_type c : data.first) {
        matcher.SetState(state);
        ASSERT_TRUE(matcher.Find(c));

        const auto& arc = matcher.Value();
        ASSERT_EQ(c, arc.ilabel);
        actual_weight.PushBack(arc.weight);
        state = arc.nextstate;
      }

      actual_weight = fst::Times(actual_weight, fst.Final(state));

      ASSERT_EQ(irs::bytes_ref(actual_weight), irs::bytes_ref(data.second));
    }
  }
}

TEST(immutable_fst_test, read_write) {
  auto expected_data = read_fst_input(test_base::resource("fst"));
  ASSERT_FALSE(expected_data.empty());

  irs::vector_byte_fst fst;
  fst_stats stats;

  // build fst
  {
    fst_byte_builder builder(fst);
    builder.reset();

    for (auto& data : expected_data) {
      builder.add(data.first, irs::byte_weight(data.second.begin(), data.second.end()));
    }

    stats = builder.finish();
  }

  fst_stats expected_stats;
  for (fst::StateIterator<irs::vector_byte_fst> states(fst); !states.Done(); states.Next()) {
    const auto stateid = states.Value();
    ++expected_stats.num_states;
    expected_stats.num_arcs += fst.NumArcs(stateid);
    expected_stats(fst.Final(stateid));
    for (fst::ArcIterator<irs::vector_byte_fst> arcs(fst, stateid); !arcs.Done(); arcs.Next()) {
      expected_stats(arcs.Value().weight);
    }
  }
  ASSERT_EQ(expected_stats, stats);

  irs::memory_output out(irs::memory_allocator::global());
  irs::immutable_byte_fst::Write(fst, out.stream, stats);
  out.stream.flush();

  irs::memory_index_input in(out.file);
  std::unique_ptr<irs::immutable_byte_fst> read_fst(irs::immutable_byte_fst::Read(in));

  ASSERT_NE(nullptr, read_fst);
  ASSERT_EQ(fst::kExpanded, read_fst->Properties(fst::kExpanded, false));
  ASSERT_EQ(fst.NumStates(), read_fst->NumStates());
  ASSERT_EQ(fst.Start(), read_fst->Start());
  for (fst::StateIterator<decltype(fst)> it(fst); !it.Done(); it.Next()) {
    const auto s = it.Value();
    ASSERT_EQ(fst.NumArcs(s), read_fst->NumArcs(s));
    ASSERT_EQ(0, read_fst->NumInputEpsilons(s));
    ASSERT_EQ(0, read_fst->NumOutputEpsilons(s));
    ASSERT_EQ(static_cast<irs::bytes_ref>(fst.Final(s)),
              static_cast<irs::bytes_ref>(read_fst->Final(s)));

    fst::ArcIterator<decltype(fst)> expected_arcs(fst, s);
    fst::ArcIterator<irs::immutable_byte_fst> actual_arcs(*read_fst, s);
    for (; !expected_arcs.Done(); expected_arcs.Next(), actual_arcs.Next()) {
      auto& expected_arc = expected_arcs.Value();
      auto& actual_arc = actual_arcs.Value();
      ASSERT_EQ(expected_arc.ilabel, actual_arc.ilabel);
      ASSERT_EQ(expected_arc.nextstate, actual_arc.nextstate);
      ASSERT_EQ(static_cast<irs::bytes_ref>(expected_arc.weight),
                static_cast<irs::bytes_ref>(actual_arc.weight));
    }
  }

  // check fst
  {
    using sorted_matcher_t = fst::SortedMatcher<irs::immutable_byte_fst>;
    using matcher_t = fst::explicit_matcher<sorted_matcher_t>; // avoid implicit loops

    ASSERT_EQ(fst::kILabelSorted, fst.Properties(fst::kILabelSorted, true));
    ASSERT_TRUE(fst.Final(fst_byte_builder::final).Empty());

    for (auto& data : expected_data) {
      irs::byte_weight actual_weight;

      auto state = fst.Start(); // root node

      matcher_t matcher(*read_fst, fst::MATCH_INPUT);
      for (irs::byte_type c : data.first) {
        matcher.SetState(state);
        ASSERT_TRUE(matcher.Find(c));

        const auto& arc = matcher.Value();
        ASSERT_EQ(c, arc.ilabel);
        actual_weight.PushBack(arc.weight.begin(), arc.weight.end());
        state = arc.nextstate;
      }

      actual_weight = fst::Times(actual_weight, fst.Final(state));

      ASSERT_EQ(irs::bytes_ref(actual_weight), irs::bytes_ref(data.second));
    }
  }
}

}

#endif // IRESEARCH_DLL
