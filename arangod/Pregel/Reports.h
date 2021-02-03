////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_PREGEL_REPORTS_H
#define ARANGODB_PREGEL_REPORTS_H
#include <sstream>

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>
#include "Pregel/Graph.h"

namespace arangodb::pregel {

enum class ReportLevel { DEBUG, INFO, WARN, ERR };
std::string to_string(ReportLevel);

using ReportAnnotations = std::unordered_map<std::string, VPackBuilder>;

struct Report {
  std::string message;
  ReportLevel level;
  ReportAnnotations annotations;

  bool isError() const { return level == ReportLevel::ERR; }

  void intoBuilder(VPackBuilder& builder) const;
  static Report fromVelocyPack(VPackSlice slice);
};

struct ReportManager;

struct ReportBuilder {
  template <typename T>
  ReportBuilder& operator<<(T&& t) {
    ss << t;
    return *this;
  }

  template <typename T>
  ReportBuilder& with(std::string_view name, T&& value) {
    VPackBuilder builder;
    using base_type = std::decay_t<T>;
    if constexpr (std::is_same_v<VPackSlice, base_type>) {
      builder.add(value);
    } else if constexpr (std::is_same_v<PregelID, base_type>) {
      VPackObjectBuilder ob(&builder);
      builder.add("key", VPackValue(value.key));
      builder.add("shard", VPackValue(value.shard));
    } else {
      builder.add(VPackValue(value));
    }
    annotations.emplace(name, std::move(builder));
    return *this;
  }

  ~ReportBuilder();

 private:
  friend struct ReportManager;
  ReportBuilder(ReportManager& manager, ReportLevel lvl);

  std::stringstream ss;
  ReportLevel level;
  ReportManager& manager;
  ReportAnnotations annotations;
};

struct ReportManager {
  auto report(ReportLevel level) -> ReportBuilder;

  void append(ReportManager other);
  void append(Report report);
  void clear();

  void appendFromSlice(VPackSlice slice);
  void intoBuilder(VPackBuilder& builder) const;
  std::size_t getNumErrors() const { return _numErrors; }

 private:
  friend struct ReportBuilder;

  std::size_t _numErrors = 0;
  std::vector<Report> _reports;
};
}  // namespace arangodb::pregel

#endif
