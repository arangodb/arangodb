////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "RestServer/counter.h"

#include <iosfwd>
#include <string_view>

namespace arangodb::metrics {

class Metric {
 public:
  Metric(std::string_view name, std::string_view help,
         std::string_view labels) noexcept;

  [[nodiscard]] std::string_view help() const noexcept;
  [[nodiscard]] std::string_view name() const noexcept;
  [[nodiscard]] std::string_view labels() const noexcept;

  virtual std::string_view type() const noexcept = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// \param result toPrometheus handler response
  /// \param first is the first call toPrometheus for a metric with that name,
  ///              so you should write type and help string
  /// \param globals labels that all metrics have
  //////////////////////////////////////////////////////////////////////////////
  virtual void toPrometheus(std::string& result, bool first,
                            std::string_view globals) const = 0;

  virtual ~Metric();

 protected:
  void addHelpType(std::string& result) const;
  void addName(std::string& result, std::string_view globals) const;

 private:
  std::string_view _name;
  std::string _help;
  std::string _labels;
};

using CounterType =
    gcl::counter::simplex<uint64_t, gcl::counter::atomicity::full>;
using HistType =
    gcl::counter::simplex_array<uint64_t, gcl::counter::atomicity::full>;
using BufferType = gcl::counter::buffer<uint64_t, gcl::counter::atomicity::full,
                                        gcl::counter::atomicity::full>;

std::ostream& operator<<(std::ostream& output, CounterType const& s);
std::ostream& operator<<(std::ostream& output, HistType const& v);

}  // namespace arangodb::metrics
