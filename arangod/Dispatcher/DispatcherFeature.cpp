void DispatcherFeature::start() {
  buildStandardQueue(nrThreads, maxSize);
  buildAQLQueue(nrThreads, maxSize);
}
