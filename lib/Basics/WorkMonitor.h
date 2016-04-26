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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_WORK_MONITOR_H
#define ARANGODB_BASICS_WORK_MONITOR_H 1

#include "Basics/Thread.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/WorkDescription.h"
#include "Basics/WorkItem.h"

namespace arangodb {
namespace velocypack {
class Builder;
}

class WorkMonitor : public Thread {
 public:
  WorkMonitor();
  ~WorkMonitor() {shutdown();}

 public:
  bool isSilent() override { return true; }

 public:
  static void freeWorkDescription(WorkDescription* desc);
  static bool pushThread(Thread* thread);
  static void popThread(Thread* thread);
  static void pushAql(TRI_vocbase_t*, uint64_t queryId, char const* text,
                      size_t length);
  static void pushAql(TRI_vocbase_t*, uint64_t queryId);
  static void popAql();
  static void pushCustom(char const* type, char const* text, size_t length);
  static void pushCustom(char const* type, uint64_t id);
  static void popCustom();
  static void pushHandler(arangodb::rest::HttpHandler*);
  static WorkDescription* popHandler(arangodb::rest::HttpHandler*, bool free);
  static void requestWorkOverview(uint64_t taskId);
  static void cancelWork(uint64_t id);

 protected:
  void run() override;

 private:
  static void sendWorkOverview(uint64_t, std::string const&);
  static bool cancelAql(WorkDescription*);
  static void deleteHandler(WorkDescription* desc);
  static void vpackHandler(arangodb::velocypack::Builder*,
                           WorkDescription* desc);

  static WorkDescription* createWorkDescription(WorkType);
  static void deleteWorkDescription(WorkDescription*, bool stopped);
  static void activateWorkDescription(WorkDescription*);
  static WorkDescription* deactivateWorkDescription();
  static void vpackWorkDescription(VPackBuilder*, WorkDescription*);
  static void cancelWorkDescriptions(Thread* thread);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief auto push and pop for HttpHandler
////////////////////////////////////////////////////////////////////////////////

class HandlerWorkStack {
  HandlerWorkStack(const HandlerWorkStack&) = delete;
  HandlerWorkStack& operator=(const HandlerWorkStack&) = delete;

 public:
  explicit HandlerWorkStack(arangodb::rest::HttpHandler*);

  explicit HandlerWorkStack(WorkItem::uptr<arangodb::rest::HttpHandler>&);

  ~HandlerWorkStack();

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the handler
  //////////////////////////////////////////////////////////////////////////////

  arangodb::rest::HttpHandler* handler() const { return _handler; }

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief handler
  //////////////////////////////////////////////////////////////////////////////

  arangodb::rest::HttpHandler* _handler;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief auto push and pop for Aql
////////////////////////////////////////////////////////////////////////////////

class AqlWorkStack {
  AqlWorkStack(const AqlWorkStack&) = delete;
  AqlWorkStack& operator=(const AqlWorkStack&) = delete;

 public:
  AqlWorkStack(TRI_vocbase_t*, uint64_t id, char const* text, size_t length);

  AqlWorkStack(TRI_vocbase_t*, uint64_t id);

  ~AqlWorkStack();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief auto push and pop for Custom
////////////////////////////////////////////////////////////////////////////////

class CustomWorkStack {
  CustomWorkStack(const CustomWorkStack&) = delete;
  CustomWorkStack& operator=(const CustomWorkStack&) = delete;

 public:
  CustomWorkStack(char const* type, char const* text, size_t length);

  CustomWorkStack(char const* type, uint64_t id);

  ~CustomWorkStack();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief starts the work monitor
////////////////////////////////////////////////////////////////////////////////

void InitializeWorkMonitor();

////////////////////////////////////////////////////////////////////////////////
/// @brief stops the work monitor
////////////////////////////////////////////////////////////////////////////////

void ShutdownWorkMonitor();
}

#endif
