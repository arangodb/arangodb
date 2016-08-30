////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "DatafileStatisticsContainer.h"

using namespace arangodb;

/// @brief create an empty datafile statistics container
DatafileStatisticsContainer::DatafileStatisticsContainer()
    : numberAlive(0),
      numberDead(0),
      numberDeletions(0),
      sizeAlive(0),
      sizeDead(0),
      numberUncollected(0) {}

/// @brief update statistics from another container
void DatafileStatisticsContainer::update(
    DatafileStatisticsContainer const& other) {
  numberAlive += other.numberAlive;
  numberDead += other.numberDead;
  numberDeletions += other.numberDeletions;
  sizeAlive += other.sizeAlive;
  sizeDead += other.sizeDead;
  numberUncollected += other.numberUncollected;
}

/// @brief flush the statistics values
void DatafileStatisticsContainer::reset() {
  numberAlive = 0;
  numberDead = 0;
  numberDeletions = 0;
  sizeAlive = 0;
  sizeDead = 0;
  numberUncollected = 0;
}
