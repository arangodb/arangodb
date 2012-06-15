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

void* context = 0;
size_t loop = 10000;
char const* connection = "tcp://localhost:5555";





void* ThreadStarter (void* data) {
  int res;

  void *requester = zmq_socket(context, ZMQ_REQ);
  res = zmq_connect(requester, connection);

  if (res != 0) {
    printf("ERROR zmq_connect: %d\n", errno);
  }
  
  for (size_t n = 0;  n < loop;  n++) {

    // send
    zmq_msg_t request;
    zmq_msg_init_size (&request, 5);
    memcpy (zmq_msg_data (&request), "Hello", 5);

    res = zmq_send (requester, &request, 0);

    if (res != 0) {
      printf("ERROR zmq_send: %d %d %s\n", (int) res, (int) errno, strerror(errno));
    }

    zmq_msg_close (&request);

    // receive
    zmq_msg_t reply;
    zmq_msg_init (&reply);

    res = zmq_recv (requester, &reply, 0);

    if (res != 0) {
      printf("ERROR zmq_recv: %d %d %s\n", (int) res, (int) errno, strerror(errno));
    }

    zmq_msg_close (&reply);
  }

  zmq_close (requester);
}




int main (int argc, char* argv[])
{
  int conc = 1;
  
  if (1 < argc) {
    loop = atoi(argv[1]);
  }

  if (2 < argc) {
    conc = atoi(argv[2]);
  }

  if (3 < argc) {
    connection = argv[3];
  }

  printf("Looping %d times pro thread, total %d\n", loop, loop * conc);
  printf("Concurrency %d\n", conc);
  printf("Connection %s\n", connection);

  context = zmq_init(16);

  // Socket to talk to server
  printf ("Connecting to hello world server…\n");

  int t1 = time(0);
  pthread_t* threads = (pthread_t*) malloc(sizeof(pthread_t) * conc);

  for (int i = 0;  i < conc;  ++i) {
    pthread_create(&threads[i], 0, &ThreadStarter, 0);
  }

  for (int i = 0;  i < conc;  ++i) {
    pthread_join(threads[i], 0);
  }

  int t2 = time(0);
  int rt = t2 - t1;

  printf("runtime %d, req/sec %f\n", rt, 1.0 * loop * conc / rt);

  zmq_term (context);
  return 0;
}
