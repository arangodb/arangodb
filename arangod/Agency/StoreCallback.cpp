#include "StoreCallback.h"

using namespace arangodb::consensus;
using namespace arangodb::velocypack;


StoreCallback::StoreCallback() {}
#include <iostream>
bool StoreCallback::operator()(arangodb::ClusterCommResult* res) {
  std::cout << "CALLED" << std::endl;
  return true;
}
  
