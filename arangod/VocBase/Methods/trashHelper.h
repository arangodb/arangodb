//
// Created by roman on 23.09.21.
//

#ifndef ARANGODB3_TRASHHELPER_H
#define ARANGODB3_TRASHHELPER_H

#include "Basics/Result.h"

Result verifyCollectionName(arangodb::velocypack::Slice const parameters);

#endif  // ARANGODB3_TRASHHELPER_H
