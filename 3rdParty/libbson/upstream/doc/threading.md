# Threading

Libbson is NOT thread-safe, but thread-aware.

You, the API consumer, are responsible for managing the data-structures in way that results two threads are not mutating at the same time.
Libbson provides a threading abstraction you may use or you can provide your own.
See `bson-thread.h` for more detailed information.
