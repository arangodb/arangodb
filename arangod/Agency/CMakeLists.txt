add_library(arango_agency STATIC
  ActiveFailoverJob.cpp
  AddFollower.cpp
  AgencyComm.cpp
  AgencyFeature.cpp
  AgencyPaths.cpp
  Agent.cpp
  AgentCallback.cpp
  AgentConfiguration.cpp
  AsyncAgencyComm.cpp
  CleanOutServer.cpp
  Compactor.cpp
  Constituent.cpp
  FailedFollower.cpp
  FailedLeader.cpp
  FailedServer.cpp
  Helpers.cpp
  Inception.cpp
  Job.cpp
  JobContext.cpp
  MoveShard.cpp
  Node.cpp
  PathComponent.cpp
  ReconfigureReplicatedLog.cpp
  RemoveFollower.cpp
  ResignLeadership.cpp
  RestAgencyHandler.cpp
  RestAgencyPrivHandler.cpp
  State.cpp
  Store.cpp
  Supervision.cpp
  v8-agency.cpp)

target_link_libraries(arango_agency
  arango
  arango_health
  arango_iresearch
  arango_metrics)

target_include_directories(arango_agency PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")
