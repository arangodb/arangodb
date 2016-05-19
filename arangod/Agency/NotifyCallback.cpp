#include "NotifyCallback.h"

using namespace arangodb::consensus;

NotifyCallback::NotifyCallback(std::function<void(bool)> cb) : _cb(cb) {}

bool NotifyCallback::operator()(arangodb::ClusterCommResult* res) {
  _cb(res->status == CL_COMM_SENT && res->result->getHttpReturnCode() == 200);
  return true;
}
