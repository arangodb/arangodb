//
// Hello World client
// Connects REQ socket to tcp://localhost:5555
// Sends "Hello" to server, expects "World" back
//
#include <zmq.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

#include <pthread.h>

#include "ProtocolBuffers/arangodb.pb.h"

using namespace std;

void* context = 0;
size_t loop = 10000;
char const* connection = "tcp://localhost:5555";


// Aufruf:
//
// -C <zeromq-connection>
// -u <path>            where <path> is of the form "/_api/version"
// -h <key> <value>     where <key> is converted to all lowercase
// -v <key> <value>
// -P <pipeline>        send <number> request in one message
// -c <concurrency>     use <concurrency> parallel threads
// -n <count>           send a total of <count> requests



void* ThreadStarter (void* data) {
  int res;
  size_t n;
  void *requester;

  requester = zmq_socket(context, ZMQ_REQ);
  res = zmq_connect(requester, connection);

  if (res != 0) {
    printf("ERROR zmq_connect: %d\n", errno);
  }
  
  printf("connected\n");

  for (n = 0;  n < loop;  n++) {
    zmq_msg_t request;
    zmq_msg_t reply;

    PB_ArangoMessage messages;
    PB_ArangoBatchMessage* batch;
    PB_ArangoBlobRequest* blob;

    batch = messages.add_messages();

    batch->set_type(PB_BLOB_REQUEST);
    blob = batch->mutable_request();

    blob->set_requesttype(PB_REQUEST_TYPE_GET);
    blob->set_url("/_api/version");
    blob->set_contenttype(PB_NO_CONTENT);
    blob->set_contentlength(0);
    blob->set_content("");

    string data;
    messages.SerializeToString(&data);

    // send
    zmq_msg_init_size (&request, data.size());
    memcpy (zmq_msg_data(&request), data.c_str(), data.size());

    res = zmq_send (requester, &request, 0);

    if (res != 0) {
      printf("ERROR zmq_send: %d %d %s\n", (int) res, (int) errno, strerror(errno));
    }

    zmq_msg_close (&request);

    // receive
    zmq_msg_init (&reply);

    res = zmq_recv (requester, &reply, 0);

    if (res != 0) {
      printf("ERROR zmq_recv: %d %d %s\n", (int) res, (int) errno, strerror(errno));
    }

    zmq_msg_close (&reply);
  }

  zmq_close (requester);
  return 0;
}




int main (int argc, char* argv[])
{
  int conc = 1;
  int t1;
  int t2;
  int rt;
  int i;
  pthread_t* threads;
  
  if (1 < argc) {
    loop = atoi(argv[1]);
  }

  if (2 < argc) {
    conc = atoi(argv[2]);
  }

  if (3 < argc) {
    connection = argv[3];
  }

  printf("Looping %d times pro thread, total %d\n", (int) loop, (int)(loop * conc));
  printf("Concurrency %d\n", conc);
  printf("Connection %s\n", connection);

  context = zmq_init(16);

  // Socket to talk to server
  printf ("Connecting to hello world server…\n");

  t1 = time(0);
  threads = (pthread_t*) malloc(sizeof(pthread_t) * conc);

  for (i = 0;  i < conc;  ++i) {
    pthread_create(&threads[i], 0, &ThreadStarter, 0);
  }

  for (i = 0;  i < conc;  ++i) {
    pthread_join(threads[i], 0);
  }

  t2 = time(0);
  rt = t2 - t1;

  printf("runtime %d, req/sec %f\n", rt, 1.0 * loop * conc / rt);

  zmq_term (context);
  return 0;
}
