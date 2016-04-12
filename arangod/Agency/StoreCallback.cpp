#include "StoreCallback.h"
#include "Store.h"

using namespace arangodb::consensus;
using namespace arangodb::velocypack;

StoreCallback::StoreCallback() {}

bool StoreCallback::operator()(arangodb::ClusterCommResult* res) {
  return true;
}
  
