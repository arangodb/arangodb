#include "RecoveringState.h"

#include <chrono>

#include "Pregel/Algorithm.h"
#include "Pregel/Conductor.h"
#include "Pregel/Conductor/State.h"
#include "Pregel/MasterContext.h"
#include "Pregel/PregelFeature.h"
#include "Pregel/Recovery.h"
#include "Pregel/WorkerConductorMessages.h"

using namespace arangodb::pregel::conductor;

Recovering::Recovering(Conductor& conductor, std::chrono::seconds const& ttl)
    : conductor{conductor} {
  conductor.updateState(ExecutionState::RECOVERING);
  expiration = std::chrono::system_clock::now() + ttl;
}

auto Recovering::run() -> void {
  if (conductor._algorithm->supportsCompensation() == false) {
    LOG_PREGEL_CONDUCTOR("12e0e", ERR) << "Algorithm does not support recovery";
    conductor.changeState(conductor::StateType::Canceled);
    return;
  }

  // we lost a DBServer, we need to reconfigure all remainging servers
  // so they load the data for the lost machine
  conductor._statistics.reset();

  TRI_ASSERT(SchedulerFeature::SCHEDULER != nullptr);

  // let's wait for a final state in the cluster
  conductor._workHandle = SchedulerFeature::SCHEDULER->queueDelayed(
      RequestLane::CLUSTER_AQL, std::chrono::seconds(2),
      [this, self = conductor.shared_from_this()](bool cancelled) {
        if (cancelled) {
          return;
        }
        std::vector<ServerID> goodServers;
        auto res = conductor._feature.recoveryManager()->filterGoodServers(
            conductor._dbServers, goodServers);
        if (res != TRI_ERROR_NO_ERROR) {
          LOG_PREGEL_CONDUCTOR("3d08b", ERR) << "Recovery proceedings failed";
          conductor.changeState(conductor::StateType::Canceled);
          return;
        }
        conductor._dbServers = goodServers;

        auto cancelGssCommand =
            CancelGss{.executionNumber = conductor._executionNumber,
                      .gss = conductor._globalSuperstep};
        auto response = conductor._sendToAllDBServers<GssCanceled>(
            Utils::cancelGSSPath, cancelGssCommand);
        if (response.fail()) {
          LOG_PREGEL_CONDUCTOR("", ERR) << "Gss could not be canceled";
        }

        // TODO somehow test here that state is not Canceled
        if (conductor._state != ExecutionState::RECOVERING) {
          return;  // seems like we are canceled
        }

        // Let's try recovery
        if (conductor._masterContext) {
          bool proceed = conductor._masterContext->preCompensation();
          if (!proceed) {
            conductor.changeState(conductor::StateType::Canceled);
            return;
          }
        }

        VPackBuilder additionalKeys;
        additionalKeys.openObject();
        additionalKeys.add(Utils::recoveryMethodKey,
                           VPackValue(Utils::compensate));
        additionalKeys.close();
        conductor._aggregators->resetValues();

        // initialize workers will reconfigure the workers and set the
        // _dbServers list to the new primary DBServers
        res = conductor._initializeWorkers(Utils::startRecoveryPath,
                                           additionalKeys.slice());
        if (res != TRI_ERROR_NO_ERROR) {
          LOG_PREGEL_CONDUCTOR("fefc6", ERR) << "Compensation failed";
          conductor.changeState(conductor::StateType::Canceled);
          return;
        }
      });
}

auto Recovering::receive(Message const& message) -> void {
  if (message.type() != MessageType::RecoveryFinished) {
    LOG_PREGEL_CONDUCTOR("2c9ee", WARN)
        << "When recovering, we expect a RecoveryFinished "
           "message, but we received message type "
        << static_cast<int>(message.type());
    return;
  }
  auto finishedMessage = static_cast<RecoveryFinished const&>(message);

  conductor._ensureUniqueResponse(finishedMessage.senderId);
  // the recovery mechanism might be gathering state information
  conductor._aggregators->aggregateValues(finishedMessage.aggregators.slice());
  if (conductor._respondedServers.size() != conductor._dbServers.size()) {
    return;
  }

  // only compensations supported
  bool proceed = false;
  if (conductor._masterContext) {
    proceed = proceed || conductor._masterContext->postCompensation();
  }

  if (!proceed) {
    LOG_PREGEL_CONDUCTOR("6ecf2", INFO)
        << "Recovery finished. Proceeding normally";

    // build the message, works for all cases
    auto finalizeRecoveryCommand =
        FinalizeRecovery{.executionNumber = conductor._executionNumber,
                         .gss = conductor._globalSuperstep};
    auto response = conductor._sendToAllDBServers<RecoveryFinalized>(
        Utils::finalizeRecoveryPath, finalizeRecoveryCommand);
    if (response.fail()) {
      LOG_PREGEL_CONDUCTOR("7f97e", INFO) << "Recovery failed";
      conductor.changeState(conductor::StateType::Canceled);
      return;
    }
    conductor.changeState(StateType::Computing);
    return;
  }

  // reset values which are calculated during the superstep
  conductor._aggregators->resetValues();
  if (conductor._masterContext) {
    conductor._masterContext->preCompensation();
  }

  VPackBuilder aggregators;
  {
    VPackObjectBuilder ob(&aggregators);
    conductor._aggregators->serializeValues(aggregators);
  }
  auto continueRecoveryCommand =
      ContinueRecovery{.executionNumber = conductor._executionNumber,
                       .aggregators = std::move(aggregators)};
  auto response = conductor._sendToAllDBServers<RecoveryContinued>(
      Utils::continueRecoveryPath, continueRecoveryCommand);
  if (response.fail()) {
    LOG_PREGEL_CONDUCTOR("7f97e", INFO) << "Recovery failed";
    conductor.changeState(conductor::StateType::Canceled);
    return;
  }
}
