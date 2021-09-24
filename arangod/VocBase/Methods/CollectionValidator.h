//
// Created by roman on 23.09.21.
//

#ifndef ARANGODB3_COLLECTIONVALIDATOR_H
#define ARANGODB3_COLLECTIONVALIDATOR_H

#include <VocBase/VocbaseInfo.h>
#include <velocypack/Slice.h>
#include "Basics/Result.h"

using namespace arangodb;

class CollectionValidator {
 public:
  static Result verifyCollectionName(arangodb::velocypack::Slice const parameters, arangodb::CreateDatabaseInfo const& info);
};


#endif  // ARANGODB3_COLLECTIONVALIDATOR_H
