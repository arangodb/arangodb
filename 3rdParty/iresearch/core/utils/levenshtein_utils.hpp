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

#ifndef IRESEARCH_LEVENSHTEIN_UTILS_H
#define IRESEARCH_LEVENSHTEIN_UTILS_H

#include <vector>
#include <numeric>

#include "string.hpp"
#include "range.hpp"
#include "automaton_decl.hpp"

namespace iresearch {

struct data_output;
struct data_input;

template<typename T, size_t SubstCost = 1>
inline size_t edit_distance(const T* lhs, size_t lhs_size,
                            const T* rhs, size_t rhs_size) {
  assert(lhs || !lhs_size);
  assert(rhs || !rhs_size);

  if (lhs_size > rhs_size) {
    std::swap(lhs, rhs);
    std::swap(lhs_size, rhs_size);
  }

  std::vector<size_t> cost(2*(lhs_size + 1));

  auto current = cost.begin();
  auto next = cost.begin() + cost.size()/2;
  std::iota(current, next, 0);

  for (size_t j = 1; j <= rhs_size; ++j) {
    next[0] = j;
    for (size_t i = 1; i <= lhs_size; ++i) {
      next[i] = std::min({
        next[i-1]    + 1,                                     // deletion
        current[i]   + 1,                                     // insertion
        current[i-1] + (lhs[i-1] == rhs[j-1] ? 0 : SubstCost) // substitution
      });
    }
    std::swap(next, current);
  }

  return current[lhs_size];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief evaluates edit distance between the specified words
/// @param lhs string to compare
/// @param rhs string to compare
/// @returns edit distance
////////////////////////////////////////////////////////////////////////////////
template<typename Char>
inline size_t edit_distance(const basic_string_ref<Char>& lhs,
                            const basic_string_ref<Char>& rhs) {
  return edit_distance(lhs.begin(), lhs.size(), rhs.begin(), rhs.size());
}

// -----------------------------------------------------------------------------
// --SECTION--
//
// Implementation of the algorithm of building Levenshtein automaton
// by Klaus Schulz, Stoyan Mihov described in
//   http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.16.652
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @class parametric_description
/// @brief Parametric description of automaton for a particular edit distance.
///        Once created the description can be used for generating DFAs
///        accepting strings at edit distance less or equal than distance
///        specified in description.
////////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API parametric_description {
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief describes trasition among parametric states
  ///        first - desination parametric state id
  ///        second - offset
  //////////////////////////////////////////////////////////////////////////////
  typedef std::pair<uint32_t, uint32_t> transition_t;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief theoretically max possible distance we can evaluate, not really
  ///        feasible due to exponential growth of parametric description size
  //////////////////////////////////////////////////////////////////////////////
  static constexpr byte_type MAX_DISTANCE = 31;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief creates default "invalid" description
  //////////////////////////////////////////////////////////////////////////////
  parametric_description() = default;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief creates description
  //////////////////////////////////////////////////////////////////////////////
  parametric_description(
      std::vector<transition_t>&& transitions,
      std::vector<byte_type>&& distance,
      byte_type max_distance) noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @return transition from 'from' state matching a provided
  ///         characteristic vector
  //////////////////////////////////////////////////////////////////////////////
  const transition_t& transition(size_t from, uint64_t chi) const noexcept {
    assert(from*chi_max_ + chi < transitions_.size());
    return transitions_[from*chi_max_ + chi];
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @return parametric transitions table
  //////////////////////////////////////////////////////////////////////////////
  range<const transition_t> transitions() const noexcept {
    return { transitions_.data(), transitions_.size() };
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @return edit distance of parametric state at a specified offset
  //////////////////////////////////////////////////////////////////////////////
  byte_type distance(size_t state, size_t offset) const noexcept {
     if (offset >= chi_size_) {
       return max_distance_ + 1;
     }

     assert(state*chi_size_ + offset < distance_.size());
     return distance_[state*chi_size_ + offset];
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @return range of edit distances for all parametric states
  //////////////////////////////////////////////////////////////////////////////
  range<const byte_type> distances() const noexcept {
    return { distance_.data(), distance_.size() };
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @return number of states in parametric description
  //////////////////////////////////////////////////////////////////////////////
  size_t size() const noexcept { return num_states_; }

  //////////////////////////////////////////////////////////////////////////////
  /// @return length of characteristic vector
  /// @note 2*max_distance_ + 1
  //////////////////////////////////////////////////////////////////////////////
  uint64_t chi_size() const noexcept { return chi_size_; }

  //////////////////////////////////////////////////////////////////////////////
  /// @return max value of characteristic vector
  /// @note 1 << chi_size_
  //////////////////////////////////////////////////////////////////////////////
  uint64_t chi_max() const noexcept { return chi_max_; }

  //////////////////////////////////////////////////////////////////////////////
  /// @return the edit distance for which this description was built
  //////////////////////////////////////////////////////////////////////////////
  byte_type max_distance() const noexcept { return max_distance_; }

  //////////////////////////////////////////////////////////////////////////////
  /// @return true if description is valid, false otherwise
  //////////////////////////////////////////////////////////////////////////////
  explicit operator bool() const noexcept { return chi_size_ > 0; }

  //////////////////////////////////////////////////////////////////////////////
  /// @return true if description is equal to a specified one
  //////////////////////////////////////////////////////////////////////////////
  bool operator==(const parametric_description& rhs) const noexcept {
    // all other members are derived
    return transitions_ == rhs.transitions_
        && distance_ == rhs.distance_
        && max_distance_ == rhs.max_distance_;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @return true if description is not equal to a specified one
  //////////////////////////////////////////////////////////////////////////////
  bool operator!=(const parametric_description& rhs) const noexcept {
    return !(*this == rhs);
  }

 private:
  std::vector<transition_t> transitions_; // transition table
  std::vector<byte_type> distance_;       // distances per state and offset
  uint64_t chi_size_{};                   // 2*max_distance_+1
  uint64_t chi_max_{};                    // 1 << chi_size
  size_t num_states_{};                   // number of parametric states
  byte_type max_distance_{};              // max allowed distance
}; // parametric_description

////////////////////////////////////////////////////////////////////////////////
/// @brief builds parametric description of Levenshtein automaton
/// @param max_distance maximum allowed distance
/// @param with_transposition count transpositions
/// @returns parametric description of Levenshtein automaton for supplied args
////////////////////////////////////////////////////////////////////////////////
IRESEARCH_API parametric_description make_parametric_description(
  byte_type max_distance,
  bool with_transposition);

////////////////////////////////////////////////////////////////////////////////
/// @brief writes parametric description to a specified output stream
/// @param description parametric description to write
/// @param out output stream
////////////////////////////////////////////////////////////////////////////////
IRESEARCH_API void write(const parametric_description& description,
                         data_output& out);

////////////////////////////////////////////////////////////////////////////////
/// @brief read parametric description from a specified input stream
/// @param in input stream
/// @returns the read parametric description
////////////////////////////////////////////////////////////////////////////////
IRESEARCH_API parametric_description read(data_input& in);

////////////////////////////////////////////////////////////////////////////////
/// @brief instantiates DFA based on provided parametric description and target
/// @param description parametric description
/// @param target valid UTF-8 encoded string
/// @returns DFA
/// @note if 'target' isn't a valid UTF-8 sequence, behaviour is undefined
////////////////////////////////////////////////////////////////////////////////
IRESEARCH_API automaton make_levenshtein_automaton(
  const parametric_description& description,
  const bytes_ref& target);

////////////////////////////////////////////////////////////////////////////////
/// @brief evaluates edit distance between the specified words up to
///        specified in description.max_distance
/// @param description parametric description
/// @param lhs string to compare (utf8 encoded)
/// @param lhs_size size of the string to comprare
/// @param rhs string to compare (utf8 encoded)
/// @param rhs_size size of the string to comprare
/// @returns edit_distance up to specified in description.max_distance
/// @note accepts only valid descriptions, calling function with
///       invalid description is undefined behaviour
////////////////////////////////////////////////////////////////////////////////
IRESEARCH_API size_t edit_distance(
  const parametric_description& description,
  const byte_type* lhs, size_t lhs_size,
  const byte_type* rhs, size_t rhs_size);

////////////////////////////////////////////////////////////////////////////////
/// @brief evaluates edit distance between the specified words up to
///        specified in description.max_distance
/// @param description parametric description
/// @param lhs string to compare (utf8 encoded)
/// @param rhs string to compare (utf8 encoded)
/// @returns edit_distance up to specified in description.max_distance
/// @note accepts only valid descriptions, calling function with
///       invalid description is undefined behaviour
////////////////////////////////////////////////////////////////////////////////
inline size_t edit_distance(
    const parametric_description& description,
    const bytes_ref& lhs,
    const bytes_ref& rhs) {
  return edit_distance(description, lhs.begin(), lhs.size(), rhs.begin(), rhs.size());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief evaluates edit distance between the specified words up to
///        specified in description.max_distance.
/// @param evaluated edit distance
/// @param description parametric description
/// @param lhs string to compare (utf8 encoded)
/// @param lhs_size size of the string to comprare
/// @param rhs string to compare (utf8 encoded)
/// @param rhs_size size of the string to comprare
/// @returns true if both lhs_string and rhs_strign are valid UTF-8 sequences,
///          false - otherwise
/// @note accepts only valid descriptions, calling function with
///       invalid description is undefined behaviour
////////////////////////////////////////////////////////////////////////////////
IRESEARCH_API bool edit_distance(
  size_t& distance,
  const parametric_description& description,
  const byte_type* lhs, size_t lhs_size,
  const byte_type* rhs, size_t rhs_size);

////////////////////////////////////////////////////////////////////////////////
/// @brief evaluates edit distance between the specified words up to
///        specified in description.max_distance
/// @param description parametric description
/// @param lhs string to compare (utf8 encoded)
/// @param rhs string to compare (utf8 encoded)
/// @returns true if both lhs_string and rhs_strign are valid UTF-8 sequences,
///          false - otherwise
/// @note accepts only valid descriptions, calling function with
///       invalid description is undefined behaviour
////////////////////////////////////////////////////////////////////////////////
inline bool edit_distance(
    size_t& distance,
    const parametric_description& description,
    const bytes_ref& lhs,
    const bytes_ref& rhs) {
  return edit_distance(distance, description,
                       lhs.begin(), lhs.size(),
                       rhs.begin(), rhs.size());
}

}

#endif // IRESEARCH_LEVENSHTEIN_UTILS_H

