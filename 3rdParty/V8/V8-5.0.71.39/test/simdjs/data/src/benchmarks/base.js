// SIMD Kernel Benchmark Harness
// Author: Peter Jensen

function Benchmark (config) {
  this.config            = config;
  this.initOk            = true;    // Initialize all properties used on a Benchmark object
  this.cleanupOk         = true;
  this.useAutoIterations = true;
  this.autoIterations    = 0;
  this.actualIterations  = 0;
  this.simdTime          = 0;
  this.nonSimdTime       = 0;
}

function Benchmarks () {
  this.benchmarks = [];
}

Benchmarks.prototype.add = function (benchmark) {
  this.benchmarks.push (benchmark);
  return this.benchmarks.length - 1;
}

Benchmarks.prototype.runOne = function (benchmark) {

  function timeKernel(kernel, iterations) {
    var start, stop;
    start = Date.now();
    kernel(iterations);
    stop = Date.now();
    return stop - start;
  }

  function computeIterations() {
    var desiredRuntime = 1000;  // milliseconds for longest running kernel
    var testIterations = 10;    // iterations used to determine time for desiredRuntime

    // Make the slowest kernel run for at least 500ms
    var simdTime = timeKernel(benchmark.config.kernelSimd, testIterations);
    var nonSimdTime = timeKernel(benchmark.config.kernelNonSimd, testIterations);
    var maxTime = simdTime > nonSimdTime ? simdTime : nonSimdTime;
    while (maxTime < 500) {
      testIterations *= 2;
      simdTime = timeKernel(benchmark.config.kernelSimd, testIterations);
      nonSimdTime = timeKernel(benchmark.config.kernelNonSimd, testIterations);
      maxTime = simdTime > nonSimdTime ? simdTime : nonSimdTime;
    }
    maxTime = simdTime > nonSimdTime ? simdTime : nonSimdTime;

    // Compute iteration count for 1 second run of slowest kernel
    var iterations = Math.ceil(desiredRuntime * testIterations / maxTime);
    return iterations;
  }

  // Initialize the kernels and check the correctness status
  if (!benchmark.config.kernelInit()) {
    benchmark.initOk = false;
    return false;
  }

  // Determine how many iterations to use.
  if (benchmark.useAutoIterations) {
    benchmark.autoIterations = computeIterations();
    benchmark.actualIterations = benchmark.autoIterations;
  }
  else {
    benchmark.actualIterations = benchmark.config.kernelIterations;
  }

  // Run the SIMD kernel
  benchmark.simdTime = timeKernel(benchmark.config.kernelSimd, benchmark.actualIterations);

  // Run the non-SIMD kernel
  benchmark.nonSimdTime = timeKernel(benchmark.config.kernelNonSimd, benchmark.actualIterations);

  // Do the final sanity check
  if (!benchmark.config.kernelCleanup()) {
    benchmark.cleanupOk = false;
    return false;
  }

  return true;
}

Benchmarks.prototype.report = function (benchmark, outputFunctions) {

  function fillRight(str, width) {
    str += ""; // make sure it's a string
    while (str.length < width) {
      str += " ";
    }
    return str;
  }

  function fillLeft(str, width) {
    str += ""; // make sure it's a string
    while (str.length < width) {
      str = " " + str;
    }
    return str;
  }

  if (!benchmark.initOk) {
    outputFunctions.notifyError(fillRight(benchmark.config.kernelName + ": ", 23) + "FAILED INIT");
    return;
  }
  if (!benchmark.cleanupOk) {
    outputFunctions.notifyError(fillRight(benchmark.config.kernelName + ": ", 23) + "FAILED CLEANUP");
    return;
  }

  var ratio = benchmark.nonSimdTime / benchmark.simdTime;
  outputFunctions.notifyResult(
    fillRight(benchmark.config.kernelName + ": ", 23) +
    "Iterations(" + fillLeft(benchmark.actualIterations, 10) + ")" +
    ", SIMD(" + fillLeft(benchmark.simdTime + "ms)", 8) +
    ", Non-SIMD(" + fillLeft(benchmark.nonSimdTime + "ms)", 8) +
    ", Speedup(" + ratio.toFixed(3) + ")");
}

Benchmarks.prototype.runAll = function (outputFunctions, useAutoIterations) {
  if (typeof useAutoIterations === "undefined") {
    useAutoIterations = false;
  }
  for (var i = 0, n = this.benchmarks.length; i < n; ++i) {
    var benchmark = this.benchmarks[i];
    benchmark.useAutoIterations = useAutoIterations;
    this.runOne(benchmark);
    this.report(benchmark, outputFunctions);
  }
}

var benchmarks = new Benchmarks ();
