////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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

#include "HotBackupFeature.h"

#include "Basics/Result.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"

#include <mutex>

using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;
using namespace arangodb;

namespace arangodb {

HotBackupFeature::HotBackupFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "HotBackup") {
  setOptional(true);
  startsAfter("DatabasePhase");
}

HotBackupFeature::~HotBackupFeature() {}

void HotBackupFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {

}

void HotBackupFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {}

void HotBackupFeature::prepare() {}

void HotBackupFeature::start() {}

void HotBackupFeature::beginShutdown() {
  // Call off uploads / downloads?
}

void HotBackupFeature::stop() {}

void HotBackupFeature::unprepare() {}

arangodb::Result HotBackupFeature::createTransferRecord (
  std::string const& operation, std::string const& remote, std::string& id) {
                                    
  std::lock_guard<std::mutex> guard(_clipBoardMutex);
  if (_clipBoard.find({operation,remote}) == _clipBoard.end()) {
    _clipBoard[{operation,remote}].push_back("CREATED"); 
  } 
  return arangodb::Result();  
}

arangodb::Result HotBackupFeature::noteTransferRecord (
  std::string const& operation, std::string const& backupId,
  std::string const& transferId, std::string const& status,
  std::string const& remote) {
  
  std::lock_guard<std::mutex> guard(_clipBoardMutex);
  auto const& t = _index.find(transferId);
  
  if (t != _index.end()) {
    auto const& back = _clipBoard.at(t->second).back();
    if (back != "COMPLETED" && back != "FAILED") {
      _clipBoard.at(t->second).push_back(status);
    } else {
      return arangodb::Result(
        TRI_ERROR_HTTP_FORBIDDEN,
        std::string("Transfer with id ") + id + " has already been completed");
    }
  } else {
    if (!remote.empty()) {
      createTransferRecord(operation, remote, );
    }
  }
  
  return arangodb::Result();
  
}

arangodb::Result HotBackupFeature::getTransferRecord(
  std::string const& id, std::vector<std::vector<std::string>>& reports) {

  if (!reports.empty()) {
    reports.clear();
  }

  std::lock_guard<std::mutex> guard(_clipBoardMutex);

  if (id.empty()) {
    reports.resize(_index.size());
    size_t n = 0;
    for (auto const t : _index) {
      auto& report = reports[n];
      auto const& clip = _clipBoard.at(t.second);
      auto const& transfer = t.second;
      report.push_back(std::string("operation: ") + transfer.operation);
      report.push_back(std::string("remote: ") + transfer.remote);
      std::copy (clip.begin(),clip.end(),back_inserter(report));
    }
  } else {
    auto const& t = _index.find(id);
    reports.resize(1);
    auto& report = reports.front();
    if (t != _index.end()) {
      auto const& clip = _clipBoard.at(t->second);
      auto const& transfer = t->second;
      report.push_back(std::string("operation: ") + transfer.operation);
      report.push_back(std::string("remote: ") + transfer.remote);
      std::copy (clip.begin(),clip.end(),back_inserter(report));
    } else {
      return arangodb::Result(
        TRI_ERROR_HTTP_NOT_FOUND, std::string("No transfer with id ") + id);
    }
  }
  return arangodb::Result();
}


}  // namespaces


SD::SD() : hash(0) {}

SD::SD(std::string const& b, std::string const& s, std::string const& d) :
  backupId(b), remote(d), hash(SD::hash_it(operation,remote)) {}

SD::SD(std::string&& b, std::string&& s, std::string&& d) :
  backupId(std::move(b)), operation(std::move(s)), remote(std::move(d)),
  hash(SD::hash_it(s,d)) {}

SD::SD(std::initializer_list<std::string> l) {
  TRI_ASSERT(l.size()==3);
  auto const it = l.begin();
  backupId  = *(it++);
  operation = *(it++);
  remote    = *(it  );
}

std::hash<std::string>::result_type SD::hash_it(
  std::string const& s, std::string const& d) {
  return std::hash<std::string>{}(s) ^ (std::hash<std::string>{}(d) << 1);
}

bool operator< (SD const& x, SD const& y) {
  return x.hash < y.hash;
}

namespace std {
ostream& operator<< (ostream& o, SD const& sd) {
  o << sd.operation << ":" << sd.remote;
  return o;
}}
