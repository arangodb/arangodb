////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_STATISTICS_DESCRIPTIONS_H
#define ARANGOD_STATISTICS_DESCRIPTIONS_H 1

#include "Basics/Common.h"

#include <velocypack/Builder.h>
#include <string>

namespace arangodb {
namespace application_features {
class ApplicationServer;
}
namespace stats {

enum RequestStatisticsSource {
  USER,
  SUPERUSER,
  ALL
};

enum class GroupType { System, Client, ClientUser, Http, Vst, Server };

std::string fromGroupType(stats::GroupType);

struct Group {
  stats::GroupType type;
  std::string name;
  std::string description;

 public:
  void toVPack(velocypack::Builder&) const;
};

enum class FigureType : char { Current, Accumulated, Distribution };

std::string fromFigureType(stats::FigureType);

enum class Unit : char { Seconds, Bytes, Percent, Number };

std::string fromUnit(stats::Unit);

struct Figure {
  stats::GroupType groupType;
  std::string identifier;
  std::string name;
  std::string description;
  stats::FigureType type;
  stats::Unit units;

  std::vector<double> cuts;

 public:
  void toVPack(velocypack::Builder&) const;
};

class Descriptions final {
 public:
  explicit Descriptions(application_features::ApplicationServer&);

  std::vector<stats::Group> const& groups() const { return _groups; }

  std::vector<stats::Figure> const& figures() const { return _figures; }

  void serverStatistics(velocypack::Builder&) const;
  void clientStatistics(velocypack::Builder&, RequestStatisticsSource source) const;
  void httpStatistics(velocypack::Builder&) const;
  void processStatistics(velocypack::Builder&) const;

 private:
  application_features::ApplicationServer& _server;

  std::vector<double> _requestTimeCuts;
  std::vector<double> _connectionTimeCuts;
  std::vector<double> _bytesSendCuts;
  std::vector<double> _bytesReceivedCuts;

  std::vector<stats::Group> _groups;
  std::vector<stats::Figure> _figures;
};
}  // namespace stats
}  // namespace arangodb

#endif
