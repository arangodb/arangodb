////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include <utility>

#include "Pregel/Utils.h"
#include "Logger/LogMacros.h"

namespace arangodb::pregel {

class PregelMetrics {
 protected:
  using server_t = std::string;
  using numGss_t = uint32_t;
  using spentTime_t = std::chrono::duration<uint64_t>;

 public:
  PregelMetrics() = default;

 private:

};

class PregelConductorMetrics : PregelMetrics {
 public:
  uint16_t numThreadsGlobal = 0;
  // server->max numThreads
  std::map<server_t, uint16_t> serverNumThreads;
  // sourceServer -> (targetServer -> numMsg): sum over all Gss
  std::map<server_t, std::map<server_t, uint16_t>> serverToServerNumMsg;
  // server->(global step number -> time)
  std::map<server_t, std::map<numGss_t, spentTime_t>> serverGssTime;
  // (sourceServer->(targetServer->(global step number->sum of sizes of
  // messages)))
  std::map<server_t, std::map<server_t, std::map<numGss_t, size_t>>>
      serverToServerGssMessageSize;
  // (Gss->number active vertices)
  std::map<numGss_t, uint16_t> GssToNumActive;
  // (server->(Gss->time preparing global step))
  std::map<server_t, std::map<numGss_t, spentTime_t>> serverGssPrepareTime;
  // (server->(Gss->time starting global step))
  std::map<server_t, std::map<numGss_t, spentTime_t>> serverGssStartTime;
  // (server->(Gss->time parsing and storing messages))
  // i.e, time spent in receivedMessages()
  std::map<server_t, std::map<numGss_t, spentTime_t>>
      serverGssAggregateMsgsTime;
  // time initializing workers (in Conductor)
  spentTime_t initWorkersTime;
  // time finalizing workers (in Conductor)
  spentTime_t finalizeWorkersTime;
  // finishedWorkerStep time (in Conductor)
  spentTime_t finishWorkersTime;
  // finishedWorkerFinalize time (in Conductor)
  spentTime_t finishedWorkerFinalizeTime;

  void serializeValues(VPackBuilder& b) const {
    // server->numThreads
    {
      VPackBuilder snT;
      snT.openObject();
      for (auto& [server, numThreads] : serverNumThreads) {
        snT.add(server, VPackValue(numThreads));
      }
      snT.close();
      b.add("serverNumThreads", snT.slice());
    }

    // sourceServer -> (targetServer -> numMsg): sum over all Gss
    {
      VPackBuilder sSrvTSrvNumMsgVP;
      sSrvTSrvNumMsgVP.openObject();
      for (auto const& [sourceServer, tServerNumMsg] : serverToServerNumMsg) {
        VPackBuilder tServerNumMsgVP;
        tServerNumMsgVP.openObject();
        for (auto const& [targetServer, numMsg] : tServerNumMsg) {
          tServerNumMsgVP.add(targetServer, VPackValue(numMsg));
        }
        tServerNumMsgVP.close();
        sSrvTSrvNumMsgVP.add(sourceServer, tServerNumMsgVP.slice());
      }
      sSrvTSrvNumMsgVP.close();
      b.add("serverToServerNumMsg", sSrvTSrvNumMsgVP.slice());
    }

    // server->(global step number -> time)
    {
      VPackBuilder serverGssTimeVP;
      serverGssTimeVP.openObject();
      for (auto const& [server, gssTime] : serverGssTime) {
        VPackBuilder gssTimeVP;
        gssTimeVP.openObject();
        for (auto const& [gss, duration] : gssTime) {
          gssTimeVP.add(std::to_string(gss), VPackValue(duration.count()));
        }
        gssTimeVP.close();
        serverGssTimeVP.add(server, gssTimeVP.slice());
      }
      serverGssTimeVP.close();
      b.add("serverGssTime", serverGssTimeVP.slice());
    }

    {
      // (sourceServer->(targetServer->(global step number->sum of sizes of
      // messages)))
      VPackBuilder sSrvTSrvGssSizeMsg;
      sSrvTSrvGssSizeMsg.openObject();
      for (auto const& [sSrv, tSrvGssSizeMsg] : serverToServerGssMessageSize) {
        VPackBuilder tSrvGssSizeMsgVP;
        tSrvGssSizeMsgVP.openObject();
        for (auto const& [tSrv, gssSizeMsg] : tSrvGssSizeMsg) {
          VPackBuilder gssSizeMsgVP;
          gssSizeMsgVP.openObject();
          for (auto const& [gss, sizeMsg] : gssSizeMsg) {
            gssSizeMsgVP.add(std::to_string(gss), VPackValue(sizeMsg));
          }
          gssSizeMsgVP.close();
          tSrvGssSizeMsgVP.add(tSrv, gssSizeMsgVP.slice());
        }
        tSrvGssSizeMsgVP.close();
        sSrvTSrvGssSizeMsg.add(sSrv, tSrvGssSizeMsgVP.slice());
      }
      sSrvTSrvGssSizeMsg.close();
      b.add("sSrvTSrvGssSizeMsg", sSrvTSrvGssSizeMsg.slice());
    }

    {
      // (Gss->number active vertices)
      VPackBuilder gssNumActive;
      gssNumActive.openObject();
      for (auto const& [gss, numActive] : GssToNumActive) {
        gssNumActive.add(std::to_string(gss), VPackValue(numActive));
      }
      gssNumActive.close();
      b.add("gssNumActive", gssNumActive.slice());
    }

    {
      // (server->(Gss->time preparing global step))
      VPackBuilder srvGssPrepTime;
      srvGssPrepTime.openObject();
      for (auto const& [srv, gssPrepTime] : serverGssPrepareTime) {
        VPackBuilder gssPrepTimeVP;
        gssPrepTimeVP.openObject();
        for (auto const& [gss, prepTime] : gssPrepTime) {
          gssPrepTimeVP.add(std::to_string(gss), VPackValue(prepTime.count()));
        }
        gssPrepTimeVP.close();
        srvGssPrepTime.add(srv, gssPrepTimeVP.slice());
      }
      srvGssPrepTime.close();
      b.add("srvGssPrepTime", srvGssPrepTime.slice());
    }

    {
      // (server->(Gss->time starting global step))
      VPackBuilder srvGssStartTime;
      srvGssStartTime.openObject();
      for (auto const& [srv, gssStartTime] : serverGssStartTime) {
        VPackBuilder gssStartTimeVP;
        gssStartTimeVP.openObject();
        for (auto const& [gss, startTime] : gssStartTime) {
          gssStartTimeVP.add(std::to_string(gss),
                             VPackValue(startTime.count()));
        }
        gssStartTimeVP.close();
        srvGssStartTime.add(srv, gssStartTimeVP.slice());
      }
      srvGssStartTime.close();
      b.add("srvGssStartTime", srvGssStartTime.slice());
    }

    {
      // (server->(Gss->time parsing and storing messages))
      VPackBuilder srvGssAggrTime;
      srvGssAggrTime.openObject();
      for (auto const& [srv, gssAggrTime] : serverGssAggregateMsgsTime) {
        VPackBuilder gssAggrTimeVP;
        gssAggrTimeVP.openObject();
        for (auto const& [gss, duration] : gssAggrTime) {
          gssAggrTimeVP.add(std::to_string(gss), VPackValue(duration.count()));
        }
        gssAggrTimeVP.close();
        srvGssAggrTime.add(srv, gssAggrTimeVP.slice());
      }
      srvGssAggrTime.close();
      b.add("srvGssAggrTime", srvGssAggrTime.slice());
    }

    // time initializing workers (in Conductor)
    b.add("initWorkersTime", VPackValue(initWorkersTime.count()));
    // time finalizing workers (in Conductor))
    b.add("finalizeWorkersTime", VPackValue(finalizeWorkersTime.count()));
    // finishedWorkerStep time (in Conductor)
    b.add("finishWorkersTime", VPackValue(finishWorkersTime.count()));
    // finishedWorkerFinalize time (in Conductor)
    b.add("finishedWorkerFinalizeTime",
          VPackValue(finishedWorkerFinalizeTime.count()));
  }

};

class PregelWorkerMetrics : PregelMetrics {
 public:

  // max numThreads
  uint16_t numThreads;
  // targetServer -> numMsg: sum over all Gss
  std::map<server_t, uint16_t> toServerNumMsg;
  // global step number -> time
  std::map<numGss_t, spentTime_t> gssTime;
  // targetServer->(global step number->sum of sizes of messages)
  std::map<server_t, std::map<numGss_t, size_t>> toServerGssMessageSize;
  // Gss->number active vertices
  std::map<numGss_t, uint16_t> gssToNumActive;
  // server->(Gss->time preparing global step)
  std::map<server_t, std::map<numGss_t, spentTime_t>> serverGssPrepareTime;
  // server->(Gss->time starting global step)
  std::map<server_t, std::map<numGss_t, spentTime_t>> serverGssStartTime;
  // server->(Gss->time parsing and storing messages)
  // i.e, time spent in receivedMessages()
  std::map<server_t, std::map<numGss_t, spentTime_t>>
      serverGssAggregateMsgsTime;




 private:
  numGss_t _gss = 0;
};

struct MessageStats {
  size_t sendCount = 0;
  size_t receivedCount = 0;
  double superstepRuntimeSecs = 0;

  MessageStats() = default;
  explicit MessageStats(VPackSlice statValues) { accumulate(statValues); }
  MessageStats(size_t s, size_t r) : sendCount(s), receivedCount(r) {}

  void accumulate(MessageStats const& other) {
    sendCount += other.sendCount;
    receivedCount += other.receivedCount;
    superstepRuntimeSecs += other.superstepRuntimeSecs;
  }

  void accumulate(VPackSlice statValues) {
    VPackSlice p = statValues.get(Utils::sendCountKey);
    if (p.isInteger()) {
      sendCount += p.getUInt();
    }
    p = statValues.get(Utils::receivedCountKey);
    if (p.isInteger()) {
      receivedCount += p.getUInt();
    }
    // p = statValues.get(Utils::superstepRuntimeKey);
    // if (p.isNumber()) {
    //  superstepRuntimeSecs += p.getNumber<double>();
    //}
  }

  void serializeValues(VPackBuilder& b) const {
    b.add(Utils::sendCountKey, VPackValue(sendCount));
    b.add(Utils::receivedCountKey, VPackValue(receivedCount));
  }

  void resetTracking() {
    sendCount = 0;
    receivedCount = 0;
    superstepRuntimeSecs = 0;
  }

  bool allMessagesProcessed() const { return sendCount == receivedCount; }
};

struct StatsManager {
  void accumulateActiveCounts(VPackSlice data) {
    VPackSlice sender = data.get(Utils::senderKey);
    if (sender.isString()) {
      VPackSlice active = data.get(Utils::activeCountKey);
      if (active.isInteger()) {
        _activeStats[sender.copyString()] += active.getUInt();
      }
    }
  }

  void accumulateMessageStats(VPackSlice data) {
    VPackSlice sender = data.get(Utils::senderKey);
    if (sender.isString()) {
      _serverStats[sender.copyString()].accumulate(data);
    }
  }

  void serializeValues(VPackBuilder& b) const {
    MessageStats stats;
    for (auto const& pair : _serverStats) {
      stats.accumulate(pair.second);
    }
    stats.serializeValues(b);
  }

  /// Test if all messages were processed
  bool allMessagesProcessed() const {
    uint64_t send = 0, received = 0;
    for (auto const& pair : _serverStats) {
      send += pair.second.sendCount;
      received += pair.second.receivedCount;
    }
    return send == received;
  }

  void debugOutput() const {
    uint64_t send = 0, received = 0;
    for (auto const& pair : _serverStats) {
      send += pair.second.sendCount;
      received += pair.second.receivedCount;
    }
    LOG_TOPIC("26dad", TRACE, Logger::PREGEL)
        << send << " - " << received << " : " << send - received;
  }

  /// tests if active count is greater 0
  bool noActiveVertices() const {
    for (auto const& pair : _activeStats) {
      if (pair.second > 0) {
        return false;
      }
    }
    return true;
  }

  void resetActiveCount() {
    for (auto& pair : _activeStats) {
      pair.second = 0;
    }
  }

  void reset() { _serverStats.clear(); }

  size_t clientCount() const { return _serverStats.size(); }

 private:
  std::map<std::string, uint64_t> _activeStats;  // (senderKey->activeCountKey)
  std::map<std::string, MessageStats> _serverStats;
  PregelConductorMetrics _pregelMetrics;
};
}  // namespace arangodb::pregel
