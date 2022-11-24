#pragma once

// This scheduler just runs any function synchronously as soon as it comes in.
struct Scheduler {
  auto operator()(auto fn) { fn(); }
};

extern Scheduler testScheduler;
