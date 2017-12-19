Command-Line Options for Communication
======================================

### Scheduler threads
@startDocuBlock schedulerThreads


### Scheduler maximal queue size
@startDocuBlock schedulerMaximalQueueSize


### Scheduler backend
@startDocuBlock schedulerBackend


### Io backends
`--show-io-backends`

If this option is specified, then the server will list available backends and
exit. This option is useful only when used in conjunction with the option
scheduler.backend. An integer is returned (which is platform dependent) which
indicates available backends on your platform. See libev for further details and
for the meaning of the integer returned. This describes the allowed integers for
*scheduler.backend*, see [here](#commandline-options-for-communication) for details.
