ApplicationScheduler::ApplicationScheduler(ApplicationServer* applicationServer)
    : ApplicationFeature("scheduler"),
      _applicationServer(applicationServer),
      _scheduler(nullptr),
      _tasks(),
      _reportInterval(0.0),
      _multiSchedulerAllowed(true),
      _nrSchedulerThreads(4),
      _backend(0),
      _descriptorMinimum(1024),
      _disableControlCHandler(false) {}

ApplicationScheduler::~ApplicationScheduler() {
  Scheduler::SCHEDULER.release();  // TODO(fc) XXX remove this
  delete _scheduler;
}
