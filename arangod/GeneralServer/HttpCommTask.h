#ifndef ARANGOD_GENERAL_SERVER_HTTP_COMM_TASK_H
#define ARANGOD_GENERAL_SERVER_HTTP_COMM_TASK_H 1

#include "GeneralServer/GeneralCommTask.h"
namespace arangodb {
namespace rest {

class HttpCommTask : public GeneralCommTask {
 public:
  HttpCommTask(GeneralServer* serv, TRI_socket_t sock, ConnectionInfo&& info,
               double timeout)
      : Task("HttpCommTask"),
        GeneralCommTask(serv, sock, std::move(info), timeout) {
    _protocol = "http";
  }
};
}  // rest
}  // arangodb

#endif
