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
/// @author Dr. Frank Celler
/// @author Jan Steemann
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_VOCBASE_METHODS_TASKS_H
#define ARANGOD_VOCBASE_METHODS_TASKS_H 1

#include <chrono>

#include <v8.h>
#include <velocypack/Builder.h>

#include "Basics/Mutex.h"
#include "Basics/Result.h"
#include "Basics/VelocyPackHelper.h"
#include "Scheduler/Scheduler.h"

struct TRI_vocbase_t;

namespace arangodb {

class DatabaseGuard;
class ExecContext;

class Task : public std::enable_shared_from_this<Task> {
 public:
  static std::shared_ptr<Task> createTask(std::string const& id, std::string const& name,
                                          TRI_vocbase_t* vocbase, std::string const& command,
                                          bool allowUseDatabase, ErrorCode& ec);

  static ErrorCode unregisterTask(std::string const& id, bool cancel);

  static std::shared_ptr<velocypack::Builder> registeredTask(std::string const& id);
  static std::shared_ptr<velocypack::Builder> registeredTasks();
  static void shutdownTasks();
  static void removeTasksForDatabase(std::string const&);
  static bool tryCompile(application_features::ApplicationServer& server,
                         v8::Isolate* isolate, std::string const& command);

 private:
  static Mutex _tasksLock;
  // id => [ user, task ]
  static std::unordered_map<std::string, std::pair<std::string, std::shared_ptr<Task>>> _tasks;

 public:
  Task(std::string const& id, std::string const& name, TRI_vocbase_t& vocbase,
       std::string const& command, bool allowUseDatabase);
  ~Task();

 public:
  void setOffset(double offset);
  void setPeriod(double offset, double period);
  void setParameter(std::shared_ptr<velocypack::Builder> const& parameters);
  void setUser(std::string const& user);

  void start();
  void cancel();

  std::shared_ptr<velocypack::Builder> toVelocyPack() const;

  bool databaseMatches(std::string const&) const;

 private:
  void toVelocyPack(velocypack::Builder&) const;
  void work(ExecContext const*);
  bool queue(std::chrono::microseconds offset) ADB_WARN_UNUSED_RESULT;
  std::function<void(bool cancelled)> callbackFunction();
  std::string const& name() const { return _name; }

 private:
  std::string const _id;
  std::string const _name;
  double const _created;
  std::string _user;

  Scheduler::WorkHandle _taskHandle;
  Mutex _taskHandleMutex;

  // guard to make sure the database is not dropped while used by us
  std::unique_ptr<DatabaseGuard> _dbGuard;

  std::string const _command;
  std::shared_ptr<velocypack::Builder> _parameters;
  bool const _allowUseDatabase;

  std::chrono::microseconds _offset;
  std::chrono::microseconds _interval;
  std::atomic<bool> _periodic{false};
};

}  // namespace arangodb

#endif
