//
// Created by roman on 23.09.21.
//

#ifndef ARANGODB3_COLLECTIONVALIDATOR_H
#define ARANGODB3_COLLECTIONVALIDATOR_H

#include <Cluster/ServerState.h>
#include <VocBase/VocbaseInfo.h>
#include <velocypack/Slice.h>
#include "Basics/Result.h"
#include "CollectionCreationInfo.h"

using namespace arangodb;

class CollectionValidator {
 public:
  CollectionValidator(CollectionCreationInfo const& info, TRI_vocbase_t const& vocbase,
                      bool isSingleServerSmartGraph, bool enforceReplicationFactor,
                      bool isLocalCollection, bool isSystemName)
      : _info(info),
        _vocbase(vocbase),
        _isSingleServerSmartGraph(isSingleServerSmartGraph),
        _enforceReplicationFactor(enforceReplicationFactor),
        _isLocalCollection(isLocalCollection),
        _isSystemName(isSystemName) {}

  Result validateCreationInfo();

 private:
  CollectionCreationInfo const& _info;
  TRI_vocbase_t const& _vocbase;
  bool _isSingleServerSmartGraph;
  bool _enforceReplicationFactor;
  bool _isLocalCollection;
  bool _isSystemName;
};

#endif  // ARANGODB3_COLLECTIONVALIDATOR_H
