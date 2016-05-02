// Kernel template
// Author: Peter Jensen
(function () {

  // Kernel configuration
  var kernelConfig = {
    kernelName:       "Test",
    kernelInit:       init,
    kernelCleanup:    cleanup,
    kernelSimd:       simd,
    kernelNonSimd:    nonSimd,
    kernelIterations: 100000000
  };

  // Hook up to the harness
  benchmarks.add (new Benchmark (kernelConfig));

  // Kernel Initializer
  function init () {
    // Do initial sanity check and initialize data for the kernels.
    // The sanity check should verify that the simd and nonSimd results
    // are the same.
    // It is recommended to do minimal object creation in the kernels
    // themselves.  If global data needs to be initialized, here would
    // be the place to do it.
    // If the sanity checks fails the kernels will not be executed
    // Returns:
    //   true:  First run (unoptimized) of the kernels passed
    //   false: First run (unoptimized) of the kernels failed
    return simd (1) === nonSimd (1);
  }

  // Kernel Cleanup
  function cleanup () {
    // Do final sanity check and perform cleanup.
    // This function is called when all the kernel iterations have been
    // executed, so they should be in their final optimized version.  The 
    // sanity check done during initialization will probably be of the 
    // initial unoptimized version.
    // Returns:
    //   true:  Last run (optimized) of the kernels passed
    //   false: last run (optimized) of the kernels failed
    return simd (1) === nonSimd (1);
  }

  // SIMD version of the kernel
  function simd (n) {
    var s = 0;
    for (var i = 0; i < n; ++i) {
      s += i;
    }
    return s;
  }

  // Non SIMD version of the kernel
  function nonSimd (n) {
    var s = 0;
    for (var i = 0; i < n; ++i) {
      s += i;
    }
    return s;
  }

} ());
