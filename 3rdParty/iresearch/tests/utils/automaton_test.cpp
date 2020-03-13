﻿////////////////////////////////////////////////////////////////////////////////
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

#include "utils/automaton_utils.hpp"
#include "utils/misc.hpp"

// -----------------------------------------------------------------------------
// --SECTION--                                               boolean_weight_test
// -----------------------------------------------------------------------------

TEST(boolean_weight_test, static_const) {
  ASSERT_EQ(fst::kLeftSemiring | fst::kRightSemiring |
            fst::kCommutative | fst::kIdempotent | fst::kPath,
            fst::fsa::BooleanWeight::Properties());
  ASSERT_EQ("boolean", fst::fsa::BooleanWeight::Type());
  ASSERT_EQ(0x3F, fst::fsa::BooleanWeight::PayloadType(fst::fsa::BooleanWeight::MaxPayload));
}

TEST(boolean_weight_test, create) {
  // not a member
  {
    const fst::fsa::BooleanWeight weight;
    ASSERT_FALSE(weight.Member());
    ASSERT_EQ(fst::fsa::BooleanWeight(), weight);
    ASSERT_NE(fst::fsa::BooleanWeight(false), weight);
    ASSERT_NE(fst::fsa::BooleanWeight(false, 2), weight);
    ASSERT_NE(fst::fsa::BooleanWeight(true), weight);
    ASSERT_NE(fst::fsa::BooleanWeight(true, 1), weight);
    ASSERT_EQ(weight, weight.Quantize());
    ASSERT_EQ(weight, weight.Reverse());
    ASSERT_TRUE(ApproxEqual(weight, weight.Quantize()));
    ASSERT_TRUE(ApproxEqual(weight, weight.Reverse()));
    ASSERT_FALSE(bool(weight));
    ASSERT_NE(fst::fsa::BooleanWeight::One(), weight);
    ASSERT_NE(fst::fsa::BooleanWeight::Zero(), weight);
    ASSERT_NE(true, bool(weight));
    ASSERT_EQ(false, bool(weight));
    ASSERT_EQ(0, weight.Payload());
    ASSERT_EQ(2, weight.Hash());

    {
      std::stringstream ss;
      ss << weight;
      ASSERT_TRUE(ss.str().empty());
    }

    {
      std::stringstream ss;
      weight.Write(ss);

      fst::fsa::BooleanWeight readWeight;
      readWeight.Read(ss);
      ASSERT_EQ(weight, readWeight);
    }
  }

  // Zero
  {
    const fst::fsa::BooleanWeight weight = false;
    ASSERT_TRUE(weight.Member());
    ASSERT_NE(fst::fsa::BooleanWeight(), weight);
    ASSERT_EQ(fst::fsa::BooleanWeight(false), weight);
    ASSERT_EQ(fst::fsa::BooleanWeight(false, 2), weight);
    ASSERT_NE(fst::fsa::BooleanWeight(true), weight);
    ASSERT_NE(fst::fsa::BooleanWeight(true, 1), weight);
    ASSERT_EQ(fst::fsa::BooleanWeight::NoWeight(), weight.Quantize());
    ASSERT_NE(weight, weight.Quantize());
    ASSERT_EQ(weight, weight.Reverse());
    ASSERT_FALSE(ApproxEqual(weight, weight.Quantize()));
    ASSERT_TRUE(ApproxEqual(weight, weight.Reverse()));
    ASSERT_FALSE(bool(weight));
    ASSERT_NE(fst::fsa::BooleanWeight::One(), weight);
    ASSERT_EQ(fst::fsa::BooleanWeight::Zero(), weight);
    ASSERT_NE(true, bool(weight));
    ASSERT_EQ(false, bool(weight));
    ASSERT_EQ(0, weight.Payload());
    ASSERT_EQ(0, weight.Hash());

    {
      std::stringstream ss;
      ss << weight;
      ASSERT_EQ("{0,0}", ss.str());
    }

    {
      std::stringstream ss;
      weight.Write(ss);

      fst::fsa::BooleanWeight readWeight;
      readWeight.Read(ss);
      ASSERT_EQ(weight, weight);
      ASSERT_EQ(weight, readWeight);
    }
  }

  // Zero + Payload
  {
    const fst::fsa::BooleanWeight weight(false, 25);
    ASSERT_TRUE(weight.Member());
    ASSERT_NE(fst::fsa::BooleanWeight(), weight);
    ASSERT_EQ(fst::fsa::BooleanWeight(false), weight);
    ASSERT_EQ(fst::fsa::BooleanWeight(false, 2), weight);
    ASSERT_EQ(fst::fsa::BooleanWeight(false, 25), weight);
    ASSERT_NE(fst::fsa::BooleanWeight(true), weight);
    ASSERT_NE(fst::fsa::BooleanWeight(true, 1), weight);
    ASSERT_NE(fst::fsa::BooleanWeight(true, 25), weight);
    ASSERT_EQ(fst::fsa::BooleanWeight::NoWeight(), weight.Quantize());
    ASSERT_NE(weight, weight.Quantize());
    ASSERT_EQ(weight, weight.Reverse());
    ASSERT_FALSE(ApproxEqual(weight, weight.Quantize()));
    ASSERT_TRUE(ApproxEqual(weight, weight.Reverse()));
    ASSERT_FALSE(bool(weight));
    ASSERT_NE(fst::fsa::BooleanWeight::One(), weight);
    ASSERT_EQ(fst::fsa::BooleanWeight::Zero(), weight);
    ASSERT_NE(true, bool(weight));
    ASSERT_EQ(false, bool(weight));
    ASSERT_EQ(25, weight.Payload());
    ASSERT_EQ(0, weight.Hash());

    {
      std::stringstream ss;
      ss << weight;
      ASSERT_EQ("{0,25}", ss.str());
    }

    {
      std::stringstream ss;
      weight.Write(ss);

      fst::fsa::BooleanWeight readWeight;
      readWeight.Read(ss);
      ASSERT_EQ(weight, weight);
      ASSERT_EQ(weight, readWeight);
    }
  }

  // One
  {
    const fst::fsa::BooleanWeight weight = true;
    ASSERT_TRUE(weight.Member());
    ASSERT_NE(fst::fsa::BooleanWeight(), weight);
    ASSERT_NE(fst::fsa::BooleanWeight(false), weight);
    ASSERT_NE(fst::fsa::BooleanWeight(false, 2), weight);
    ASSERT_EQ(fst::fsa::BooleanWeight(true), weight);
    ASSERT_EQ(fst::fsa::BooleanWeight(true, 1), weight);
    ASSERT_EQ(fst::fsa::BooleanWeight::NoWeight(), weight.Quantize());
    ASSERT_NE(weight, weight.Quantize());
    ASSERT_EQ(weight, weight.Reverse());
    ASSERT_TRUE(bool(weight));
    ASSERT_EQ(fst::fsa::BooleanWeight::One(), weight);
    ASSERT_NE(fst::fsa::BooleanWeight::Zero(), weight);
    ASSERT_EQ(true, bool(weight));
    ASSERT_NE(false, bool(weight));
    ASSERT_EQ(0, weight.Payload());
    ASSERT_EQ(1, weight.Hash());

    {
      std::stringstream ss;
      ss << weight;
      ASSERT_EQ("{1,0}", ss.str());
    }

    {
      std::stringstream ss;
      weight.Write(ss);

      fst::fsa::BooleanWeight readWeight;
      readWeight.Read(ss);
      ASSERT_EQ(weight, weight);
      ASSERT_EQ(weight, readWeight);
    }
  }

  // One + Payload
  {
    const fst::fsa::BooleanWeight weight(true, 32);
    ASSERT_TRUE(weight.Member());
    ASSERT_NE(fst::fsa::BooleanWeight(), weight);
    ASSERT_NE(fst::fsa::BooleanWeight(false), weight);
    ASSERT_NE(fst::fsa::BooleanWeight(false, 2), weight);
    ASSERT_NE(fst::fsa::BooleanWeight(false, 32), weight);
    ASSERT_EQ(fst::fsa::BooleanWeight(true), weight);
    ASSERT_EQ(fst::fsa::BooleanWeight(true, 1), weight);
    ASSERT_EQ(fst::fsa::BooleanWeight(true, 32), weight);
    ASSERT_EQ(fst::fsa::BooleanWeight::NoWeight(), weight.Quantize());
    ASSERT_NE(weight, weight.Quantize());
    ASSERT_EQ(weight, weight.Reverse());
    ASSERT_TRUE(bool(weight));
    ASSERT_EQ(fst::fsa::BooleanWeight::One(), weight);
    ASSERT_NE(fst::fsa::BooleanWeight::Zero(), weight);
    ASSERT_EQ(true, bool(weight));
    ASSERT_NE(false, bool(weight));
    ASSERT_EQ(32, weight.Payload());
    ASSERT_EQ(1, weight.Hash());

    {
      std::stringstream ss;
      ss << weight;
      ASSERT_EQ("{1,32}", ss.str());
    }

    {
      std::stringstream ss;
      weight.Write(ss);

      fst::fsa::BooleanWeight readWeight;
      readWeight.Read(ss);
      ASSERT_EQ(weight, weight);
      ASSERT_EQ(weight, readWeight);
    }
  }

  // Max payload
  {
    const fst::fsa::BooleanWeight weight(true, 0xFF);
    ASSERT_TRUE(weight.Member());
    ASSERT_NE(fst::fsa::BooleanWeight(), weight);
    ASSERT_NE(fst::fsa::BooleanWeight(false), weight);
    ASSERT_NE(fst::fsa::BooleanWeight(false, 2), weight);
    ASSERT_NE(fst::fsa::BooleanWeight(false, fst::fsa::BooleanWeight::MaxPayload), weight);
    ASSERT_EQ(fst::fsa::BooleanWeight(true), weight);
    ASSERT_EQ(fst::fsa::BooleanWeight(true, 1), weight);
    ASSERT_EQ(fst::fsa::BooleanWeight(true, fst::fsa::BooleanWeight::MaxPayload), weight);
    ASSERT_EQ(fst::fsa::BooleanWeight::NoWeight(), weight.Quantize());
    ASSERT_NE(weight, weight.Quantize());
    ASSERT_EQ(weight, weight.Reverse());
    ASSERT_TRUE(bool(weight));
    ASSERT_EQ(fst::fsa::BooleanWeight::One(), weight);
    ASSERT_NE(fst::fsa::BooleanWeight::Zero(), weight);
    ASSERT_EQ(true, bool(weight));
    ASSERT_NE(false, bool(weight));
    ASSERT_EQ(fst::fsa::BooleanWeight::PayloadType(fst::fsa::BooleanWeight::MaxPayload),
              weight.Payload());
    ASSERT_EQ(1, weight.Hash());

    {
      std::stringstream ss;
      ss << weight;
      ASSERT_EQ("{1,63}", ss.str());
    }

    {
      std::stringstream ss;
      weight.Write(ss);

      fst::fsa::BooleanWeight readWeight;
      readWeight.Read(ss);
      ASSERT_EQ(weight, weight);
      ASSERT_EQ(weight, readWeight);
    }
  }
}

TEST(boolean_weight_test, divide) {
  using namespace fst;
  using namespace fst::fsa;

  ASSERT_EQ(BooleanWeight::NoWeight(), Divide(BooleanWeight(true, 31), BooleanWeight(true, 11), DIVIDE_LEFT));
  ASSERT_EQ(BooleanWeight::NoWeight(), Divide(BooleanWeight(true, 31), BooleanWeight(true, 11), DIVIDE_RIGHT));
  ASSERT_EQ(BooleanWeight::NoWeight(), Divide(BooleanWeight(true, 31), BooleanWeight(true, 11), DIVIDE_ANY));
  ASSERT_EQ(BooleanWeight::NoWeight(), Divide(BooleanWeight::One(), BooleanWeight::Zero(), DIVIDE_LEFT));
  ASSERT_EQ(BooleanWeight::NoWeight(), Divide(BooleanWeight::One(), BooleanWeight::Zero(), DIVIDE_RIGHT));
  ASSERT_EQ(BooleanWeight::NoWeight(), Divide(BooleanWeight::One(), BooleanWeight::Zero(), DIVIDE_ANY));
  ASSERT_EQ(BooleanWeight::NoWeight(), Divide(BooleanWeight::Zero(), BooleanWeight::One(), DIVIDE_LEFT));
  ASSERT_EQ(BooleanWeight::NoWeight(), Divide(BooleanWeight::Zero(), BooleanWeight::One(), DIVIDE_RIGHT));
  ASSERT_EQ(BooleanWeight::NoWeight(), Divide(BooleanWeight::Zero(), BooleanWeight::One(), DIVIDE_ANY));
  ASSERT_EQ(BooleanWeight::NoWeight(), Divide(BooleanWeight::Zero(), BooleanWeight::Zero(), DIVIDE_LEFT));
  ASSERT_EQ(BooleanWeight::NoWeight(), Divide(BooleanWeight::Zero(), BooleanWeight::Zero(), DIVIDE_RIGHT));
  ASSERT_EQ(BooleanWeight::NoWeight(), Divide(BooleanWeight::Zero(), BooleanWeight::Zero(), DIVIDE_ANY));
  ASSERT_EQ(BooleanWeight::NoWeight(), Divide(BooleanWeight::One(), BooleanWeight::One(), DIVIDE_LEFT));
  ASSERT_EQ(BooleanWeight::NoWeight(), Divide(BooleanWeight::One(), BooleanWeight::One(), DIVIDE_RIGHT));
  ASSERT_EQ(BooleanWeight::NoWeight(), Divide(BooleanWeight::One(), BooleanWeight::One(), DIVIDE_ANY));
  ASSERT_EQ(BooleanWeight::NoWeight(), Divide(BooleanWeight::One(), BooleanWeight::NoWeight(), DIVIDE_LEFT));
  ASSERT_EQ(BooleanWeight::NoWeight(), Divide(BooleanWeight::One(), BooleanWeight::NoWeight(), DIVIDE_RIGHT));
  ASSERT_EQ(BooleanWeight::NoWeight(), Divide(BooleanWeight::One(), BooleanWeight::NoWeight(), DIVIDE_ANY));
  ASSERT_EQ(BooleanWeight::NoWeight(), Divide(BooleanWeight::NoWeight(), BooleanWeight::One(), DIVIDE_LEFT));
  ASSERT_EQ(BooleanWeight::NoWeight(), Divide(BooleanWeight::NoWeight(), BooleanWeight::One(), DIVIDE_RIGHT));
  ASSERT_EQ(BooleanWeight::NoWeight(), Divide(BooleanWeight::NoWeight(), BooleanWeight::One(), DIVIDE_ANY));
  ASSERT_EQ(BooleanWeight::NoWeight(), Divide(BooleanWeight::NoWeight(), BooleanWeight::NoWeight(), DIVIDE_LEFT));
  ASSERT_EQ(BooleanWeight::NoWeight(), Divide(BooleanWeight::NoWeight(), BooleanWeight::NoWeight(), DIVIDE_RIGHT));
  ASSERT_EQ(BooleanWeight::NoWeight(), Divide(BooleanWeight::NoWeight(), BooleanWeight::NoWeight(), DIVIDE_ANY));
  ASSERT_EQ(BooleanWeight::NoWeight(), Divide(BooleanWeight::NoWeight(), BooleanWeight::One(), DIVIDE_LEFT));
  ASSERT_EQ(BooleanWeight::NoWeight(), Divide(BooleanWeight::NoWeight(), BooleanWeight::One(), DIVIDE_RIGHT));
  ASSERT_EQ(BooleanWeight::NoWeight(), Divide(BooleanWeight::NoWeight(), BooleanWeight::One(), DIVIDE_ANY));
}

TEST(boolean_weight_test, times) {
  using namespace fst;
  using namespace fst::fsa;

  ASSERT_EQ(BooleanWeight(false, 0), Times(BooleanWeight(false, 31), BooleanWeight(true, 32)));
  ASSERT_EQ(BooleanWeight(true, 11), Times(BooleanWeight(true, 31), BooleanWeight(true, 11)));
  ASSERT_EQ(BooleanWeight(false, 11), Times(BooleanWeight(false, 31), BooleanWeight(false, 11)));
  ASSERT_EQ(BooleanWeight::One(), Times(BooleanWeight::One(), BooleanWeight::One()));
  ASSERT_EQ(BooleanWeight::Zero(), Times(BooleanWeight::One(), BooleanWeight::Zero()));
  ASSERT_EQ(BooleanWeight::One(), Times(BooleanWeight::One(), BooleanWeight::NoWeight()));
  ASSERT_EQ(BooleanWeight::One(), Times(BooleanWeight::NoWeight(), BooleanWeight::NoWeight()));
  ASSERT_EQ(BooleanWeight::One(), Times(BooleanWeight::NoWeight(), BooleanWeight::One()));
}

TEST(boolean_weight_test, plus) {
  using namespace fst;
  using namespace fst::fsa;

  ASSERT_EQ(BooleanWeight(true, 63), Plus(BooleanWeight(false, 31), BooleanWeight(true, 32)));
  ASSERT_EQ(BooleanWeight(true, 31), Plus(BooleanWeight(true, 31), BooleanWeight(true, 11)));
  ASSERT_EQ(BooleanWeight(false, 31), Plus(BooleanWeight(false, 31), BooleanWeight(false, 11)));
  ASSERT_EQ(BooleanWeight::One(), Plus(BooleanWeight::One(), BooleanWeight::One()));
  ASSERT_EQ(BooleanWeight::One(), Plus(BooleanWeight::One(), BooleanWeight::Zero()));
  ASSERT_EQ(BooleanWeight::One(), Plus(BooleanWeight::One(), BooleanWeight::NoWeight()));
  ASSERT_EQ(BooleanWeight::One(), Plus(BooleanWeight::NoWeight(), BooleanWeight::NoWeight()));
  ASSERT_EQ(BooleanWeight::One(), Plus(BooleanWeight::NoWeight(), BooleanWeight::One()));
}

// -----------------------------------------------------------------------------
// --SECTION--                                     utf8_transitions_builder_test
// -----------------------------------------------------------------------------

class utf8_transitions_builder_test : public test_base {
 protected:
  static void assert_properties(const irs::automaton& a) {
    constexpr auto EXPECTED_PROPERTIES =
      fst::kILabelSorted | fst::kOLabelSorted |
      fst::kIDeterministic | fst::kODeterministic |
      fst::kAcceptor;

    ASSERT_EQ(EXPECTED_PROPERTIES, a.Properties(EXPECTED_PROPERTIES, true));
  }

  static void assert_arc(
      const irs::automaton::Arc& actual_arc,
      irs::automaton::Arc::Label expected_label,
      irs::automaton::StateId expected_target) {
    ASSERT_EQ(expected_label, actual_arc.ilabel);
    ASSERT_EQ(expected_label, actual_arc.olabel);
    ASSERT_EQ(expected_target, actual_arc.nextstate);
    ASSERT_EQ(fst::fsa::BooleanWeight(false), actual_arc.weight);
  }

  static void assert_state(
      const irs::automaton& a,
      const irs::automaton::StateId state,
      const std::vector<std::pair<irs::automaton::Arc::Label, irs::automaton::StateId>>& expected_arcs) {
    fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
    a.InitArcIterator(state, &actual_arcs);
    ASSERT_EQ(expected_arcs.size(), actual_arcs.narcs);

    auto* actual_arc = actual_arcs.arcs;
    for (auto& expected_arc : expected_arcs) {
      assert_arc(*actual_arc, expected_arc.first, expected_arc.second);
      ++actual_arc;
    }
  };

  static void assert_automaton(
      const irs::automaton& a,
      const std::vector<std::vector<std::pair<irs::automaton::Arc::Label, irs::automaton::StateId>>>& expected_automaton) {
    ASSERT_EQ(expected_automaton.size(), a.NumStates());
    irs::automaton::StateId state = 0;
    for (auto& expected_arcs : expected_automaton) {
      assert_state(a, state, expected_arcs);
      ++state;
    }
  }
};

TEST_F(utf8_transitions_builder_test, no_arcs) {
  irs::utf8_transitions_builder builder;

  // no default transition
  {
    irs::automaton a;
    auto start = a.AddState();

    std::vector<std::pair<irs::bytes_ref, irs::automaton::StateId>> arcs;
    builder.insert(a, start, fst::kNoStateId, arcs.begin(), arcs.end());

    assert_properties(a);
    ASSERT_EQ(1, a.NumStates());

    // arcs from start state
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(start, &actual_arcs);
      ASSERT_EQ(0, actual_arcs.narcs);
    }
  }

  // default transition
  {
    irs::automaton a;
    auto start = a.AddState();
    auto finish = a.AddState();
    auto intermediate0 = a.NumStates();
    auto intermediate1 = a.NumStates() + 1;
    auto intermediate2 = a.NumStates() + 2;

    std::vector<std::pair<irs::bytes_ref, irs::automaton::StateId>> arcs;
    builder.insert(a, start, finish, arcs.begin(), arcs.end());

    assert_properties(a);
    ASSERT_EQ(5, a.NumStates());

    // arcs from start state
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(start, &actual_arcs);
      ASSERT_EQ(256, actual_arcs.narcs);

      auto* actual_arc = actual_arcs.arcs;
      irs::automaton::Arc::Label label = 0;

      for (; label < 192; ++label) {
        assert_arc(*actual_arc, label, finish);
        ++actual_arc;
      }

      for (; label < 224; ++label) {
        assert_arc(*actual_arc, label, intermediate0);
        ++actual_arc;
      }

      for (; label < 240; ++label) {
        assert_arc(*actual_arc, label, intermediate1);
        ++actual_arc;
      }

      for (; label < 256; ++label) {
        assert_arc(*actual_arc, label, intermediate2);
        ++actual_arc;
      }

      ASSERT_EQ(actual_arc, actual_arcs.arcs + actual_arcs.narcs);
    }

    // arcs from 'intermediate0'
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(intermediate0, &actual_arcs);
      ASSERT_EQ(1, actual_arcs.narcs);
      assert_arc(actual_arcs.arcs[0], fst::fsa::kRho, finish);
    }

    // arcs from 'intermediate1'
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(intermediate1, &actual_arcs);
      ASSERT_EQ(1, actual_arcs.narcs);
      assert_arc(actual_arcs.arcs[0], fst::fsa::kRho, intermediate0);
    }

    // arcs from 'intermediate2'
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(intermediate2, &actual_arcs);
      ASSERT_EQ(1, actual_arcs.narcs);
      assert_arc(actual_arcs.arcs[0], fst::fsa::kRho, intermediate1);
    }

    // arcs from 'finish0'
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(finish, &actual_arcs);
      ASSERT_EQ(0, actual_arcs.narcs);
    }
  }
}

TEST_F(utf8_transitions_builder_test, empty_arc) {
  irs::utf8_transitions_builder builder;

  // no default transition
  {
    irs::automaton a;
    auto start = a.AddState();
    auto finish = a.AddState();

    std::vector<std::pair<irs::bytes_ref, irs::automaton::StateId>> arcs {
      { irs::bytes_ref::EMPTY, finish }
    };
    builder.insert(a, start, fst::kNoStateId, arcs.begin(), arcs.end());

    assert_properties(a);
    ASSERT_EQ(2, a.NumStates());

    // arcs from start state
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(start, &actual_arcs);
      ASSERT_EQ(0, actual_arcs.narcs);
    }

    // arcs from final state
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(finish, &actual_arcs);
      ASSERT_EQ(0, actual_arcs.narcs);
    }
  }

  // default transition
  {
    irs::automaton a;
    auto start = a.AddState();
    auto finish = a.AddState();
    auto intermediate0 = a.NumStates();
    auto intermediate1 = a.NumStates() + 1;
    auto intermediate2 = a.NumStates() + 2;

    std::vector<std::pair<irs::bytes_ref, irs::automaton::StateId>> arcs {
      { irs::bytes_ref::EMPTY, finish }
    };
    builder.insert(a, start, finish, arcs.begin(), arcs.end());

    assert_properties(a);
    ASSERT_EQ(5, a.NumStates());

    // arcs from start state
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(start, &actual_arcs);
      ASSERT_EQ(256, actual_arcs.narcs);

      auto* actual_arc = actual_arcs.arcs;
      irs::automaton::Arc::Label label = 0;

      for (; label < 192; ++label) {
        assert_arc(*actual_arc, label, finish);
        ++actual_arc;
      }

      for (; label < 224; ++label) {
        assert_arc(*actual_arc, label, intermediate0);
        ++actual_arc;
      }

      for (; label < 240; ++label) {
        assert_arc(*actual_arc, label, intermediate1);
        ++actual_arc;
      }

      for (; label < 256; ++label) {
        assert_arc(*actual_arc, label, intermediate2);
        ++actual_arc;
      }

      ASSERT_EQ(actual_arc, actual_arcs.arcs + actual_arcs.narcs);
    }

    // arcs from 'intermediate0'
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(intermediate0, &actual_arcs);
      ASSERT_EQ(1, actual_arcs.narcs);
      assert_arc(actual_arcs.arcs[0], fst::fsa::kRho, finish);
    }

    // arcs from 'intermediate1'
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(intermediate1, &actual_arcs);
      ASSERT_EQ(1, actual_arcs.narcs);
      assert_arc(actual_arcs.arcs[0], fst::fsa::kRho, intermediate0);
    }

    // arcs from 'intermediate2'
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(intermediate2, &actual_arcs);
      ASSERT_EQ(1, actual_arcs.narcs);
      assert_arc(actual_arcs.arcs[0], fst::fsa::kRho, intermediate1);
    }

    // arcs from 'finish0'
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(finish, &actual_arcs);
      ASSERT_EQ(0, actual_arcs.narcs);
    }
  }
}

TEST_F(utf8_transitions_builder_test, single_byte_sequence) {
  irs::utf8_transitions_builder builder;

  // no default transition
  {
    irs::automaton a;
    auto start = a.AddState();
    auto finish0 = a.AddState();
    auto finish1 = a.AddState();

    std::vector<std::pair<irs::bytes_ref, irs::automaton::StateId>> arcs;
    arcs.emplace_back(irs::ref_cast<irs::byte_type>(irs::string_ref("a")), finish0);
    arcs.emplace_back(irs::ref_cast<irs::byte_type>(irs::string_ref("0")), finish0);
    arcs.emplace_back(irs::ref_cast<irs::byte_type>(irs::string_ref("7")), finish1);
    arcs.emplace_back(irs::ref_cast<irs::byte_type>(irs::string_ref("U")), finish0);
    arcs.emplace_back(irs::ref_cast<irs::byte_type>(irs::string_ref("b")), finish1);
    std::sort(arcs.begin(), arcs.end());

    builder.insert(a, start, fst::kNoStateId, arcs.begin(), arcs.end());

    assert_properties(a);
    ASSERT_EQ(3, a.NumStates());

    // arcs from start state
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(start, &actual_arcs);
      ASSERT_EQ(arcs.size(), actual_arcs.narcs);

      auto* actual_arc = actual_arcs.arcs;
      for (auto& arc : arcs) {
        ASSERT_EQ(1, arc.first.size());
        assert_arc(*actual_arc, arc.first[0], arc.second);
        ++actual_arc;
      }
      ASSERT_EQ(actual_arc, actual_arcs.arcs + actual_arcs.narcs);
    }

    // arcs from 'finish0'
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(finish0, &actual_arcs);
      ASSERT_EQ(0, actual_arcs.narcs);
    }

    // arcs from 'finish1'
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(finish1, &actual_arcs);
      ASSERT_EQ(0, actual_arcs.narcs);
    }
  }

  // default transition
  {
    irs::automaton a;
    auto start = a.AddState();
    auto finish0 = a.AddState();
    auto finish1 = a.AddState();
    auto intermediate0 = a.NumStates();
    auto intermediate1 = a.NumStates() + 1;
    auto intermediate2 = a.NumStates() + 2;

    a.SetStart(start);

    std::vector<std::pair<irs::bytes_ref, irs::automaton::StateId>> arcs;
    arcs.emplace_back(irs::ref_cast<irs::byte_type>(irs::string_ref("a")), finish0);
    arcs.emplace_back(irs::ref_cast<irs::byte_type>(irs::string_ref("0")), finish0);
    arcs.emplace_back(irs::ref_cast<irs::byte_type>(irs::string_ref("7")), finish1);
    arcs.emplace_back(irs::ref_cast<irs::byte_type>(irs::string_ref("U")), finish0);
    arcs.emplace_back(irs::ref_cast<irs::byte_type>(irs::string_ref("b")), finish1);
    std::sort(arcs.begin(), arcs.end());

    builder.insert(a, start, finish1, arcs.begin(), arcs.end());

    assert_properties(a);
    ASSERT_EQ(6, a.NumStates()); // +3 intermediate states

    // arcs from start state
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(start, &actual_arcs);
      ASSERT_EQ(256, actual_arcs.narcs);

      auto* actual_arc = actual_arcs.arcs;
      auto expected_arc = arcs.begin();
      irs::automaton::Arc::Label label = 0;

      while (expected_arc != arcs.end() && label < 192) {
        for (; label < expected_arc->first[0]; ++label) {
          assert_arc(*actual_arc, label, finish1);
          ++actual_arc;
        }

        ASSERT_EQ(label, expected_arc->first[0]);
        assert_arc(*actual_arc, label, expected_arc->second);
        ++label;
        ++actual_arc;
        ++expected_arc;
      }

      for (; label < 192; ++label) {
        assert_arc(*actual_arc, label, finish1);
        ++actual_arc;
      }

      for (; label < 224; ++label) {
        assert_arc(*actual_arc, label, intermediate0);
        ++actual_arc;
      }

      for (; label < 240; ++label) {
        assert_arc(*actual_arc, label, intermediate1);
        ++actual_arc;
      }

      for (; label < 256; ++label) {
        assert_arc(*actual_arc, label, intermediate2);
        ++actual_arc;
      }

      ASSERT_EQ(actual_arc, actual_arcs.arcs + actual_arcs.narcs);
    }

    // arcs from 'intermediate0'
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(intermediate0, &actual_arcs);
      ASSERT_EQ(1, actual_arcs.narcs);
      assert_arc(actual_arcs.arcs[0], fst::fsa::kRho, finish1);
    }

    // arcs from 'intermediate1'
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(intermediate1, &actual_arcs);
      ASSERT_EQ(1, actual_arcs.narcs);
      assert_arc(actual_arcs.arcs[0], fst::fsa::kRho, intermediate0);
    }

    // arcs from 'intermediate2'
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(intermediate2, &actual_arcs);
      ASSERT_EQ(1, actual_arcs.narcs);
      assert_arc(actual_arcs.arcs[0], fst::fsa::kRho, intermediate1);
    }

    // arcs from 'finish0'
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(finish0, &actual_arcs);
      ASSERT_EQ(0, actual_arcs.narcs);
    }

    // arcs from 'finish1'
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(finish1, &actual_arcs);
      ASSERT_EQ(0, actual_arcs.narcs);
    }
  }
}

TEST_F(utf8_transitions_builder_test, multi_byte_sequence) {
  irs::utf8_transitions_builder builder;

  {
    irs::automaton a;
    auto start = a.AddState();
    a.SetStart(start);
    auto finish0 = a.AddState();
    auto finish1 = a.AddState();
    a.SetStart(start);

    std::vector<std::pair<irs::bytes_ref, irs::automaton::StateId>> arcs;
    arcs.emplace_back(irs::ref_cast<irs::byte_type>(irs::string_ref("a")), finish0);
    arcs.emplace_back(irs::ref_cast<irs::byte_type>(irs::string_ref("0")), finish0);
    arcs.emplace_back(irs::ref_cast<irs::byte_type>(irs::string_ref("7")), finish1);
    arcs.emplace_back(irs::ref_cast<irs::byte_type>(irs::string_ref("U")), finish0);
    arcs.emplace_back(irs::ref_cast<irs::byte_type>(irs::string_ref("b")), finish1);
    arcs.emplace_back(irs::ref_cast<irs::byte_type>(irs::string_ref("\xF5\x85\x97\x86")), finish1);
    arcs.emplace_back(irs::ref_cast<irs::byte_type>(irs::string_ref("\xFE\x85\x97\x86")), finish1);
    arcs.emplace_back(irs::ref_cast<irs::byte_type>(irs::string_ref("\xFF\x85\x97\x86")), finish1);
    arcs.emplace_back(irs::ref_cast<irs::byte_type>(irs::string_ref("\xFF\x85\x97\x86")), finish1);
    arcs.emplace_back(irs::ref_cast<irs::byte_type>(irs::string_ref("\xFF\x85\x97\x96")), finish1);
    arcs.emplace_back(irs::ref_cast<irs::byte_type>(irs::string_ref("\xD1\x90")), finish0);
    arcs.emplace_back(irs::ref_cast<irs::byte_type>(irs::string_ref("\xD1\x86")), finish0);
    arcs.emplace_back(irs::ref_cast<irs::byte_type>(irs::string_ref("b")), finish1);
    arcs.emplace_back(irs::ref_cast<irs::byte_type>(irs::string_ref("\xE2\x85\x96")), finish1);
    arcs.emplace_back(irs::ref_cast<irs::byte_type>(irs::string_ref("\xE2\x9E\x96")), finish1);
    arcs.emplace_back(irs::ref_cast<irs::byte_type>(irs::string_ref("\xE2\x9E\x96")), finish1);
    arcs.emplace_back(irs::ref_cast<irs::byte_type>(irs::string_ref("\xE2\x9E\x97")), finish1);
    arcs.emplace_back(irs::ref_cast<irs::byte_type>(irs::string_ref("\xE3\x9E\x97")), finish1);
    arcs.emplace_back(irs::ref_cast<irs::byte_type>(irs::string_ref("\xE3\x85\x97")), finish1);
    std::sort(arcs.begin(), arcs.end());

    builder.insert(a, start, fst::kNoStateId, arcs.begin(), arcs.end());
    assert_properties(a);

    std::vector<std::vector<std::pair<irs::automaton::Arc::Label, irs::automaton::StateId>>> expected_automaton {
      { { 48, 1 }, { 55, 2 }, { 85, 1 }, { 97, 1 },
        { 98, 2 }, { 209, 3 }, { 226, 6 }, { 227, 8 },
        { 245, 11 }, { 254, 11 }, { 255, 14 } },
      { },
      { },
      { { 134, 1 }, { 144, 1 } },
      { { 150, 2 } },
      { { 150, 2 }, { 151, 2 } },
      { { 133, 4 }, { 158, 5 } },
      { { 151, 2 } },
      { { 133, 7 }, { 158, 7 } },
      { { 134, 2 } },
      { { 151, 9 } },
      { { 133, 10 } },
      { { 134, 2 }, { 150, 2 } },
      { { 151, 12 } },
      { { 133, 13 } },
    };

    assert_automaton(a, expected_automaton);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                             utf8_emplace_arc_test
// -----------------------------------------------------------------------------

class utf8_emplace_arc_test : public test_base {
 protected:
  static void assert_properties(const irs::automaton& a) {
    constexpr auto EXPECTED_PROPERTIES =
      fst::kILabelSorted | fst::kOLabelSorted |
      fst::kIDeterministic | fst::kODeterministic |
      fst::kAcceptor;

    ASSERT_EQ(EXPECTED_PROPERTIES, a.Properties(EXPECTED_PROPERTIES, true));
  }

  static void assert_arc(
      const irs::automaton::Arc& actual_arc,
      irs::automaton::Arc::Label expected_label,
      irs::automaton::StateId expected_target) {
    ASSERT_EQ(expected_label, actual_arc.ilabel);
    ASSERT_EQ(expected_label, actual_arc.olabel);
    ASSERT_EQ(expected_target, actual_arc.nextstate);
    ASSERT_EQ(fst::fsa::BooleanWeight(false), actual_arc.weight);
  }

  static void assert_state(
      const irs::automaton& a,
      const irs::automaton::StateId state,
      const std::vector<std::pair<irs::automaton::Arc::Label, irs::automaton::StateId>>& expected_arcs) {
    fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
    a.InitArcIterator(state, &actual_arcs);
    ASSERT_EQ(expected_arcs.size(), actual_arcs.narcs);

    auto* actual_arc = actual_arcs.arcs;
    for (auto& expected_arc : expected_arcs) {
      assert_arc(*actual_arc, expected_arc.first, expected_arc.second);
      ++actual_arc;
    }
  };

  static void assert_automaton(
      const irs::automaton& a,
      const std::vector<std::vector<std::pair<irs::automaton::Arc::Label, irs::automaton::StateId>>>& expected_automaton) {
    ASSERT_EQ(expected_automaton.size(), a.NumStates());
    irs::automaton::StateId state = 0;
    for (auto& expected_arcs : expected_automaton) {
      assert_state(a, state, expected_arcs);
      ++state;
    }
  }
};

TEST_F(utf8_emplace_arc_test, emplace_arc_no_default_arc) {
   // 1-byte sequence
   {
     irs::automaton a;
     auto start = a.AddState();
     auto finish = a.AddState();
     a.SetStart(start);
     a.SetFinal(finish);

     const irs::string_ref label = "a";
     irs::utf8_emplace_arc(a, start, fst::kNoStateId, irs::ref_cast<irs::byte_type>(label), finish);

     const std::vector<std::vector<std::pair<irs::automaton::Arc::Label, irs::automaton::StateId>>> expected_automaton{
       { { 0x61, 1 } },
       { }
     };

     assert_properties(a);
     assert_automaton(a, expected_automaton);

     ASSERT_FALSE(irs::accept<char>(a, ""));
     ASSERT_TRUE(irs::accept<char>(a, "a"));
     ASSERT_FALSE(irs::accept<char>(a, "\xD0\xBF"));
     ASSERT_FALSE(irs::accept<char>(a, "\xE2\x9E\x96"));
     ASSERT_FALSE(irs::accept<char>(a, "\xF0\x9F\x98\x81"));
   }

   // 2-byte sequence
   {
     irs::automaton a;
     auto start = a.AddState();
     auto finish = a.AddState();
     a.SetStart(start);
     a.SetFinal(finish);

     const irs::string_ref label = "\xD0\xBF";
     irs::utf8_emplace_arc(a, start, fst::kNoStateId, irs::ref_cast<irs::byte_type>(label), finish);

     const std::vector<std::vector<std::pair<irs::automaton::Arc::Label, irs::automaton::StateId>>> expected_automaton{
       { { 0xD0, 2 } },
       { },
       { { 0xBF, 1 } },
     };

     assert_properties(a);
     assert_automaton(a, expected_automaton);

     ASSERT_FALSE(irs::accept<char>(a, ""));
     ASSERT_FALSE(irs::accept<char>(a, "a"));
     ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xD0\xBF"))));
     ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xE2\x9E\x96"))));
     ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xF0\x9F\x98\x81"))));
   }

   // 3-byte sequence
   {
     irs::automaton a;
     auto start = a.AddState();
     auto finish = a.AddState();
     a.SetStart(start);
     a.SetFinal(finish);

     const irs::string_ref label = "\xE2\x9E\x96";
     irs::utf8_emplace_arc(a, start, fst::kNoStateId, irs::ref_cast<irs::byte_type>(label), finish);

     const std::vector<std::vector<std::pair<irs::automaton::Arc::Label, irs::automaton::StateId>>> expected_automaton{
       { { 0xE2, 2 } },
       { },
       { { 0x9E, 3 } },
       { { 0x96, 1 } },
     };

     assert_properties(a);
     assert_automaton(a, expected_automaton);

     ASSERT_FALSE(irs::accept<char>(a, ""));
     ASSERT_FALSE(irs::accept<char>(a, "a"));
     ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xD0\xBF"))));
     ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xE2\x9E\x96"))));
     ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xF0\x9F\x98\x81"))));
   }

   // 4-byte sequence
   {
     irs::automaton a;
     auto start = a.AddState();
     auto finish = a.AddState();
     a.SetStart(start);
     a.SetFinal(finish);

     const irs::string_ref label = "\xF0\x9F\x98\x81";
     irs::utf8_emplace_arc(a, start, fst::kNoStateId, irs::ref_cast<irs::byte_type>(label), finish);

     const std::vector<std::vector<std::pair<irs::automaton::Arc::Label, irs::automaton::StateId>>> expected_automaton{
       { { 0xF0, 2 } },
       { },
       { { 0x9F, 3 } },
       { { 0x98, 4 } },
       { { 0x81, 1 } },
     };

     assert_properties(a);
     assert_automaton(a, expected_automaton);

     ASSERT_FALSE(irs::accept<char>(a, ""));
     ASSERT_FALSE(irs::accept<char>(a, "a"));
     ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xD0\xBF"))));
     ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xE2\x9E\x96"))));
     ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xF0\x9F\x98\x81"))));
   }
}

TEST_F(utf8_emplace_arc_test, emplace_arc_default_arc) {
  // 1-byte sequence
  {
    irs::automaton a;
    auto start = a.AddState();
    auto finish = a.AddState();
    auto def = a.AddState();
    a.SetStart(start);
    a.SetFinal(finish);
    a.SetFinal(def);
    auto intermediate0 = a.NumStates();
    auto intermediate1 = a.NumStates() + 1;
    auto intermediate2 = a.NumStates() + 2;

    const irs::string_ref label = "a";
    irs::utf8_emplace_arc(a, start, def, irs::ref_cast<irs::byte_type>(label), finish);
    ASSERT_EQ(6, a.NumStates());
    assert_properties(a);

    // arcs from start state
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(start, &actual_arcs);
      ASSERT_EQ(256, actual_arcs.narcs);

      irs::automaton::Arc::Label label = 0;
      auto* actual_arc = actual_arcs.arcs;

      for (; label < 'a'; ++label) {
        assert_arc(*actual_arc, label, def);
        ++actual_arc;
      }

      ASSERT_EQ('a', label);
      assert_arc(*actual_arc, label, finish);
      ++label;
      ++actual_arc;

      for (; label < 192; ++label) {
        assert_arc(*actual_arc, label, def);
        ++actual_arc;
      }

      for (; label < 224; ++label) {
        assert_arc(*actual_arc, label, intermediate0);
        ++actual_arc;
      }

      for (; label < 240; ++label) {
        assert_arc(*actual_arc, label, intermediate1);
        ++actual_arc;
      }

      for (; label < 256; ++label) {
        assert_arc(*actual_arc, label, intermediate2);
        ++actual_arc;
      }

      ASSERT_EQ(actual_arc, actual_arcs.arcs + actual_arcs.narcs);
    }

    // arcs from 'intermediate0'
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(intermediate0, &actual_arcs);
      ASSERT_EQ(1, actual_arcs.narcs);
      assert_arc(actual_arcs.arcs[0], fst::fsa::kRho, def);
    }

    // arcs from 'intermediate1'
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(intermediate1, &actual_arcs);
      ASSERT_EQ(1, actual_arcs.narcs);
      assert_arc(actual_arcs.arcs[0], fst::fsa::kRho, intermediate0);
    }

    // arcs from 'intermediate2'
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(intermediate2, &actual_arcs);
      ASSERT_EQ(1, actual_arcs.narcs);
      assert_arc(actual_arcs.arcs[0], fst::fsa::kRho, intermediate1);
    }

    // arcs from 'finish'
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(finish, &actual_arcs);
      ASSERT_EQ(0, actual_arcs.narcs);
    }

    // arcs from 'def'
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(def, &actual_arcs);
      ASSERT_EQ(0, actual_arcs.narcs);
    }

    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::bytes_ref::EMPTY));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("a"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xD0\xBF"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xE2\x9E\x96"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xF0\x9F\x98\x81"))));
  }

  // 2-byte sequence
  {
    irs::automaton a;
    auto start = a.AddState();
    auto finish = a.AddState();
    auto def = a.AddState();
    a.SetStart(start);
    a.SetFinal(finish);
    a.SetFinal(def);
    auto intermediate0 = a.NumStates();
    auto intermediate1 = a.NumStates() + 1;
    auto intermediate2 = a.NumStates() + 2;

    const irs::string_ref label = "\xD0\xBF";
    irs::utf8_emplace_arc(a, start, def, irs::ref_cast<irs::byte_type>(label), finish);
    ASSERT_EQ(7, a.NumStates());
    assert_properties(a);

    // arcs from start state
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(start, &actual_arcs);
      ASSERT_EQ(256, actual_arcs.narcs);

      irs::automaton::Arc::Label label = 0;
      auto* actual_arc = actual_arcs.arcs;

      for (; label < 192; ++label) {
        assert_arc(*actual_arc, label, def);
        ++actual_arc;
      }

      for (; label < 208; ++label) {
        assert_arc(*actual_arc, label, intermediate0);
        ++actual_arc;
      }

      ASSERT_EQ(208, label);
      assert_arc(*actual_arc, label, intermediate2+1);
      ++label;
      ++actual_arc;

      for (; label < 224; ++label) {
        assert_arc(*actual_arc, label, intermediate0);
        ++actual_arc;
      }

      for (; label < 240; ++label) {
        assert_arc(*actual_arc, label, intermediate1);
        ++actual_arc;
      }

      for (; label < 256; ++label) {
        assert_arc(*actual_arc, label, intermediate2);
        ++actual_arc;
      }

      ASSERT_EQ(actual_arc, actual_arcs.arcs + actual_arcs.narcs);
    }

    // arcs from 'intermediate2+1'
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(intermediate2+1, &actual_arcs);
      ASSERT_EQ(2, actual_arcs.narcs);
      assert_arc(actual_arcs.arcs[0], 191, finish);
      assert_arc(actual_arcs.arcs[1], fst::fsa::kRho, def);
    }

    // arcs from 'intermediate0'
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(intermediate0, &actual_arcs);
      ASSERT_EQ(1, actual_arcs.narcs);
      assert_arc(actual_arcs.arcs[0], fst::fsa::kRho, def);
    }

    // arcs from 'intermediate1'
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(intermediate1, &actual_arcs);
      ASSERT_EQ(1, actual_arcs.narcs);
      assert_arc(actual_arcs.arcs[0], fst::fsa::kRho, intermediate0);
    }

    // arcs from 'intermediate2'
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(intermediate2, &actual_arcs);
      ASSERT_EQ(1, actual_arcs.narcs);
      assert_arc(actual_arcs.arcs[0], fst::fsa::kRho, intermediate1);
    }

    // arcs from 'finish'
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(finish, &actual_arcs);
      ASSERT_EQ(0, actual_arcs.narcs);
    }

    // arcs from 'def'
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(def, &actual_arcs);
      ASSERT_EQ(0, actual_arcs.narcs);
    }

    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::bytes_ref::EMPTY));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("a"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xD0\xBF"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xE2\x9E\x96"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xF0\x9F\x98\x81"))));
  }

  // 3-byte sequence
  {
    irs::automaton a;
    auto start = a.AddState();
    auto finish = a.AddState();
    auto def = a.AddState();
    a.SetStart(start);
    a.SetFinal(finish);
    a.SetFinal(def);
    auto intermediate0 = a.NumStates();
    auto intermediate1 = a.NumStates() + 1;
    auto intermediate2 = a.NumStates() + 2;

    const irs::string_ref label = "\xE2\x9E\x96";
    irs::utf8_emplace_arc(a, start, def, irs::ref_cast<irs::byte_type>(label), finish);
    ASSERT_EQ(8, a.NumStates());
    assert_properties(a);

    // arcs from start state
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(start, &actual_arcs);
      ASSERT_EQ(256, actual_arcs.narcs);

      irs::automaton::Arc::Label label = 0;
      auto* actual_arc = actual_arcs.arcs;

      for (; label < 192; ++label) {
        assert_arc(*actual_arc, label, def);
        ++actual_arc;
      }

      for (; label < 224; ++label) {
        assert_arc(*actual_arc, label, intermediate0);
        ++actual_arc;
      }

      for (; label < 226; ++label) {
        assert_arc(*actual_arc, label, intermediate1);
        ++actual_arc;
      }

      ASSERT_EQ(226, label);
      assert_arc(*actual_arc, label, intermediate2+1);
      ++label;
      ++actual_arc;

      for (; label < 240; ++label) {
        assert_arc(*actual_arc, label, intermediate1);
        ++actual_arc;
      }

      for (; label < 256; ++label) {
        assert_arc(*actual_arc, label, intermediate2);
        ++actual_arc;
      }

      ASSERT_EQ(actual_arc, actual_arcs.arcs + actual_arcs.narcs);
    }

    // arcs from 'intermediate2+1'
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(intermediate2+1, &actual_arcs);
      ASSERT_EQ(2, actual_arcs.narcs);
      assert_arc(actual_arcs.arcs[0], 158, intermediate2+2);
      assert_arc(actual_arcs.arcs[1], fst::fsa::kRho, intermediate0);
    }

    // arcs from 'intermediate2+2'
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(intermediate2+2, &actual_arcs);
      ASSERT_EQ(2, actual_arcs.narcs);
      assert_arc(actual_arcs.arcs[0], 150, finish);
      assert_arc(actual_arcs.arcs[1], fst::fsa::kRho, def);
    }

    // arcs from 'intermediate0'
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(intermediate0, &actual_arcs);
      ASSERT_EQ(1, actual_arcs.narcs);
      assert_arc(actual_arcs.arcs[0], fst::fsa::kRho, def);
    }

    // arcs from 'intermediate1'
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(intermediate1, &actual_arcs);
      ASSERT_EQ(1, actual_arcs.narcs);
      assert_arc(actual_arcs.arcs[0], fst::fsa::kRho, intermediate0);
    }

    // arcs from 'intermediate2'
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(intermediate2, &actual_arcs);
      ASSERT_EQ(1, actual_arcs.narcs);
      assert_arc(actual_arcs.arcs[0], fst::fsa::kRho, intermediate1);
    }

    // arcs from 'finish'
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(finish, &actual_arcs);
      ASSERT_EQ(0, actual_arcs.narcs);
    }

    // arcs from 'def'
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(def, &actual_arcs);
      ASSERT_EQ(0, actual_arcs.narcs);
    }

    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::bytes_ref::EMPTY));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("a"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xD0\xBF"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xE2\x9E\x96"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xF0\x9F\x98\x81"))));
  }

  // 4-byte sequence
  {
    irs::automaton a;
    auto start = a.AddState();
    auto finish = a.AddState();
    auto def = a.AddState();
    a.SetStart(start);
    a.SetFinal(finish);
    a.SetFinal(def);
    auto intermediate0 = a.NumStates();
    auto intermediate1 = a.NumStates() + 1;
    auto intermediate2 = a.NumStates() + 2;

    const irs::string_ref label = "\xF0\x9F\x98\x81";
    irs::utf8_emplace_arc(a, start, def, irs::ref_cast<irs::byte_type>(label), finish);
    ASSERT_EQ(9, a.NumStates());
    assert_properties(a);

    // arcs from start state
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(start, &actual_arcs);
      ASSERT_EQ(256, actual_arcs.narcs);

      irs::automaton::Arc::Label label = 0;
      auto* actual_arc = actual_arcs.arcs;

      for (; label < 192; ++label) {
        assert_arc(*actual_arc, label, def);
        ++actual_arc;
      }

      for (; label < 224; ++label) {
        assert_arc(*actual_arc, label, intermediate0);
        ++actual_arc;
      }

      for (; label < 240; ++label) {
        assert_arc(*actual_arc, label, intermediate1);
        ++actual_arc;
      }

      ASSERT_EQ(240, label);
      assert_arc(*actual_arc, label, intermediate2+1);
      ++label;
      ++actual_arc;

      for (; label < 256; ++label) {
        assert_arc(*actual_arc, label, intermediate2);
        ++actual_arc;
      }

      ASSERT_EQ(actual_arc, actual_arcs.arcs + actual_arcs.narcs);
    }

    // arcs from 'intermediate2+1'
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(intermediate2+1, &actual_arcs);
      ASSERT_EQ(2, actual_arcs.narcs);
      assert_arc(actual_arcs.arcs[0], 159, intermediate2+2);
      assert_arc(actual_arcs.arcs[1], fst::fsa::kRho, intermediate1);
    }

    // arcs from 'intermediate2+2'
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(intermediate2+2, &actual_arcs);
      ASSERT_EQ(2, actual_arcs.narcs);
      assert_arc(actual_arcs.arcs[0], 152, intermediate2+3);
      assert_arc(actual_arcs.arcs[1], fst::fsa::kRho, intermediate0);
    }

    // arcs from 'intermediate2+3'
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(intermediate2+3, &actual_arcs);
      ASSERT_EQ(2, actual_arcs.narcs);
      assert_arc(actual_arcs.arcs[0], 129, finish);
      assert_arc(actual_arcs.arcs[1], fst::fsa::kRho, def);
    }

    // arcs from 'intermediate0'
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(intermediate0, &actual_arcs);
      ASSERT_EQ(1, actual_arcs.narcs);
      assert_arc(actual_arcs.arcs[0], fst::fsa::kRho, def);
    }

    // arcs from 'intermediate1'
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(intermediate1, &actual_arcs);
      ASSERT_EQ(1, actual_arcs.narcs);
      assert_arc(actual_arcs.arcs[0], fst::fsa::kRho, intermediate0);
    }

    // arcs from 'intermediate2'
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(intermediate2, &actual_arcs);
      ASSERT_EQ(1, actual_arcs.narcs);
      assert_arc(actual_arcs.arcs[0], fst::fsa::kRho, intermediate1);
    }

    // arcs from 'finish'
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(finish, &actual_arcs);
      ASSERT_EQ(0, actual_arcs.narcs);
    }

    // arcs from 'def'
    {
      fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
      a.InitArcIterator(def, &actual_arcs);
      ASSERT_EQ(0, actual_arcs.narcs);
    }

    ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::bytes_ref::EMPTY));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("a"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xD0\xBF"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xE2\x9E\x96"))));
    ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xF0\x9F\x98\x81"))));
  }
}

TEST_F(utf8_emplace_arc_test, emplace_arc_rho_arc) {
   irs::automaton a;
   auto start = a.AddState();
   auto finish = a.AddState();
   a.SetStart(start);
   a.SetFinal(finish);
   auto intermediate0 = a.NumStates();
   auto intermediate1 = a.NumStates() + 1;
   auto intermediate2 = a.NumStates() + 2;
   irs::utf8_emplace_rho_arc(a, start, finish);

   assert_properties(a);

   {
     fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
     a.InitArcIterator(start, &actual_arcs);
     ASSERT_EQ(256, actual_arcs.narcs);

     auto* actual_arc = actual_arcs.arcs;
     irs::automaton::Arc::Label label = 0;

     for (; label < 192; ++label) {
       assert_arc(*actual_arc, label, finish);
       ++actual_arc;
     }

     for (; label < 224; ++label) {
       assert_arc(*actual_arc, label, intermediate0);
       ++actual_arc;
     }

     for (; label < 240; ++label) {
       assert_arc(*actual_arc, label, intermediate1);
       ++actual_arc;
     }

     for (; label < 256; ++label) {
       assert_arc(*actual_arc, label, intermediate2);
       ++actual_arc;
     }

     ASSERT_EQ(actual_arc, actual_arcs.arcs + actual_arcs.narcs);
   }

   // arcs from 'intermediate0'
   {
     fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
     a.InitArcIterator(intermediate0, &actual_arcs);
     ASSERT_EQ(1, actual_arcs.narcs);
     assert_arc(actual_arcs.arcs[0], fst::fsa::kRho, finish);
   }

   // arcs from 'intermediate1'
   {
     fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
     a.InitArcIterator(intermediate1, &actual_arcs);
     ASSERT_EQ(1, actual_arcs.narcs);
     assert_arc(actual_arcs.arcs[0], fst::fsa::kRho, intermediate0);
   }

   // arcs from 'intermediate2'
   {
     fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
     a.InitArcIterator(intermediate2, &actual_arcs);
     ASSERT_EQ(1, actual_arcs.narcs);
     assert_arc(actual_arcs.arcs[0], fst::fsa::kRho, intermediate1);
   }

   // arcs from 'finish0'
   {
     fst::ArcIteratorData<irs::automaton::Arc> actual_arcs;
     a.InitArcIterator(finish, &actual_arcs);
     ASSERT_EQ(0, actual_arcs.narcs);
   }

   ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::bytes_ref::EMPTY));
   ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("a"))));
   ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xD0\xBF"))));
   ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xE2\x9E\x96"))));
   ASSERT_TRUE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xF0\x9F\x98\x81"))));
   ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xD0\xBF\xD0\xBF"))));
   ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xE2\x9E\x96\xD0\xBF"))));
   ASSERT_FALSE(irs::accept<irs::byte_type>(a, irs::ref_cast<irs::byte_type>(irs::string_ref("\xF0\x9F\x98\x81\xD0\xBF"))));
}
