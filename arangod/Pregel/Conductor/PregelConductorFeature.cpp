#include "PregelConductorFeature.h"
#include "ApplicationFeatures/ApplicationServer.h"


using namespace arangodb::pregel;

PregelConductorFeature::PregelConductorFeature(Server& server)
    : ArangodFeature{server, *this} {

//  static_assert(
//      Server::isCreatedAfter<PregelFeature, metrics::MetricsFeature>());
//  setOptional(true);
//  startsAfter<DatabaseFeature>();
//  startsAfter<application_features::V8FeaturePhase>();
}