Asynchronous Tasks
==================

### maximal queue size

Maximum size of the queue for requests: `--server.maximal-queue-size
size`

Specifies the maximum *size* of the queue for asynchronous task
execution. If the queue already contains *size* tasks, new tasks will
be rejected until other tasks are popped from the queue. Setting this
value may help preventing from running out of memory if the queue is
filled up faster than the server can process requests.
