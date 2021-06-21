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

#include "tests_shared.hpp"

#include "store/memory_directory.hpp"
#include "store/store_utils.hpp"
#include "utils/automaton_utils.hpp"
#include "utils/levenshtein_utils.hpp"
#include "utils/fstext/fst_table_matcher.hpp"
#include "utils/utf8_utils.hpp"

using namespace irs::literals;

namespace {

void assert_description(
    const irs::parametric_description& description,
    const irs::bytes_ref& target,
    const std::vector<std::tuple<irs::bytes_ref, size_t, size_t, size_t>>& candidates) {
  auto a = irs::make_levenshtein_automaton(description, target);

  // ensure only invalid state has no outbound connections
  ASSERT_GE(a.NumStates(), 1);
  ASSERT_EQ(0, a.NumArcs(0));
  for (irs::automaton::StateId state = 1; state < a.NumStates(); ++state) {
    ASSERT_GT(a.NumArcs(state), 0);
  }

  for (auto& entry : candidates) {
    const auto candidate = std::get<0>(entry);
    const size_t expected_distance = std::get<1>(entry);
    const size_t expected_distance_automaton = std::get<2>(entry);
    const size_t expected_distance_precise = std::get<3>(entry);
    SCOPED_TRACE(testing::Message("Target: '") << irs::ref_cast<char>(target) <<
                 "', Candidate: '" << irs::ref_cast<char>(candidate) <<
                 "' , Distance: " << expected_distance <<
                 " , Precise distance: " << expected_distance_precise);

    std::vector<uint32_t> utf8_target, utf8_candidate;
    irs::utf8_utils::utf8_to_utf32<true>(target, std::back_inserter(utf8_target));
    irs::utf8_utils::utf8_to_utf32<true>(candidate, std::back_inserter(utf8_candidate));
    const irs::basic_string_ref<uint32_t> utf8_target_ref(utf8_target.data(), utf8_target.size());
    const irs::basic_string_ref<uint32_t> utf8_candidate_ref(utf8_candidate.data(), utf8_candidate.size());

    ASSERT_EQ(expected_distance_precise, irs::edit_distance(utf8_candidate_ref, utf8_target_ref));
    ASSERT_EQ(expected_distance_precise, irs::edit_distance(utf8_target_ref, utf8_candidate_ref));

    {
      size_t actual_distance;
      if (irs::edit_distance(actual_distance, description, candidate, target)) {
        ASSERT_EQ(expected_distance, actual_distance);
        ASSERT_EQ(expected_distance, irs::edit_distance(description, candidate, target));
      }
    }

    {
      size_t actual_distance;
      if (irs::edit_distance(actual_distance, description, target, candidate)) {
        ASSERT_EQ(expected_distance, actual_distance);
        ASSERT_EQ(expected_distance, irs::edit_distance(description, target, candidate));
      }
    }

    const auto state = irs::accept(a, candidate);
    ASSERT_EQ(expected_distance_automaton <= description.max_distance(), bool(state));
    if (state) {
      // every final state contains valid edit distance
      ASSERT_EQ(expected_distance_automaton, state.Payload());
    }
  }
}

void assert_read_write(const irs::parametric_description& description) {
  irs::bstring buf;
  {
    irs::bytes_output out(buf);
    irs::write(description, static_cast<data_output&>(out));
    ASSERT_FALSE(buf.empty());
  }
  {
    irs::bytes_ref_input in(buf);
    ASSERT_EQ(description, irs::read(in));
  }
}

}

TEST(levenshtein_utils_test, test_distance) {
  {
    const irs::string_ref lhs = "aec";
    const irs::string_ref rhs = "abcd";

    ASSERT_EQ(2, irs::edit_distance(lhs.c_str(), lhs.size(), rhs.c_str(), rhs.size()));
    ASSERT_EQ(2, irs::edit_distance(rhs.c_str(), rhs.size(), lhs.c_str(), lhs.size()));
  }

  {
    const irs::string_ref lhs = "elephant";
    const irs::string_ref rhs = "relevant";

    ASSERT_EQ(3, irs::edit_distance(lhs.c_str(), lhs.size(), rhs.c_str(), rhs.size()));
    ASSERT_EQ(3, irs::edit_distance(rhs.c_str(), rhs.size(), lhs.c_str(), lhs.size()));
  }

  {
    const irs::string_ref lhs = "\xD0\xBF\xD1\x83\xD1\x82\xD0\xB8\xD0\xBD";
    const irs::string_ref rhs = "\xD1\x85\xD1\x83\xD0\xB9\xD0\xBB\xD0\xBE";

    std::vector<uint32_t> lhs_utf8, rhs_utf8;
    irs::utf8_utils::utf8_to_utf32<false>(irs::ref_cast<irs::byte_type>(lhs), std::back_inserter(lhs_utf8));
    irs::utf8_utils::utf8_to_utf32<false>(irs::ref_cast<irs::byte_type>(rhs), std::back_inserter(rhs_utf8));

    ASSERT_EQ(4, irs::edit_distance(lhs_utf8.data(), lhs_utf8.size(), rhs_utf8.data(), rhs_utf8.size()));
    ASSERT_EQ(4, irs::edit_distance(rhs_utf8.data(), rhs_utf8.size(), lhs_utf8.data(), lhs_utf8.size()));
  }

  {
    const irs::string_ref lhs = "";
    const irs::string_ref rhs = "aec";

    ASSERT_EQ(3, irs::edit_distance(lhs.c_str(), lhs.size(), rhs.c_str(), rhs.size()));
    ASSERT_EQ(3, irs::edit_distance(rhs.c_str(), rhs.size(), lhs.c_str(), lhs.size()));
  }

  {
    const irs::string_ref lhs = "";
    const irs::string_ref rhs = "";

    ASSERT_EQ(0, irs::edit_distance(lhs.c_str(), lhs.size(), rhs.c_str(), rhs.size()));
    ASSERT_EQ(0, irs::edit_distance(rhs.c_str(), rhs.size(), lhs.c_str(), lhs.size()));
  }

  {
    const irs::string_ref lhs;
    const irs::string_ref rhs;

    ASSERT_EQ(0, irs::edit_distance(lhs.c_str(), lhs.size(), rhs.c_str(), rhs.size()));
    ASSERT_EQ(0, irs::edit_distance(rhs.c_str(), rhs.size(), lhs.c_str(), lhs.size()));
  }
}

TEST(levenshtein_utils_test, test_static_const) {
  ASSERT_EQ(31, decltype(irs::parametric_description::MAX_DISTANCE)(irs::parametric_description::MAX_DISTANCE));
}

TEST(levenshtein_utils_test, test_description_0) {
  auto assert_distance = [](const irs::parametric_description& d) {
    // state 0
    ASSERT_EQ(1, d.distance(0, 0));
    // state 1
    ASSERT_EQ(0, d.distance(1, 0));
  };

  auto assert_transitions = [](const irs::parametric_description& d) {
    // state 0
    ASSERT_EQ(irs::parametric_description::transition_t(0, 0), d.transition(0, 0));
    ASSERT_EQ(irs::parametric_description::transition_t(0, 0), d.transition(0, 1));
    // state 1
    ASSERT_EQ(irs::parametric_description::transition_t(0, 0), d.transition(1, 0));
    ASSERT_EQ(irs::parametric_description::transition_t(1, 1), d.transition(1, 1));
  };

  // no transpositions
  {
    auto description = irs::make_parametric_description(0, false);
    ASSERT_TRUE(bool(description));
    ASSERT_EQ(2, description.size());
    ASSERT_EQ(1, description.chi_size());
    ASSERT_EQ(2, description.chi_max());
    ASSERT_EQ(0, description.max_distance());
    ASSERT_EQ(description, irs::make_parametric_description(0, true));
    ASSERT_EQ(description, irs::make_parametric_description(0, false));
    assert_distance(description);
    assert_transitions(description);
    assert_read_write(description);

    assert_description(
      description,
      irs::ref_cast<irs::byte_type>("alphabet"_sr),
      {
        { irs::ref_cast<irs::byte_type>("alphabet"_sr),  0, 0, 0 },
        { irs::ref_cast<irs::byte_type>("alphabez"_sr),  1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("alphaet"_sr),   1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("alhpaebt"_sr),  1, 1, 4 },
        { irs::ref_cast<irs::byte_type>("alphaebt"_sr),  1, 1, 2 },
        { irs::ref_cast<irs::byte_type>("alphabezz"_sr), 1, 1, 2 },
        { irs::ref_cast<irs::byte_type>("alphazez"_sr),  1, 1, 2 },
        { irs::ref_cast<irs::byte_type>("lphazez"_sr),   1, 1, 3 },
        { irs::ref_cast<irs::byte_type>("\x61\x6c\x70\x68\xD1\xFE\x62\x65\x74"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\x6c\x70\x68\xD1\xFE\x62\x65\x74"_sr), 1, 1, 2 },
        { irs::ref_cast<irs::byte_type>("\x61\x6c\x70\xE2\xFF\xFF\xD1\xFE\x62\x65\x74"_sr), 1, 1, 2 },
      }
    );
  }

  // transpositions
  {
    auto description = irs::make_parametric_description(0, true);
    ASSERT_TRUE(bool(description));
    ASSERT_EQ(2, description.size());
    ASSERT_EQ(1, description.chi_size());
    ASSERT_EQ(2, description.chi_max());
    ASSERT_EQ(0, description.max_distance());
    ASSERT_EQ(description, irs::make_parametric_description(0, true));
    ASSERT_EQ(description, irs::make_parametric_description(0, false));
    assert_distance(description);
    assert_transitions(description);
    assert_read_write(description);

    assert_description(
      description,
      irs::ref_cast<irs::byte_type>("alphabet"_sr),
      {
        { irs::ref_cast<irs::byte_type>("alphabet"_sr),  0, 0, 0 },
        { irs::ref_cast<irs::byte_type>("alphabez"_sr),  1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("alphaet"_sr),   1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("alhpaebt"_sr),  1, 1, 4 },
        { irs::ref_cast<irs::byte_type>("alphaebt"_sr),  1, 1, 2 },
        { irs::ref_cast<irs::byte_type>("alphabezz"_sr), 1, 1, 2 },
        { irs::ref_cast<irs::byte_type>("alphazez"_sr),  1, 1, 2 },
        { irs::ref_cast<irs::byte_type>("lphazez"_sr),   1, 1, 3 },
      }
    );
  }
}

TEST(levenshtein_utils_test, test_description_1) {
  // no transpositions
  {
    auto assert_distance = [](const irs::parametric_description& d) {
      // state 0
      ASSERT_EQ(2, d.distance(0, 0));
      ASSERT_EQ(2, d.distance(0, 1));
      ASSERT_EQ(2, d.distance(0, 2));
      // state 1
      ASSERT_EQ(0, d.distance(1, 0));
      ASSERT_EQ(1, d.distance(1, 1));
      ASSERT_EQ(2, d.distance(1, 2));
      // state 2
      ASSERT_EQ(1, d.distance(2, 0));
      ASSERT_EQ(1, d.distance(2, 1));
      ASSERT_EQ(2, d.distance(2, 2));
      // state 3
      ASSERT_EQ(1, d.distance(3, 0));
      ASSERT_EQ(1, d.distance(3, 1));
      ASSERT_EQ(1, d.distance(3, 2));
      // state 4
      ASSERT_EQ(1, d.distance(4, 0));
      ASSERT_EQ(2, d.distance(4, 1));
      ASSERT_EQ(2, d.distance(4, 2));
      // state 5
      ASSERT_EQ(1, d.distance(5, 0));
      ASSERT_EQ(2, d.distance(5, 1));
      ASSERT_EQ(1, d.distance(5, 2));
    };

    auto assert_transitions = [](const irs::parametric_description& d) {
      // state 0
      ASSERT_EQ(irs::parametric_description::transition_t(0, 0), d.transition(0, 0));
      ASSERT_EQ(irs::parametric_description::transition_t(0, 0), d.transition(0, 1));
      ASSERT_EQ(irs::parametric_description::transition_t(0, 0), d.transition(0, 2));
      ASSERT_EQ(irs::parametric_description::transition_t(0, 0), d.transition(0, 3));
      ASSERT_EQ(irs::parametric_description::transition_t(0, 0), d.transition(0, 4));
      ASSERT_EQ(irs::parametric_description::transition_t(0, 0), d.transition(0, 5));
      ASSERT_EQ(irs::parametric_description::transition_t(0, 0), d.transition(0, 6));
      ASSERT_EQ(irs::parametric_description::transition_t(0, 0), d.transition(0, 7));
      // state 1
      ASSERT_EQ(irs::parametric_description::transition_t(2, 0), d.transition(1, 0));
      ASSERT_EQ(irs::parametric_description::transition_t(1, 1), d.transition(1, 1));
      ASSERT_EQ(irs::parametric_description::transition_t(3, 0), d.transition(1, 2));
      ASSERT_EQ(irs::parametric_description::transition_t(1, 1), d.transition(1, 3));
      ASSERT_EQ(irs::parametric_description::transition_t(2, 0), d.transition(1, 4));
      ASSERT_EQ(irs::parametric_description::transition_t(1, 1), d.transition(1, 5));
      ASSERT_EQ(irs::parametric_description::transition_t(3, 0), d.transition(1, 6));
      ASSERT_EQ(irs::parametric_description::transition_t(1, 1), d.transition(1, 7));
      // state 2
      ASSERT_EQ(irs::parametric_description::transition_t(0, 0), d.transition(2, 0));
      ASSERT_EQ(irs::parametric_description::transition_t(4, 1), d.transition(2, 1));
      ASSERT_EQ(irs::parametric_description::transition_t(4, 2), d.transition(2, 2));
      ASSERT_EQ(irs::parametric_description::transition_t(2, 1), d.transition(2, 3));
      ASSERT_EQ(irs::parametric_description::transition_t(0, 0), d.transition(2, 4));
      ASSERT_EQ(irs::parametric_description::transition_t(4, 1), d.transition(2, 5));
      ASSERT_EQ(irs::parametric_description::transition_t(4, 2), d.transition(2, 6));
      ASSERT_EQ(irs::parametric_description::transition_t(2, 1), d.transition(2, 7));
      // state 3
      ASSERT_EQ(irs::parametric_description::transition_t(0, 0), d.transition(3, 0));
      ASSERT_EQ(irs::parametric_description::transition_t(4, 1), d.transition(3, 1));
      ASSERT_EQ(irs::parametric_description::transition_t(4, 2), d.transition(3, 2));
      ASSERT_EQ(irs::parametric_description::transition_t(2, 1), d.transition(3, 3));
      ASSERT_EQ(irs::parametric_description::transition_t(4, 3), d.transition(3, 4));
      ASSERT_EQ(irs::parametric_description::transition_t(5, 1), d.transition(3, 5));
      ASSERT_EQ(irs::parametric_description::transition_t(2, 2), d.transition(3, 6));
      ASSERT_EQ(irs::parametric_description::transition_t(3, 1), d.transition(3, 7));
      // state 4
      ASSERT_EQ(irs::parametric_description::transition_t(0, 0), d.transition(4, 0));
      ASSERT_EQ(irs::parametric_description::transition_t(4, 1), d.transition(4, 1));
      ASSERT_EQ(irs::parametric_description::transition_t(0, 0), d.transition(4, 2));
      ASSERT_EQ(irs::parametric_description::transition_t(4, 1), d.transition(4, 3));
      ASSERT_EQ(irs::parametric_description::transition_t(0, 0), d.transition(4, 4));
      ASSERT_EQ(irs::parametric_description::transition_t(4, 1), d.transition(4, 5));
      ASSERT_EQ(irs::parametric_description::transition_t(0, 0), d.transition(4, 6));
      ASSERT_EQ(irs::parametric_description::transition_t(4, 1), d.transition(4, 7));
      // state 5
      ASSERT_EQ(irs::parametric_description::transition_t(0, 0), d.transition(5, 0));
      ASSERT_EQ(irs::parametric_description::transition_t(4, 1), d.transition(5, 1));
      ASSERT_EQ(irs::parametric_description::transition_t(0, 0), d.transition(5, 2));
      ASSERT_EQ(irs::parametric_description::transition_t(4, 1), d.transition(5, 3));
      ASSERT_EQ(irs::parametric_description::transition_t(4, 3), d.transition(5, 4));
      ASSERT_EQ(irs::parametric_description::transition_t(5, 1), d.transition(5, 5));
      ASSERT_EQ(irs::parametric_description::transition_t(4, 3), d.transition(5, 6));
      ASSERT_EQ(irs::parametric_description::transition_t(5, 1), d.transition(5, 7));
    };

    auto description = irs::make_parametric_description(1, false);
    ASSERT_TRUE(bool(description));
    ASSERT_EQ(6, description.size());
    ASSERT_EQ(3, description.chi_size());
    ASSERT_EQ(8, description.chi_max());
    ASSERT_EQ(1, description.max_distance());
    ASSERT_NE(description, irs::make_parametric_description(1, true));
    ASSERT_EQ(description, irs::make_parametric_description(1, false));
    assert_distance(description);
    assert_transitions(description);
    assert_read_write(description);

    assert_description(
      description,
      irs::ref_cast<irs::byte_type>("a"_sr),
      {
        { irs::bytes_ref::EMPTY, 1, 1, 1 },
        // 1-byte sequence
        { irs::ref_cast<irs::byte_type>("a"_sr), 0, 0, 0 },
        { irs::ref_cast<irs::byte_type>("b"_sr), 1, 1, 1 },
        // 2-byte sequence
        { irs::ref_cast<irs::byte_type>("\xD1\x83"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xD1\x83\xD1\x83"_sr), 2, 2, 2 },
        // incomplete 2-byte utf8 char - treat symbol as non-existent
        { irs::ref_cast<irs::byte_type>("\xD1"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xD1\x83\xD1"_sr), 1, 1, 1 },
        // 3-byte sequence
        { irs::ref_cast<irs::byte_type>("\xE2\x9E\x96"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xE2\x9E\x96\xE2\x9E\xE2"_sr), 2, 2, 2 },
        // incomplete 3-byte utf8 char - treat symbol as non-existent
        { irs::ref_cast<irs::byte_type>("\xE2\x9E"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xE2\x9E\x96\xE2"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xE2\x9E\x96\xE2\x9E"_sr), 1, 1, 1 },
        // 4-byte sequence
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xE2\x9E\x96"_sr), 2, 2, 2 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xF0\x9F\x98\x81"_sr), 2, 2, 2 },
        // incomplete 4-byte utf8 char - treat symbol as non-existent
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xF0"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xF0\x9F"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xF0\x9F\x98"_sr), 1, 1, 1 },
      }
    );

    assert_description(
      description,
      irs::ref_cast<irs::byte_type>("\xD1\x83"_sr),
      {
        { irs::bytes_ref::EMPTY, 1, 1, 1 },
        // 1-byte sequence
        { irs::ref_cast<irs::byte_type>("a"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("b"_sr), 1, 1, 1 },
        // 2-byte sequence
        { irs::ref_cast<irs::byte_type>("\xD1\x83"_sr), 0, 0, 0 },
        { irs::ref_cast<irs::byte_type>("\xD1\x83\xD1\x83"_sr), 1, 1, 1 },
        // incomplete 2-byte utf8 char - treat symbol as non-existent
        { irs::ref_cast<irs::byte_type>("\xD1"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xD1\x83\xD1"_sr), 0, 0, 0 },
        // 3-byte sequence
        { irs::ref_cast<irs::byte_type>("\xE2\x9E\x96"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xE2\x9E\x96\xE2\x9E\x9E"_sr), 2, 2, 2 },
        // incomplete 3-byte utf8 char - treat symbol as non-existent
        { irs::ref_cast<irs::byte_type>("\xE2\x9E"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xE2\x9E\x96\xE2"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xE2\x9E\x96\xE2\x9E"_sr), 1, 1, 1 },
        // 4-byte sequence
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xE2\x9E\x96"_sr), 2, 2, 2 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xF0\x9F\x98\x81"_sr), 2, 2, 2 },
        // incomplete 4-byte utf8 char - treat symbol as non-existent
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xF0"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xF0\x9F"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xF0\x9F\x98"_sr), 1, 1, 1 },
      }
    );

    assert_description(
      description,
      irs::ref_cast<irs::byte_type>("\xE2\x9E\x96"_sr),
      {
        { irs::bytes_ref::EMPTY, 1, 1, 1 },
        // 1-byte sequence
        { irs::ref_cast<irs::byte_type>("a"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("b"_sr), 1, 1, 1 },
        // 2-byte sequence
        { irs::ref_cast<irs::byte_type>("\xD1\x83"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xD1\x83\xD1\x83"_sr), 2, 2, 2 },
        // incomplete 2-byte utf8 char - treat symbol as non-existent
        { irs::ref_cast<irs::byte_type>("\xD1"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xD1\x83\xD1"_sr), 1, 1, 1 },
        // invalid 2-byte utf8 char - not accepted by automaton
        { irs::ref_cast<irs::byte_type>("\xD1\xFF"_sr), 1, 2, 1 },
        // 3-byte sequence
        { irs::ref_cast<irs::byte_type>("\xE2\x9E\x96"_sr), 0, 0, 0 },
        { irs::ref_cast<irs::byte_type>("\xE2\x9E\x96\xE2\x9E\x9E"_sr), 1, 1, 1 },
        // incomplete 3-byte utf8 char - treat symbol as non-existent
        { irs::ref_cast<irs::byte_type>("\xE2\x9E"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xE2\x9E\x96\xE2"_sr), 0, 0, 0 },
        { irs::ref_cast<irs::byte_type>("\xE2\x9E\x96\xE2\x9E"_sr), 0, 0, 0 },
        // 4-byte sequence
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xE2\x9E\x96"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xF0\x9F\x98\x81"_sr), 2, 2, 2 },
        // incomplete 4-byte utf8 char - treat symbol as non-existent
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xF0"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xF0\x9F"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xF0\x9F\x98"_sr), 1, 1, 1 },
      }
    );

    assert_description(
      description,
      irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81"_sr),
      {
        { irs::bytes_ref::EMPTY, 1, 1, 1 },
        // 1-byte sequence
        { irs::ref_cast<irs::byte_type>("a"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("b"_sr), 1, 1, 1 },
        // 2-byte sequence
        { irs::ref_cast<irs::byte_type>("\xD1\x83"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xD1\x83\xD1\x83"_sr), 2, 2, 2 },
        // incomplete 2-byte utf8 char - treat symbol as non-existent
        { irs::ref_cast<irs::byte_type>("\xD1"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xD1\x83\xD1"_sr), 1, 1, 1 },
        // 3-byte sequence
        { irs::ref_cast<irs::byte_type>("\xE2\x9E\x96"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xE2\x9E\x96\xE2\x9E\xE2"_sr), 2, 2, 2 },
        // incomplete 3-byte utf8 char - treat symbol as non-existent
        { irs::ref_cast<irs::byte_type>("\xE2\x9E"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xE2\x9E\x96\xE2"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xE2\x9E\x96\xE2\x9E"_sr), 1, 1, 1 },
        // 4-byte sequence
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81"_sr), 0, 0, 0 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xE2\x9E\x96"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xF0\x9F\x98\x81"_sr), 1, 1, 1 },
        // incomplete 4-byte utf8 char - treat symbol as non-existent
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xF0"_sr), 0, 0, 0 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xF0\x9F"_sr), 0, 0, 0 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xF0\x9F\x98"_sr), 0, 0, 0 },
      }
    );

    assert_description(
      description,
      irs::ref_cast<irs::byte_type>("alphabet"_sr),
      {
        { irs::ref_cast<irs::byte_type>("alphabet"_sr),  0, 0, 0 },
        { irs::ref_cast<irs::byte_type>("alphaet"_sr),   1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("alphaebt"_sr),  2, 2, 2 },
        { irs::ref_cast<irs::byte_type>("alphabezz"_sr), 2, 2, 2 },
        { irs::ref_cast<irs::byte_type>("alphazez"_sr),  2, 2, 2 },
      }
    );

    assert_description(
      description,
      irs::ref_cast<irs::byte_type>("\xD1\x85\xD1\x83\xD0\xB9"_sr),
      {
        { irs::ref_cast<irs::byte_type>("\xD1\x85\xD1\x83\xD0\xB9"_sr), 0, 0, 0 },
        { irs::ref_cast<irs::byte_type>("\xD1\x85\xD1\x84\xD0\xB9"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xD1\x85\xF0\x9F\x98\x82\xD0\xB9"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xD0\xBF\xD1\x83\xD0\xB9"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xD1\x85\xD1\x83\xD0\xB0"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xD1\x85\xD1\x83\xD1\xB9"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xD1\x85\xD1\x83\xF0\xB9\xB9\xB9"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xD1\x85\xD1\x83\xD0\xB9\xD0\xBD\xD1\x8F"_sr), 2, 2, 2 },
        { irs::ref_cast<irs::byte_type>("\xD0\xBF\xD1\x83\xD0\xB9\xD0\xBD\xD1\x8F"_sr), 2, 2, 3 },
      }
    );
  }

  // transpositions
  {
    auto description = irs::make_parametric_description(1, true);
    ASSERT_TRUE(bool(description));
    ASSERT_EQ(8, description.size());
    ASSERT_EQ(3, description.chi_size());
    ASSERT_EQ(8, description.chi_max());
    ASSERT_EQ(1, description.max_distance());
    ASSERT_EQ(description, irs::make_parametric_description(1, true));
    ASSERT_NE(description, irs::make_parametric_description(1, false));
    assert_read_write(description);

    assert_description(
      description,
      irs::ref_cast<irs::byte_type>("alphabet"_sr),
      {
        { irs::ref_cast<irs::byte_type>("alphabet"_sr),  0, 0, 0 },
        { irs::ref_cast<irs::byte_type>("alphabez"_sr),  1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("alphaet"_sr),   1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("alphaebt"_sr),  1, 1, 2 },
        { irs::ref_cast<irs::byte_type>("alphabezz"_sr), 2, 2, 2 },
        { irs::ref_cast<irs::byte_type>("alphazez"_sr),  2, 2, 2 },
      }
    );

    assert_description(
      description,
      irs::ref_cast<irs::byte_type>("a"_sr),
      {
        { irs::bytes_ref::EMPTY, 1, 1, 1 },
        // 1-byte sequence
        { irs::ref_cast<irs::byte_type>("a"_sr), 0, 0, 0 },
        { irs::ref_cast<irs::byte_type>("b"_sr), 1, 1, 1 },
        // 2-byte sequence
        { irs::ref_cast<irs::byte_type>("\xD1\x83"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xD1\x83\xD1\x83"_sr), 2, 2, 2 },
        // incomplete 2-byte utf8 char - treat symbol as non-existent
        { irs::ref_cast<irs::byte_type>("\xD1"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xD1\x83\xD1"_sr), 1, 1, 1 },
        // 3-byte sequence
        { irs::ref_cast<irs::byte_type>("\xE2\x9E\x96"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xE2\x9E\x96\xE2\x9E\xE2"_sr), 2, 2, 2 },
        // incomplete 3-byte utf8 char - treat symbol as non-existent
        { irs::ref_cast<irs::byte_type>("\xE2\x9E"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xE2\x9E\x96\xE2"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xE2\x9E\x96\xE2\x9E"_sr), 1, 1, 1 },
        // 4-byte sequence
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xE2\x9E\x96"_sr), 2, 2, 2 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xF0\x9F\x98\x81"_sr), 2, 2, 2 },
        // incomplete 4-byte utf8 char - treat symbol as non-existent
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xF0"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xF0\x9F"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xF0\x9F\x98"_sr), 1, 1, 1 },
      }
    );

    assert_description(
      description,
      irs::ref_cast<irs::byte_type>("\xD1\x83"_sr),
      {
        { irs::bytes_ref::EMPTY, 1, 1, 1 },
        // 1-byte sequence
        { irs::ref_cast<irs::byte_type>("a"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("b"_sr), 1, 1, 1 },
        // 2-byte sequence
        { irs::ref_cast<irs::byte_type>("\xD1\x83"_sr), 0, 0, 0 },
        { irs::ref_cast<irs::byte_type>("\xD1\x83\xD1\x83"_sr), 1, 1, 1 },
        // incomplete 2-byte utf8 char - treat symbol as non-existent
        { irs::ref_cast<irs::byte_type>("\xD1"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xD1\x83\xD1"_sr), 0, 0, 0 },
        // 3-byte sequence
        { irs::ref_cast<irs::byte_type>("\xE2\x9E\x96"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xE2\x9E\x96\xE2\x9E\xE2"_sr), 2, 2, 2 },
        // incomplete 3-byte utf8 char - treat symbol as non-existent
        { irs::ref_cast<irs::byte_type>("\xE2\x9E"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xE2\x9E\x96\xE2"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xE2\x9E\x96\xE2\x9E"_sr), 1, 1, 1 },
        // 4-byte sequence
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xE2\x9E\x96"_sr), 2, 2, 2 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xF0\x9F\x98\x81"_sr), 2, 2, 2 },
        // incomplete 4-byte utf8 char - treat symbol as non-existent
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xF0"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xF0\x9F"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xF0\x9F\x98"_sr), 1, 1, 1 },
      }
    );

    assert_description(
      description,
      irs::ref_cast<irs::byte_type>("\xE2\x9E\x96"_sr),
      {
        { irs::bytes_ref::EMPTY, 1, 1, 1 },
        // 1-byte sequence
        { irs::ref_cast<irs::byte_type>("a"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("b"_sr), 1, 1, 1 },
        // 2-byte sequence
        { irs::ref_cast<irs::byte_type>("\xD1\x83"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xD1\x83\xD1\x83"_sr), 2, 2, 2 },
        // incomplete 2-byte utf8 char - treat symbol as non-existent
        { irs::ref_cast<irs::byte_type>("\xD1"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xD1\x83\xD1"_sr), 1, 1, 1 },
        // 3-byte sequence
        { irs::ref_cast<irs::byte_type>("\xE2\x9E\x96"_sr), 0, 0, 0 },
        { irs::ref_cast<irs::byte_type>("\xE2\x9E\x96\xE2\x9E\x9E"_sr), 1, 1, 1 },
        // not accepted by automaton due to invalid utf8 characters
        { irs::ref_cast<irs::byte_type>("\xE2\x9E\x96\xE2\x9E\xE2"_sr), 1, 2, 1 },
        // incomplete 3-byte utf8 char - treat symbol as non-existent
        { irs::ref_cast<irs::byte_type>("\xE2\x9E"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xE2\x9E\x96\xE2"_sr), 0, 0, 0 },
        { irs::ref_cast<irs::byte_type>("\xE2\x9E\x96\xE2\x9E"_sr), 0, 0, 0 },
        // 4-byte sequence
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xE2\x9E\x96"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xF0\x9F\x98\x81"_sr), 2, 2, 2 },
        // incomplete 4-byte utf8 char - treat symbol as non-existent
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xF0"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xF0\x9F"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xF0\x9F\x98"_sr), 1, 1, 1 },
      }
    );

    assert_description(
      description,
      irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81"_sr),
      {
        { irs::bytes_ref::EMPTY, 1, 1, 1 },
        // 1-byte sequence
        { irs::ref_cast<irs::byte_type>("a"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("b"_sr), 1, 1, 1 },
        // 2-byte sequence
        { irs::ref_cast<irs::byte_type>("\xD1\x83"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xD1\x83\xD1\x83"_sr), 2, 2, 2 },
        // incomplete 2-byte utf8 char - treat symbol as non-existent
        { irs::ref_cast<irs::byte_type>("\xD1"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xD1\x83\xD1"_sr), 1, 1, 1 },
        // 3-byte sequence
        { irs::ref_cast<irs::byte_type>("\xE2\x9E\x96"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xE2\x9E\x96\xE2\x9E\xE2"_sr), 2, 2, 2 },
        // incomplete 3-byte utf8 char - treat symbol as non-existent
        { irs::ref_cast<irs::byte_type>("\xE2\x9E"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xE2\x9E\x96\xE2"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xE2\x9E\x96\xE2\x9E"_sr), 1, 1, 1 },
        // 4-byte sequence
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81"_sr), 0, 0, 0 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xE2\x9E\x96"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xF0\x9F\x98\x81"_sr), 1, 1, 1 },
        // incomplete 4-byte utf8 char - treat symbol as non-existent
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xF0"_sr), 0, 0, 0 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xF0\x9F"_sr), 0, 0, 0 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xF0\x9F\x98"_sr), 0, 0, 0 },
      }
    );

    assert_description(
      description,
      irs::ref_cast<irs::byte_type>("\xD1\x85\xD1\x83\xD0\xB9"_sr),
      {
        { irs::ref_cast<irs::byte_type>("\xD1\x85\xD1\x83\xD0\xB9"_sr), 0, 0, 0 },
        { irs::ref_cast<irs::byte_type>("\xD0\xBF\xD1\x83\xD0\xB9"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xD1\x85\xD1\x83\xD0\xB9\xD0\xBD\xD1\x8F"_sr), 2, 2, 2 },
        { irs::ref_cast<irs::byte_type>("\xD0\xBF\xD1\x83\xD0\xB9\xD0\xBD\xD1\x8F"_sr), 2, 2, 3 },
      }
    );
  }
}

TEST(levenshtein_utils_test, test_description_2) {
  // no transpositions
  {
    auto description = irs::make_parametric_description(2, false);
    ASSERT_TRUE(bool(description));
    ASSERT_EQ(31, description.size());
    ASSERT_EQ(5, description.chi_size());
    ASSERT_EQ(32, description.chi_max());
    ASSERT_EQ(2, description.max_distance());
    ASSERT_NE(description, irs::make_parametric_description(2, true));
    assert_read_write(description);

    assert_description(
      description,
      irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81"_sr),
      {
        { irs::bytes_ref::EMPTY, 1, 1, 1 },
        // 1-byte sequence
        { irs::ref_cast<irs::byte_type>("a"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("b"_sr), 1, 1, 1 },
        // 2-byte sequence
        { irs::ref_cast<irs::byte_type>("\xD1\x83"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xD1\x83\xD1\x83"_sr), 2, 2, 2 },
        // incomplete 2-byte utf8 char - treat symbol as non-existent
        { irs::ref_cast<irs::byte_type>("\xD1"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xD1\x83\xD1"_sr), 1, 1, 1 },
        // 3-byte sequence
        { irs::ref_cast<irs::byte_type>("\xE2\x9E\x96"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xE2\x9E\x96\xE2\x9E\x9E"_sr), 2, 2, 2 },
        // not accepted by automaton due to invalid utf8 characters
        { irs::ref_cast<irs::byte_type>("\xE2\x9E\x96\xE2\x9E\xE2"_sr), 2, 3, 2 },
        // incomplete 3-byte utf8 char - treat symbol as non-existent
        { irs::ref_cast<irs::byte_type>("\xE2\x9E"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xE2\x9E\x96\xE2"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xE2\x9E\x96\xE2\x9E"_sr), 1, 1, 1 },
        // 4-byte sequence
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81"_sr), 0, 0, 0 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xE2\x9E\x96"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xF0\x9F\x98\x81"_sr), 1, 1, 1 },
        // incomplete 4-byte utf8 char - treat symbol as non-existent
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xF0"_sr), 0, 0, 0 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xF0\x9F"_sr), 0, 0, 0 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xF0\x9F\x98"_sr), 0, 0, 0 },
      }
    );

    assert_description(
      description,
      irs::ref_cast<irs::byte_type>("alphabet"_sr),
      {
        { irs::ref_cast<irs::byte_type>("alphabet"_sr),  0, 0, 0 },
        { irs::ref_cast<irs::byte_type>("alphabez"_sr),  1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("alphaet"_sr),   1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("alhpaebt"_sr),  3, 3, 4 },
        { irs::ref_cast<irs::byte_type>("alphaebt"_sr),  2, 2, 2 },
        { irs::ref_cast<irs::byte_type>("alphabezz"_sr), 2, 2, 2 },
        { irs::ref_cast<irs::byte_type>("alphazez"_sr),  2, 2, 2 },
        { irs::ref_cast<irs::byte_type>("lphazez"_sr),   3, 3, 3 },
        { irs::ref_cast<irs::byte_type>("\x61\x6c\x70\x68\xD1\x96\x62\x65\x74"_sr), 1, 1, 1 },
        // not accepted by automaton due to invalid utf8 characters
        { irs::ref_cast<irs::byte_type>("\x61\x6c\x70\x68\xD1\xFE\x62\x65\x74"_sr), 1, 3, 1 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\x6c\x70\x68\xD1\x96\x62\x65\x74"_sr), 2, 2, 2 },
        // not accepted by automaton due to invalid utf8 characters
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\x6c\x70\x68\xD1\xFE\x62\x65\x74"_sr), 2, 3, 2 },
        // not accepted by automaton due to invalid utf8 characters
        { irs::ref_cast<irs::byte_type>("\x61\x6c\x70\xE2\xFF\xFF\xD1\xFE\x62\x65\x74"_sr), 2, 3, 2 },
        { irs::ref_cast<irs::byte_type>("\x61\x6c\x70\xD1\x95\x70\xD1\x96\x62\x65\x74"_sr), 3, 3, 3 },
      }
    );

    assert_description(
      description,
      irs::ref_cast<irs::byte_type>("\xD1\x85\xD1\x83\xD0\xB9"_sr),
      {
        { irs::ref_cast<irs::byte_type>("\xD1\x85\xD1\x83\xD0\xB9"_sr), 0, 0, 0 },
        { irs::ref_cast<irs::byte_type>("\xD0\xBF\xD1\x83\xD0\xB9"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xD1\x85\xD1\x83\xD0\xB9\xD0\xBD\xD1\x8F"_sr), 2, 2, 2 },
        { irs::ref_cast<irs::byte_type>("\xD0\xBF\xD1\x83\xD0\xB9\xD0\xBD\xD1\x8F"_sr), 3, 3, 3 },
      }
    );
  }

  // transpositions
  {
    auto description = irs::make_parametric_description(2, true);
    ASSERT_TRUE(bool(description));
    ASSERT_EQ(68, description.size());
    ASSERT_EQ(5, description.chi_size());
    ASSERT_EQ(32, description.chi_max());
    ASSERT_EQ(2, description.max_distance());
    ASSERT_NE(description, irs::make_parametric_description(2, false));
    assert_read_write(description);

    assert_description(
      description,
      irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81"_sr),
      {
        { irs::bytes_ref::EMPTY, 1, 1, 1 },
        // 1-byte sequence
        { irs::ref_cast<irs::byte_type>("a"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("b"_sr), 1, 1, 1 },
        // 2-byte sequence
        { irs::ref_cast<irs::byte_type>("\xD1\x83"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xD1\x83\xD1\x83"_sr), 2, 2, 2 },
        // incomplete 2-byte utf8 char - treat symbol as non-existent
        { irs::ref_cast<irs::byte_type>("\xD1"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xD1\x83\xD1"_sr), 1, 1, 1 },
        // 3-byte sequence
        { irs::ref_cast<irs::byte_type>("\xE2\x9E\x96"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xE2\x9E\x96\xE2\x9E\x96"_sr), 2, 2, 2 },
        // not accepted by automaton due to invalid utf8 characters
        { irs::ref_cast<irs::byte_type>("\xE2\x9E\x96\xE2\x9E\xE2"_sr), 2, 3, 2 },
        // incomplete 3-byte utf8 char - treat symbol as non-existent
        { irs::ref_cast<irs::byte_type>("\xE2\x9E"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xE2\x9E\x96\xE2"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xE2\x9E\x96\xE2\x9E"_sr), 1, 1, 1 },
        // 4-byte sequence
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81"_sr), 0, 0, 0 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xE2\x9E\x96"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xF0\x9F\x98\x81"_sr), 1, 1, 1 },
        // incomplete 4-byte utf8 char - treat symbol as non-existent
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xF0"_sr), 0, 0, 0 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xF0\x9F"_sr), 0, 0, 0 },
        { irs::ref_cast<irs::byte_type>("\xF0\x9F\x98\x81\xF0\x9F\x98"_sr), 0, 0, 0 },
      }
    );

    assert_description(
      description,
      irs::ref_cast<irs::byte_type>("alphabet"_sr),
      {
        { irs::ref_cast<irs::byte_type>("alphabet"_sr),  0, 0, 0 },
        { irs::ref_cast<irs::byte_type>("alphabez"_sr),  1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("alphaet"_sr),   1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("alhpaebt"_sr),  2, 2, 4 },
        { irs::ref_cast<irs::byte_type>("alphaebt"_sr),  1, 1, 2 },
        { irs::ref_cast<irs::byte_type>("alphabezz"_sr), 2, 2, 2 },
        { irs::ref_cast<irs::byte_type>("alphazez"_sr),  2, 2, 2 },
        { irs::ref_cast<irs::byte_type>("lphazez"_sr),   3, 3, 3 },
      }
    );

    assert_description(
      description,
      irs::ref_cast<irs::byte_type>("\xD1\x85\xD1\x83\xD0\xB9"_sr),
      {
        { irs::ref_cast<irs::byte_type>("\xD1\x85\xD1\x83\xD0\xB9"_sr), 0, 0, 0 },
        { irs::ref_cast<irs::byte_type>("\xD0\xBF\xD1\x83\xD0\xB9"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xD1\x85\xD1\x83\xD0\xB9\xD0\xBD\xD1\x8F"_sr), 2, 2, 2 },
        { irs::ref_cast<irs::byte_type>("\xD0\xBF\xD1\x83\xD0\xB9\xD0\xBD\xD1\x8F"_sr), 3, 3, 3 },
      }
    );
  }
}

TEST(levenshtein_utils_test, test_description_3) {
  // no transpositions
  {
    auto description = irs::make_parametric_description(3, false);
    ASSERT_TRUE(bool(description));
    ASSERT_EQ(197, description.size());
    ASSERT_EQ(7, description.chi_size());
    ASSERT_EQ(128, description.chi_max());
    ASSERT_EQ(3, description.max_distance());
    assert_read_write(description);

    assert_description(
      description,
      irs::ref_cast<irs::byte_type>("alphabet"_sr),
      {
        { irs::ref_cast<irs::byte_type>("alphabet"_sr),  0, 0, 0 },
        { irs::ref_cast<irs::byte_type>("alphabez"_sr),  1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("alphaet"_sr),   1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("alhpaebt"_sr),  4, 4, 4 },
        { irs::ref_cast<irs::byte_type>("alhpeabt"_sr),  3, 3, 3 },
        { irs::ref_cast<irs::byte_type>("laphaebt"_sr),  4, 4, 4 },
        { irs::ref_cast<irs::byte_type>("alphaebt"_sr),  2, 2, 2 },
        { irs::ref_cast<irs::byte_type>("alphabezz"_sr), 2, 2, 2 },
        { irs::ref_cast<irs::byte_type>("alphazez"_sr),  2, 2, 2 },
        { irs::ref_cast<irs::byte_type>("lphazez"_sr),   3, 3, 3 },
      }
    );

    assert_description(
      description,
      irs::ref_cast<irs::byte_type>("\xD1\x85\xD1\x83\xD0\xB9"_sr),
      {
        { irs::ref_cast<irs::byte_type>("\xD1\x85\xD1\x83\xD0\xB9"_sr), 0, 0, 0 },
        { irs::ref_cast<irs::byte_type>("\xD0\xBF\xD1\x83\xD0\xB9"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xD1\x85\xD1\x83\xD0\xB9\xD0\xBD\xD1\x8F"_sr), 2, 2, 2 },
        { irs::ref_cast<irs::byte_type>("\xD0\xBF\xD1\x83\xD0\xB9\xD0\xBD\xD1\x8F"_sr), 3, 3, 3 },
      }
    );

    // "aec" vs "abc"
    ASSERT_EQ(1, irs::edit_distance(description, irs::ref_cast<irs::byte_type>("aec"_sr), irs::ref_cast<irs::byte_type>("abc"_sr)));
    ASSERT_EQ(1, irs::edit_distance(description, irs::ref_cast<irs::byte_type>("abc"_sr), irs::ref_cast<irs::byte_type>("aec"_sr)));
    // "aec" vs "ac"
    ASSERT_EQ(1, irs::edit_distance(description, irs::ref_cast<irs::byte_type>("aec"_sr), irs::ref_cast<irs::byte_type>("ac"_sr)));
    ASSERT_EQ(1, irs::edit_distance(description, irs::ref_cast<irs::byte_type>("ac"_sr), irs::ref_cast<irs::byte_type>("aec"_sr)));
    // "aec" vs "zaec"
    ASSERT_EQ(1, irs::edit_distance(description, irs::ref_cast<irs::byte_type>("aec"_sr), irs::ref_cast<irs::byte_type>("zaec"_sr)));
    ASSERT_EQ(1, irs::edit_distance(description, irs::ref_cast<irs::byte_type>("zaec"_sr), irs::ref_cast<irs::byte_type>("aec"_sr)));
    // "aec" vs "abcd"
    ASSERT_EQ(2, irs::edit_distance(description, irs::ref_cast<irs::byte_type>("aec"_sr), irs::ref_cast<irs::byte_type>("abcd"_sr)));
    ASSERT_EQ(2, irs::edit_distance(description, irs::ref_cast<irs::byte_type>("abcd"_sr), irs::ref_cast<irs::byte_type>("aec"_sr)));
    // "aec" vs "abcdz"
    ASSERT_EQ(3, irs::edit_distance(description, irs::ref_cast<irs::byte_type>("aec"_sr), irs::ref_cast<irs::byte_type>("abcdz"_sr)));
    ASSERT_EQ(3, irs::edit_distance(description, irs::ref_cast<irs::byte_type>("abcdz"_sr), irs::ref_cast<irs::byte_type>("aec"_sr)));
    // "aec" vs "abcdefasdfasdf", can differentiate distances up to 'desc.max_distance'
    ASSERT_EQ(description.max_distance()+1, irs::edit_distance(description, irs::ref_cast<irs::byte_type>("aec"_sr), irs::ref_cast<irs::byte_type>("abcdefasdfasdf"_sr)));
    ASSERT_EQ(description.max_distance()+1, irs::edit_distance(description, irs::ref_cast<irs::byte_type>("abcdefasdfasdf"_sr), irs::ref_cast<irs::byte_type>("aec"_sr)));
  }

  // transpositions
  {
    auto description = irs::make_parametric_description(3, true);
    ASSERT_TRUE(bool(description));
    ASSERT_EQ(769, description.size());
    ASSERT_EQ(7, description.chi_size());
    ASSERT_EQ(128, description.chi_max());
    ASSERT_EQ(3, description.max_distance());
    assert_read_write(description);

    assert_description(
      description,
      irs::ref_cast<irs::byte_type>("alphabet"_sr),
      {
        { irs::ref_cast<irs::byte_type>("alphabet"_sr),  0, 0, 0 },
        { irs::ref_cast<irs::byte_type>("alphabez"_sr),  1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("alphaet"_sr),   1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("alhpaebt"_sr),  2, 2, 4 },
        { irs::ref_cast<irs::byte_type>("alhpeabt"_sr),  3, 3, 3 },
        { irs::ref_cast<irs::byte_type>("laphaebt"_sr),  2, 2, 4 },
        { irs::ref_cast<irs::byte_type>("alphaebt"_sr),  1, 1, 2 },
        { irs::ref_cast<irs::byte_type>("alphabezz"_sr), 2, 2, 2 },
        { irs::ref_cast<irs::byte_type>("alphazez"_sr),  2, 2, 2 },
        { irs::ref_cast<irs::byte_type>("lphazez"_sr),   3, 3, 3 },
        { irs::ref_cast<irs::byte_type>("lpzazez"_sr),   4, 4, 4 },
      }
    );

    assert_description(
      description,
      irs::ref_cast<irs::byte_type>("\xD1\x85\xD1\x83\xD0\xB9"_sr),
      {
        { irs::ref_cast<irs::byte_type>("\xD1\x85\xD1\x83\xD0\xB9"_sr), 0, 0, 0 },
        { irs::ref_cast<irs::byte_type>("\xD0\xBF\xD1\x83\xD0\xB9"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xD1\x85\xD1\x83\xD0\xB9\xD0\xBD\xD1\x8F"_sr), 2, 2, 2 },
        { irs::ref_cast<irs::byte_type>("\xD0\xBF\xD1\x83\xD0\xB9\xD0\xBD\xD1\x8F"_sr), 3, 3, 3 },
      }
    );
  }
}

TEST(levenshtein_utils_test, test_description_4) {
  // no transpositions
  {
    auto description = irs::make_parametric_description(4, false);
    ASSERT_TRUE(bool(description));
    ASSERT_EQ(1354, description.size());
    ASSERT_EQ(9, description.chi_size());
    ASSERT_EQ(512, description.chi_max());
    ASSERT_EQ(4, description.max_distance());
    ASSERT_EQ(description, description);
    assert_read_write(description);

    assert_description(
      description,
      irs::ref_cast<irs::byte_type>("alphabet"_sr),
      {
        { irs::ref_cast<irs::byte_type>("alphabet"_sr),  0, 0, 0 },
        { irs::ref_cast<irs::byte_type>("alphabez"_sr),  1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("alphaet"_sr),   1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("alhpaebt"_sr),  4, 4, 4 },
        { irs::ref_cast<irs::byte_type>("alhpeabt"_sr),  3, 3, 3 },
        { irs::ref_cast<irs::byte_type>("laphaebt"_sr),  4, 4, 4 },
        { irs::ref_cast<irs::byte_type>("alphaebt"_sr),  2, 2, 2 },
        { irs::ref_cast<irs::byte_type>("alphabezz"_sr), 2, 2, 2 },
        { irs::ref_cast<irs::byte_type>("alphazez"_sr),  2, 2, 2 },
        { irs::ref_cast<irs::byte_type>("lphazez"_sr),   3, 3, 3 },
        { irs::ref_cast<irs::byte_type>("phazez"_sr),    4, 4, 4 },
        { irs::ref_cast<irs::byte_type>("phzez"_sr),     5, 5, 5 },
        { irs::ref_cast<irs::byte_type>("hzez"_sr),      5, 5, 6 },
      }
    );

    assert_description(
      description,
      irs::ref_cast<irs::byte_type>("\xD1\x85\xD1\x83\xD0\xB9"_sr),
      {
        { irs::ref_cast<irs::byte_type>("\xD1\x85\xD1\x83\xD0\xB9"_sr), 0, 0, 0 },
        { irs::ref_cast<irs::byte_type>("\xD0\xBF\xD1\x83\xD0\xB9"_sr), 1, 1, 1 },
        { irs::ref_cast<irs::byte_type>("\xD1\x85\xD1\x83\xD0\xB9\xD0\xBD\xD1\x8F"_sr), 2, 2, 2 },
        { irs::ref_cast<irs::byte_type>("\xD0\xBF\xD1\x83\xD0\xB9\xD0\xBD\xD1\x8F"_sr), 3, 3, 3 },
      }
    );
  }

// Commented out since it takes ~10 min to pass
//#ifndef IRESEARCH_DEBUG
//  // with transpositions
//  {
//    auto description = irs::make_parametric_description(4, true);
//    ASSERT_TRUE(bool(description));
//    ASSERT_EQ(9628, description.size());
//    ASSERT_EQ(9, description.chi_size());
//    ASSERT_EQ(512, description.chi_max());
//    ASSERT_EQ(4, description.max_distance());
//
//    assert_description(
//      description,
//      irs::ref_cast<irs::byte_type>("alphabet"_sr),
//      {
//        { irs::ref_cast<irs::byte_type>("alphabet"_sr),  0, 0, 0 },
//        { irs::ref_cast<irs::byte_type>("alphabez"_sr),  1, 1, 1 },
//        { irs::ref_cast<irs::byte_type>("alphaet"_sr),   1, 1, 1 },
//        { irs::ref_cast<irs::byte_type>("alhpaebt"_sr),  2, 2, 4 },
//        { irs::ref_cast<irs::byte_type>("alhpeabt"_sr),  3, 3, 3 },
//        { irs::ref_cast<irs::byte_type>("laphaebt"_sr),  2, 2, 4 },
//        { irs::ref_cast<irs::byte_type>("alphaebt"_sr),  1, 1, 2 },
//        { irs::ref_cast<irs::byte_type>("alphabezz"_sr), 2, 2, 2 },
//        { irs::ref_cast<irs::byte_type>("alphazez"_sr),  2, 2, 2 },
//        { irs::ref_cast<irs::byte_type>("lphazez"_sr),   3, 3, 3 },
//        { irs::ref_cast<irs::byte_type>("phazez"_sr),    4, 4, 4 },
//        { irs::ref_cast<irs::byte_type>("phzez"_sr),     5, 5, 5 },
//        { irs::ref_cast<irs::byte_type>("hzez"_sr),      5, 5, 6 },
//      }
//    );
//  }
//#endif
}

TEST(levenshtein_utils_test, test_description_invalid) {
  ASSERT_FALSE(irs::parametric_description());

  // no transpositions
  {
    auto description = irs::make_parametric_description(irs::parametric_description::MAX_DISTANCE+1, false);
    ASSERT_FALSE(bool(description));
    ASSERT_EQ(0, description.size());
    ASSERT_EQ(0, description.chi_size());
    ASSERT_EQ(0, description.chi_max());
    ASSERT_EQ(0, description.max_distance());
    ASSERT_EQ(irs::parametric_description(), description);
    ASSERT_NE(description, irs::make_parametric_description(0, false));
  }

  // transpositions
  {
    auto description = irs::make_parametric_description(irs::parametric_description::MAX_DISTANCE+1, true);
    ASSERT_FALSE(bool(description));
    ASSERT_EQ(0, description.size());
    ASSERT_EQ(0, description.chi_size());
    ASSERT_EQ(0, description.chi_max());
    ASSERT_EQ(0, description.max_distance());
  }
}
