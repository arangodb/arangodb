////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_STORAGE_ENGINE_HOT_BACKUP_FEATURE_H
#define ARANGOD_STORAGE_ENGINE_HOT_BACKUP_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"

#include <mutex>

namespace arangodb {

class HotBackupFeature : virtual public application_features::ApplicationFeature {
public:

  struct SD {
    std::string backupId;
    std::string operation;
    std::string remote;
    std::string started;
    SD();
    SD(std::string const&, std::string const&, std::string const&);
    SD(std::string&&, std::string&&, std::string&&);
    SD(std::initializer_list<std::string> const&);
    static std::hash<std::string>::result_type hash_it(
      std::string const&, std::string const&);
    std::hash<std::string>::result_type hash;
  };

  struct Progress {
    size_t done;
    size_t total;
    std::string timeStamp;
    Progress ();
    Progress (std::initializer_list<size_t> const& l);
  };

  explicit HotBackupFeature(application_features::ApplicationServer& server);
  ~HotBackupFeature();

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void start() override final;
  void beginShutdown() override final;
  void stop() override final;
  void unprepare() override final;

  arangodb::Result createTransferRecordNoLock(
    std::string const& operation, std::string const& remote,
    std::string const& backupId, std::string const& transferId);

  arangodb::Result noteTransferRecord(
    std::string const& operation, std::string const& backupId,
    std::string const& transferId, std::string const& status,
    std::string const& remote);

  arangodb::Result noteTransferRecord(
    std::string const& operation, std::string const& backupId,
    std::string const& transferId, size_t const& done, size_t const& total);

  arangodb::Result noteTransferRecord(
    std::string const& operation, std::string const& backupId,
    std::string const& transferId, arangodb::Result const& result);

  arangodb::Result getTransferRecord(
    std::string const& id, VPackBuilder& reports) const;

private:
  
  mutable std::mutex _clipBoardMutex;
  std::map<SD, std::vector<std::string>> _clipBoard;
  std::map<SD, std::vector<std::string>> _archive;
  std::map<std::string, SD> _index;
  std::map<std::string, Progress> _progress;
  
};

} // namespaces

std::ostream& operator<< (std::ostream& o, arangodb::HotBackupFeature::SD const& sd); 
namespace std {
template<> struct hash<arangodb::HotBackupFeature::SD> {
  typedef arangodb::HotBackupFeature::SD argument_type;
  typedef std::hash<std::string>::result_type result_type;
  result_type operator()(arangodb::HotBackupFeature::SD const& st) const noexcept {
    return st.hash;
  }
};
}
bool operator< (arangodb::HotBackupFeature::SD const& x, arangodb::HotBackupFeature::SD const& y);

#endif
