// Simple performance test of SIMD.add operation.  Use SIMD.add to average up elements
// in a Float32Array. Compare to scalar implementation of same function.
// Author: Peter Jensen

(function () {

  // Kernel configuration
  var kernelConfig = {
    kernelName:       "AverageFloat32x4",
    kernelInit:       initArray,
    kernelCleanup:    cleanup,
    kernelSimd:       simdAverage,
    kernelNonSimd:    average,
    kernelIterations: 1000
  };

  // Hook up to the harness
  benchmarks.add(new Benchmark(kernelConfig));

  // Benchmark data, initialization and kernel functions
  var a   = new Float32Array(10000);

  function sanityCheck() {
     return Math.abs(average(1) - simdAverage(1)) < 0.0001;
  }

  function initArray() {
    var j = 0;
    for (var i = 0, l = a.length; i < l; ++i) {
      a[i] = 0.1;
    }
    // Check that the two kernel functions yields the same result, roughly
    // Account for the fact that the simdAverage() is computed using float32
    // precision and the average() is using double precision
    return sanityCheck();
  }

  function cleanup() {
    return sanityCheck();
  }

  function average(n) {
    for (var i = 0; i < n; ++i) {
      var sum = 0.0;
      for (var j = 0, l = a.length; j < l; ++j) {
        sum += a[j];
      }
    }
    return sum/a.length;
  }

  function simdAverage(n) {
    var a_length = a.length;
    for (var i = 0; i < n; ++i) {
      var sum4 = SIMD.Float32x4.splat(0.0);
      for (var j = 0; j < a_length; j += 4) {
        sum4 = SIMD.Float32x4.add(sum4, SIMD.Float32x4.load(a, j));
      }
    }
    return (SIMD.Float32x4.extractLane(sum4, 0) +
        SIMD.Float32x4.extractLane(sum4, 1) +
        SIMD.Float32x4.extractLane(sum4, 2) +
        SIMD.Float32x4.extractLane(sum4, 3)) / a.length;
  }

} ());
