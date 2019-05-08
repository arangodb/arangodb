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

struct SD {
  std::string backupId;
  std::string operation;
  std::string remote;
  SD();
  SD(std::string const&, std::string const&, std::string const&);
  SD(std::string&&, std::string&&, std::string&&);
  SD(std::initializer_list<std::string> const&);
  static std::hash<std::string>::result_type hash_it(
    std::string const&, std::string const&);
  std::hash<std::string>::result_type hash;
};

std::ostream& operator<< (std::ostream& o, SD const& sd); 
namespace std {
template<> struct hash<SD> {
  typedef SD argument_type;
  typedef std::hash<std::string>::result_type result_type;
  result_type operator()(SD const& st) const noexcept {
    return st.hash;
  }
};
}
bool operator< (SD const& x, SD const& y);

namespace arangodb {

class HotBackupFeature : virtual public application_features::ApplicationFeature {
public:

  explicit HotBackupFeature(application_features::ApplicationServer& server);
  ~HotBackupFeature();

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void start() override final;
  void beginShutdown() override final;
  void stop() override final;
  void unprepare() override final;

  arangodb::Result createTransferRecord(
    std::string const& operation, std::string const& remote,
    std::string const& backupId, std::string const& transferId);

  arangodb::Result noteTransferRecord(
    std::string const& operation, std::string const& backupId,
    std::string const& transferId, std::string const& status);

  arangodb::Result getTransferRecord(
    std::string const& id, std::vector<std::vector<std::string>>& reports);

private:
  
  std::mutex _clipBoardMutex;
  std::map<SD, std::vector<std::string>> _clipBoard;
  std::map<std::string, SD> _index;
  
};

} // namespaces

#endif
