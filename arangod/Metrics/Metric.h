////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#include <string>

namespace arangodb::velocypack {

class Builder;

}  // namespace arangodb::velocypack
namespace arangodb::metrics {

class Metric {
 public:
  static void addInfo(std::string& result, std::string_view name,
                      std::string_view help, std::string_view type);
  static void addMark(std::string& result, std::string_view name,
                      std::string_view globals, std::string_view labels);

  Metric(std::string_view name, std::string_view help, std::string_view labels,
         bool dynamic = false);

  virtual ~Metric();

  [[nodiscard]] std::string_view help() const noexcept;
  [[nodiscard]] std::string_view name() const noexcept;
  [[nodiscard]] std::string_view labels() const noexcept;

  [[nodiscard]] virtual std::string_view type() const noexcept = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// \param result toPrometheus handler response
  /// \param globals labels that all metrics have
  //////////////////////////////////////////////////////////////////////////////
  virtual void toPrometheus(std::string& result, std::string_view globals,
                            bool ensureWhitespace) const = 0;
  virtual void toVPack(velocypack::Builder& builder) const = 0;

  void setDynamic() noexcept { _dynamic = true; }

  bool isDynamic() const noexcept { return _dynamic; }

 private:
  std::string_view _name;
  std::string _help;
  std::string _labels;
  bool _dynamic;
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
