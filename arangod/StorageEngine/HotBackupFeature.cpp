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

#include "Agency/TimeString.h"
#include "Basics/Result.h"
#include "Cluster/ServerState.h"
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
  } else {
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      "A transfer to/from the remote destination is already in progress");
  }
  return arangodb::Result();  
}

arangodb::Result HotBackupFeature::noteTransferRecord (
  std::string const& operation, std::string const& backupId,
  std::string const& transferId, std::string const& status,
  std::string const& remote) {

  arangodb::Result res;
  std::lock_guard<std::mutex> guard(_clipBoardMutex);
  auto const& t = _index.find(transferId);
  
  if (t != _index.end()) {
    auto const& back = _clipBoard.at(t->second).back();
    if (back != "COMPLETED" && back != "FAILED") {
      _clipBoard.at(t->second).push_back(status);
    } else {
      res.reset(
        TRI_ERROR_HTTP_FORBIDDEN,
        std::string("Transfer with id ") + transferId + " has already been completed");
    }
  } else {
    if (!remote.empty()) {
      res = createTransferRecordNoLock(operation, remote, backupId, transferId);
    } else {
      res.reset(
        TRI_ERROR_HTTP_NOT_FOUND, std::string("No transfer with id ") + transferId);
    }
  }
  
  return res;
  
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

  LOG_DEVEL << typeid(t).name();
  
  if (t != _index.end()) {

    auto cit = _clipBoard.find(t->second);

    auto const& back = cit->second.back();
    if (back != "COMPLETED" && back != "FAILED") {
      auto& clip = _clipBoard.at(t->second);

      // Last status
      if (result.ok()) {
        clip.push_back("COMPLETED");
      } else {
        clip.push_back(std::to_string(result.errorNumber()));
        clip.push_back(std::string("Error: ") + result.errorMessage());
        clip.push_back("FAILED");
      }
      
      // Clean up progress
      _progress.erase(transferId);

      // Archive results
      _archive.insert(std::make_move_iterator(cit), std::make_move_iterator(cit));

    } else {
      return arangodb::Result(
        TRI_ERROR_HTTP_FORBIDDEN,
        std::string("Transfer with id ") +
        transferId + " has already been completed");
    }
  } else {
    return arangodb::Result(
      TRI_ERROR_HTTP_NOT_FOUND, std::string("No transfer with id ") + transferId);
  }
    
  return arangodb::Result();
  
}



arangodb::Result HotBackupFeature::getTransferRecord(
  std::string const& id, VPackBuilder& report) const {

  if (!report.isEmpty()) {
    report.clear();
  }

  std::lock_guard<std::mutex> guard(_clipBoardMutex);

  auto const& t = _index.find(id);
  if (t != _index.end()) {
    auto const& clip = _clipBoard.find(t->second);
    auto const& arch = _archive.find(t->second);

    auto const& transfer = t->second;
    {
      VPackObjectBuilder r(&report);
      report.add("Timestamp", VPackValue(transfer.started));
      report.add(
        (transfer.operation == "upload") ? "UploadId" : "DownloadId",
        VPackValue(transfer.backupId));
      report.add(VPackValue("DBServers"));
      {
        VPackObjectBuilder dbservers(&report);
        report.add(VPackValue("SNGL"));
        {
          VPackObjectBuilder server(&report);
          report.add("Status",
                     VPackValue(clip != _clipBoard.end() ?
                                clip->second.back() : arch->second.back()));
          auto const& pit = _progress.find(t->first);
          if (pit != _progress.end()) {
            auto const& progress = pit->second;
            report.add(VPackValue("Progress"));
            {
              VPackObjectBuilder o(&report);
              report.add("Total", VPackValue(progress.total));
              report.add("Done", VPackValue(progress.done));
              report.add("Time", VPackValue(progress.timeStamp));
            }
          }
        }
      }
    }
  } else {
    return arangodb::Result(
      TRI_ERROR_HTTP_NOT_FOUND, std::string("No transfer with id ") + id);
  }

  return arangodb::Result();
}


HotBackupFeature::SD::SD() : hash(0) {}

HotBackupFeature::SD::SD(std::string const& b, std::string const& s, std::string const& d) :
  backupId(b), remote(d), started(timepointToString(std::chrono::system_clock::now())),
  hash(SD::hash_it(operation,remote)) {}

HotBackupFeature::SD::SD(std::string&& b, std::string&& o, std::string&& r) :
  backupId(std::move(b)), operation(std::move(o)), remote(std::move(r)),
  started(timepointToString(std::chrono::system_clock::now())),
  hash(SD::hash_it(o,r)) {}

HotBackupFeature::SD::SD(std::initializer_list<std::string> const& l) {
  TRI_ASSERT(l.size()==3);
  auto it   = l.begin();
  backupId  = *(it++);
  operation = *(it++);
  remote    = *(it  );
  started   = timepointToString(std::chrono::system_clock::now());
  hash      = SD::hash_it(operation,remote);
}

bool operator< (HotBackupFeature::SD const& x, HotBackupFeature::SD const& y) {
  return x.hash < y.hash;
}

HotBackupFeature::Progress::Progress() :
  done(0), total(0), timeStamp(timepointToString(std::chrono::system_clock::now())){}

HotBackupFeature::Progress::Progress(std::initializer_list<size_t> const& l) {
  TRI_ASSERT(l.size() == 2);
  done = *l.begin();
  total = *(l.begin()+1);
  timeStamp = timepointToString(std::chrono::system_clock::now());
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
