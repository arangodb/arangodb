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
#include "utils/fst_table_matcher.hpp"

using namespace irs::literals;

namespace {

void assert_description(
    const irs::parametric_description& description,
    const irs::bytes_ref& target,
    const std::vector<std::tuple<irs::bytes_ref, size_t, size_t>>& candidates) {
  auto a = irs::make_levenshtein_automaton(description, target);

  for (auto& entry : candidates) {
    const auto candidate = std::get<0>(entry);
    const size_t expected_distance = std::get<1>(entry);
    const size_t expected_distance_precise = std::get<2>(entry);
    SCOPED_TRACE(testing::Message("Target: '") << irs::ref_cast<char>(target) <<
                 "', Candidate: '" << irs::ref_cast<char>(candidate) <<
                 "' , Distance: " << expected_distance <<
                 " , Precise distance: " << expected_distance_precise);
    ASSERT_EQ(expected_distance_precise, irs::edit_distance(candidate, target));
    ASSERT_EQ(expected_distance_precise, irs::edit_distance(target, candidate));
    ASSERT_EQ(expected_distance, irs::edit_distance(description, candidate, target));
    ASSERT_EQ(expected_distance, irs::edit_distance(description, target, candidate));
    ASSERT_EQ(expected_distance <= description.max_distance(), irs::accept(a, candidate));
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
        { irs::ref_cast<irs::byte_type>("alphabet"_sr),  0, 0 },
        { irs::ref_cast<irs::byte_type>("alphabez"_sr),  1, 1 },
        { irs::ref_cast<irs::byte_type>("alphaet"_sr),   1, 1 },
        { irs::ref_cast<irs::byte_type>("alhpaebt"_sr),  1, 4 },
        { irs::ref_cast<irs::byte_type>("alphaebt"_sr),  1, 2 },
        { irs::ref_cast<irs::byte_type>("alphabezz"_sr), 1, 2 },
        { irs::ref_cast<irs::byte_type>("alphazez"_sr),  1, 2 },
        { irs::ref_cast<irs::byte_type>("lphazez"_sr),   1, 3 },
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
        { irs::ref_cast<irs::byte_type>("alphabet"_sr),  0, 0 },
        { irs::ref_cast<irs::byte_type>("alphabez"_sr),  1, 1 },
        { irs::ref_cast<irs::byte_type>("alphaet"_sr),   1, 1 },
        { irs::ref_cast<irs::byte_type>("alhpaebt"_sr),  1, 4 },
        { irs::ref_cast<irs::byte_type>("alphaebt"_sr),  1, 2 },
        { irs::ref_cast<irs::byte_type>("alphabezz"_sr), 1, 2 },
        { irs::ref_cast<irs::byte_type>("alphazez"_sr),  1, 2 },
        { irs::ref_cast<irs::byte_type>("lphazez"_sr),   1, 3 },
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
      irs::ref_cast<irs::byte_type>("alphabet"_sr),
      {
        { irs::ref_cast<irs::byte_type>("alphabet"_sr),  0, 0 },
        { irs::ref_cast<irs::byte_type>("alphaet"_sr),   1, 1 },
        { irs::ref_cast<irs::byte_type>("alphaebt"_sr),  2, 2 },
        { irs::ref_cast<irs::byte_type>("alphabezz"_sr), 2, 2 },
        { irs::ref_cast<irs::byte_type>("alphazez"_sr),  2, 2 },
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
        { irs::ref_cast<irs::byte_type>("alphabet"_sr),  0, 0 },
        { irs::ref_cast<irs::byte_type>("alphabez"_sr),  1, 1 },
        { irs::ref_cast<irs::byte_type>("alphaet"_sr),   1, 1 },
        { irs::ref_cast<irs::byte_type>("alphaebt"_sr),  1, 2 },
        { irs::ref_cast<irs::byte_type>("alphabezz"_sr), 2, 2 },
        { irs::ref_cast<irs::byte_type>("alphazez"_sr),  2, 2 },
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
      irs::ref_cast<irs::byte_type>("alphabet"_sr),
      {
        { irs::ref_cast<irs::byte_type>("alphabet"_sr),  0, 0 },
        { irs::ref_cast<irs::byte_type>("alphabez"_sr),  1, 1 },
        { irs::ref_cast<irs::byte_type>("alphaet"_sr),   1, 1 },
        { irs::ref_cast<irs::byte_type>("alhpaebt"_sr),  3, 4 },
        { irs::ref_cast<irs::byte_type>("alphaebt"_sr),  2, 2 },
        { irs::ref_cast<irs::byte_type>("alphabezz"_sr), 2, 2 },
        { irs::ref_cast<irs::byte_type>("alphazez"_sr),  2, 2 },
        { irs::ref_cast<irs::byte_type>("lphazez"_sr),   3, 3 },
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
      irs::ref_cast<irs::byte_type>("alphabet"_sr),
      {
        { irs::ref_cast<irs::byte_type>("alphabet"_sr),  0, 0 },
        { irs::ref_cast<irs::byte_type>("alphabez"_sr),  1, 1 },
        { irs::ref_cast<irs::byte_type>("alphaet"_sr),   1, 1 },
        { irs::ref_cast<irs::byte_type>("alhpaebt"_sr),  2, 4 },
        { irs::ref_cast<irs::byte_type>("alphaebt"_sr),  1, 2 },
        { irs::ref_cast<irs::byte_type>("alphabezz"_sr), 2, 2 },
        { irs::ref_cast<irs::byte_type>("alphazez"_sr),  2, 2 },
        { irs::ref_cast<irs::byte_type>("lphazez"_sr),   3, 3 },
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
        { irs::ref_cast<irs::byte_type>("alphabet"_sr),  0, 0 },
        { irs::ref_cast<irs::byte_type>("alphabez"_sr),  1, 1 },
        { irs::ref_cast<irs::byte_type>("alphaet"_sr),   1, 1 },
        { irs::ref_cast<irs::byte_type>("alhpaebt"_sr),  4, 4 },
        { irs::ref_cast<irs::byte_type>("alhpeabt"_sr),  3, 3 },
        { irs::ref_cast<irs::byte_type>("laphaebt"_sr),  4, 4 },
        { irs::ref_cast<irs::byte_type>("alphaebt"_sr),  2, 2 },
        { irs::ref_cast<irs::byte_type>("alphabezz"_sr), 2, 2 },
        { irs::ref_cast<irs::byte_type>("alphazez"_sr),  2, 2 },
        { irs::ref_cast<irs::byte_type>("lphazez"_sr),   3, 3 },
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
        { irs::ref_cast<irs::byte_type>("alphabet"_sr),  0, 0 },
        { irs::ref_cast<irs::byte_type>("alphabez"_sr),  1, 1 },
        { irs::ref_cast<irs::byte_type>("alphaet"_sr),   1, 1 },
        { irs::ref_cast<irs::byte_type>("alhpaebt"_sr),  2, 4 },
        { irs::ref_cast<irs::byte_type>("alhpeabt"_sr),  3, 3 },
        { irs::ref_cast<irs::byte_type>("laphaebt"_sr),  2, 4 },
        { irs::ref_cast<irs::byte_type>("alphaebt"_sr),  1, 2 },
        { irs::ref_cast<irs::byte_type>("alphabezz"_sr), 2, 2 },
        { irs::ref_cast<irs::byte_type>("alphazez"_sr),  2, 2 },
        { irs::ref_cast<irs::byte_type>("lphazez"_sr),   3, 3 },
        { irs::ref_cast<irs::byte_type>("lpzazez"_sr),   4, 4 },
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
        { irs::ref_cast<irs::byte_type>("alphabet"_sr),  0, 0 },
        { irs::ref_cast<irs::byte_type>("alphabez"_sr),  1, 1 },
        { irs::ref_cast<irs::byte_type>("alphaet"_sr),   1, 1 },
        { irs::ref_cast<irs::byte_type>("alhpaebt"_sr),  4, 4 },
        { irs::ref_cast<irs::byte_type>("alhpeabt"_sr),  3, 3 },
        { irs::ref_cast<irs::byte_type>("laphaebt"_sr),  4, 4 },
        { irs::ref_cast<irs::byte_type>("alphaebt"_sr),  2, 2 },
        { irs::ref_cast<irs::byte_type>("alphabezz"_sr), 2, 2 },
        { irs::ref_cast<irs::byte_type>("alphazez"_sr),  2, 2 },
        { irs::ref_cast<irs::byte_type>("lphazez"_sr),   3, 3 },
        { irs::ref_cast<irs::byte_type>("phazez"_sr),    4, 4 },
        { irs::ref_cast<irs::byte_type>("phzez"_sr),     5, 5 },
        { irs::ref_cast<irs::byte_type>("hzez"_sr),      5, 6 },
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
//        { irs::ref_cast<irs::byte_type>("alphabet"_sr),  0, 0 },
//        { irs::ref_cast<irs::byte_type>("alphabez"_sr),  1, 1 },
//        { irs::ref_cast<irs::byte_type>("alphaet"_sr),   1, 1 },
//        { irs::ref_cast<irs::byte_type>("alhpaebt"_sr),  2, 4 },
//        { irs::ref_cast<irs::byte_type>("alhpeabt"_sr),  3, 3 },
//        { irs::ref_cast<irs::byte_type>("laphaebt"_sr),  2, 4 },
//        { irs::ref_cast<irs::byte_type>("alphaebt"_sr),  1, 2 },
//        { irs::ref_cast<irs::byte_type>("alphabezz"_sr), 2, 2 },
//        { irs::ref_cast<irs::byte_type>("alphazez"_sr),  2, 2 },
//        { irs::ref_cast<irs::byte_type>("lphazez"_sr),   3, 3 },
//        { irs::ref_cast<irs::byte_type>("phazez"_sr),    4, 4 },
//        { irs::ref_cast<irs::byte_type>("phzez"_sr),     5, 5 },
//        { irs::ref_cast<irs::byte_type>("hzez"_sr),      5, 6 },
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
