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

void HotBackupFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {}

void HotBackupFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {}

void HotBackupFeature::prepare() {}

void HotBackupFeature::start() {}

void HotBackupFeature::beginShutdown() {
  // Call off uploads / downloads?
}

void HotBackupFeature::stop() {}

void HotBackupFeature::unprepare() {}

// Lock must be held
arangodb::Result HotBackupFeature::createTransferRecordNoLock (
  std::string const& operation, std::string const& remote,
  std::string const& backupId, std::string const& transferId) {
                                    
  if (_clipBoard.find({backupId,operation,remote}) == _clipBoard.end()) {
    _clipBoard[{backupId,operation,remote}].push_back("CREATED");
    _index[transferId] = {backupId,operation,remote};
    
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
        std::string("Transfer with id ") + transferId + " has already been completed");
    }
  } else {
    if (!remote.empty()) {
      createTransferRecordNoLock(operation, remote, backupId, transferId);
    } else {
      return arangodb::Result(
        TRI_ERROR_HTTP_NOT_FOUND, std::string("No transfer with id ") + transferId);
    }
  }
  
  return arangodb::Result();
  
}

arangodb::Result HotBackupFeature::noteTransferRecord (
  std::string const& operation, std::string const& backupId,
  std::string const& transferId, size_t const& done, size_t const& total) {
  
  std::lock_guard<std::mutex> guard(_clipBoardMutex);
  auto const& t = _index.find(transferId);
  
  if (t != _index.end()) {
    auto const& back = _clipBoard.at(t->second).back();
    if (back != "COMPLETED" && back != "FAILED") {
      _progress[transferId] = {done,total};
    } else {
      return arangodb::Result(
        TRI_ERROR_HTTP_FORBIDDEN,
        std::string("Transfer with id ") + transferId + " has already been completed");
    }
  } else {
    return arangodb::Result(
      TRI_ERROR_HTTP_NOT_FOUND, std::string("No transfer with id ") + transferId);
  }
  
  return arangodb::Result();
  
}

arangodb::Result HotBackupFeature::noteTransferRecord (
  std::string const& operation, std::string const& backupId,
  std::string const& transferId, arangodb::Result const& result) {
  
  std::lock_guard<std::mutex> guard(_clipBoardMutex);
  auto const& t = _index.find(transferId);
  
  if (t != _index.end()) {
    auto const& back = _clipBoard.at(t->second).back();
    if (back != "COMPLETED" && back != "FAILED") {
      auto& clip = _clipBoard.at(t->second);
      if (result.ok()) {
        clip.push_back("COMPLETED");
      } else {
        clip.push_back(std::to_string(result.errorNumber()));
        clip.push_back(std::string("Error: ") + result.errorMessage());
        clip.push_back("FAILED");
      }
      _progress.erase(transferId);
    } else {
      return arangodb::Result(
        TRI_ERROR_HTTP_FORBIDDEN,
        std::string("Transfer with id ") + transferId + " has already been completed");
    }
  } else {
    return arangodb::Result(
      TRI_ERROR_HTTP_NOT_FOUND, std::string("No transfer with id ") + transferId);
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


HotBackupFeature::SD::SD() : hash(0) {}

HotBackupFeature::SD::SD(std::string const& b, std::string const& s, std::string const& d) :
  backupId(b), remote(d), hash(SD::hash_it(operation,remote)) {}

HotBackupFeature::SD::SD(std::string&& b, std::string&& s, std::string&& d) :
  backupId(std::move(b)), operation(std::move(s)), remote(std::move(d)),
  hash(SD::hash_it(s,d)) {}

HotBackupFeature::SD::SD(std::initializer_list<std::string> const& l) {
  TRI_ASSERT(l.size()==3);
  auto it = l.begin();
  backupId  = *(it++);
  operation = *(it++);
  remote    = *(it  );
}

bool operator< (HotBackupFeature::SD const& x, HotBackupFeature::SD const& y) {
  return x.hash < y.hash;
}

}  // namespaces


std::hash<std::string>::result_type arangodb::HotBackupFeature::SD::hash_it(
  std::string const& s, std::string const& d) {
  return std::hash<std::string>{}(s) ^ (std::hash<std::string>{}(d) << 1);
}

namespace std {
ostream& operator<< (ostream& o, arangodb::HotBackupFeature::SD const& sd) {
  o << sd.operation << ":" << sd.remote;
  return o;
}}
