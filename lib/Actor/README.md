# Actor framework

This actor frameworks implements the actor model. An actor model describes several units of computation (the actors) that run concurrently and possibly distributed. Actors only interact via messages and don't share any resources. Actors have a state which can be modified only via received messages. In this implementation, an actor has a message queue where receiving messages are buffered. Inside the actor, everything runs non-concurrently: An actor takes a message from the queue, then processes it, then takes the next message from the queue and processes that and so on. This makes the processing inside the actor much less error-prone.
Additionally, the framework abstracts the message sending away, which is especially useful in a distributed system: An actor sends a message to another actor, regardless wether this other actor lives on the same or a different server. The user does not need to know how to send a message and wether the receiver lives on the same or another server, this is done by the framework.

## Usage

On each server, there should exist one running actor runtime. This runtime keeps track of all actors running on this server and is responsible for spawning new actors and communication between actors (also across servers).

To create your own custom actor you need to define

- an actor state,
- message types the actor can receive (combined inside a variant)
- a handler that defines what happens when a specific message type arrives. 

Have a look at example actors used in the tests: TrivialActor.cpp, SpawnActor.cpp or PingPongActor.cpp.

### Receiving a message
Received messages are processed in the handler. Here - as a result of a received message - the actor's state can be changed, new actors can be spawned and messages can be send to other actors. Actors are typesafe, meaning they can only receive messages of their specified message types and some predefined error message types.

### Starting actors
When spawning an actor, a specific state and a start message have to be given. Then the runtime spawns an actor in the specific state and directly sends the start message to it.

### Identifying actors
Actors are identified via their PID, which consists of a server ID and an actor ID on that server. The handler can only send messages to other actors if it knows the receiving actor's PID. This PID can e.g. come from a received message or it is part of the actor's state.

### Finishing actors
When an actor is not needed any more, it can be finished. Then, the actor does not receive any new messages but finishes processing all messages in its queue. After that, garbage collection can then delete this actor from the runtime..

### Error handling
Errors can occur when messages are send, e.g. the receiving actor of a message cannot be found: Then an error message is sent back to the sending actor. The actor handler can either define the behaviors for individual error types or define a catch-all for all message types it does not define a handle for. The error types are defined in Message.h; the following errors can currently occur:

- UnknownMessage: The actor tried to send a message to another actor which does not recognize the sent message type,
- ActorNotFound: The receiving actor cannot be found on the receiving server or
- ServerNotFound: The receiving server cannot be found.

## Our implementation

### Actor

An actor receives messages (serialized or non-serialized) via the process method, tries to convert the message into its own message type or an error-message and - if successful - pushes this message to its queue. If the actor is idle (meaning the actor does not work off messages on the queue or has not scheduled any such work), this schedules a work cycle. In this work cycle, batchsize messages from the queue are processed sequentially. If the queue is still not empty afterward, the actor schedules a new work cycle. 

The scheduler is customizable, in ArangoDB we will use the ArangoDB scheduler. We use a lock-free multi-producer-single-consumer message queue (defined in MPSCQueue.h) such that we will not get any troubles when using the ArangoDB scheduler. The message queue queues messages of type MessageOrError which is a variant of the actor-specific messages and pre-defined errors. This way, the actor can receive messages and errors.

### Runtime

An actor runtime is running on each server and has a list of all actors running on this server - all having the same ActorBase interface. This runtime is responsible for

- spawning new actors,
- sending a message to another actor (which can be an actor on the same server - meaning inside the same runtime or on another server), and
- distributing messages coming from another server to the receiving actor in this runtime.

The runtime decides whether a message is sent locally (because the receiving actor runs on the same server) or externally (because the receiving actor runs on another server). If a message is sent to an actor on another server, the message is serialized to velocypack and an external dispatcher sends the serialized message. The external dispatcher is customizable: A very basic example of an external dispatcher is given in MultiRuntimeTest.cpp, for ArangoDB we have to write one that sends the message via our REST interface.

The runtime can garbage collect all actors which were set to finished and have processed their remaining message queue. 

### HandlerBase

The HandlerBase provides method calls of the runtime (spawning a new actor and dispatching a message to another actor) to a custom actor handler. This way, the custom handler needs to inherit from HandlerBase to spawn and dispatch without having a handle on the runtime itself. Additionally, the handler base provides the current actor state, the actor's own PID and the sender PID of the message it processes.

