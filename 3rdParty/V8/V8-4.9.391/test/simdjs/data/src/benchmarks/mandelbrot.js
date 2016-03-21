// Mandelbrot Benchmark
// Author: Peter Jensen
(function () {

  // Kernel configuration
  var kernelConfig = {
    kernelName:       "Mandelbrot",
    kernelInit:       initMandelbrot,
    kernelCleanup:    cleanupMandelbrot,
    kernelSimd:       simdMandelbrot,
    kernelNonSimd:    nonSimdMandelbrot,
    kernelIterations: 10000
  };

  // Hook up to the harness
  benchmarks.add (new Benchmark (kernelConfig));

  function Float32x4ToString (f4) {
    return "[" + SIMD.Float32x4.extractLane(f4, 0) + "," +
        SIMD.Float32x4.extractLane(f4, 1) + "," +
        SIMD.Float32x4.extractLane(f4, 2) + "," +
        SIMD.Float32x4.extractLane(f4, 3) + "]";
  }

  function Int32x4ToString (i4) {
    return "[" + SIMD.Int32x4.extractLane(i4, 0) + "," +
        SIMD.Int32x4.extractLane(i4, 1) + "," +
        SIMD.Int32x4.extractLane(i4, 2) + "," +
        SIMD.Int32x4.extractLane(i4, 3) + "]";
  }

  function mandelx1(c_re, c_im, max_iterations) {
    var z_re = c_re,
        z_im = c_im,
        i;
    for (i = 0; i < max_iterations; i++) {
      var z_re2 = z_re*z_re;
      var z_im2 = z_im*z_im;
      if (z_re2 + z_im2 > 4.0)
        break;

      var new_re = z_re2 - z_im2;
      var new_im = 2.0 * z_re * z_im;
      z_re = c_re + new_re;
      z_im = c_im + new_im;
    }
    return i;
  }

  function mandelx4(c_re4, c_im4, max_iterations) {
    var z_re4  = c_re4;
    var z_im4  = c_im4;
    var four4  = SIMD.Float32x4.splat (4.0);
    var two4   = SIMD.Float32x4.splat (2.0);
    var count4 = SIMD.Int32x4.splat (0);
    var one4   = SIMD.Int32x4.splat (1);

    for (var i = 0; i < max_iterations; ++i) {
      var z_re24 = SIMD.Float32x4.mul (z_re4, z_re4);
      var z_im24 = SIMD.Float32x4.mul (z_im4, z_im4);

      var mi4    = SIMD.Float32x4.lessThanOrEqual (SIMD.Float32x4.add (z_re24, z_im24), four4);
      // if all 4 values are greater than 4.0, there's no reason to continue
      if (mi4.signMask === 0x00) {
        break;
      }

      var new_re4 = SIMD.Float32x4.sub(z_re24, z_im24);
      var new_im4 = SIMD.Float32x4.mul(SIMD.Float32x4.mul (two4, z_re4), z_im4);
      z_re4       = SIMD.Float32x4.add(c_re4, new_re4);
      z_im4       = SIMD.Float32x4.add(c_im4, new_im4);
      count4      = SIMD.Int32x4.add(count4, SIMD.Int32x4.and (mi4, one4));
    }
    return count4;
  }

  function sanityCheck() {
    var simd    = simdMandelbrot(1);
    var nonSimd = nonSimdMandelbrot(1);
    if (simd.length !== nonSimd.length) {
      return false;
    }
    for (var i = 0, n = simd.length; i < n; ++i) {
      if (simd[i] !== nonSimd[i]) {
        return false;
      }
    }
    return true;
  }

  function initMandelbrot() {
    return sanityCheck();
  }

  function cleanupMandelbrot() {
    return sanityCheck();
  }

  // Non SIMD version of the kernel
  function nonSimdMandelbrot (n) {
    var result = new Array (4);
    for (var i = 0; i < n; ++i) {
      result [0] = mandelx1 (0.01, 0.01, 100);
      result [1] = mandelx1 (0.01, 0.01, 100);
      result [2] = mandelx1 (0.01, 0.01, 100);
      result [3] = mandelx1 (0.01, 0.01, 100);
    }
    return result;
  }

  // SIMD version of the kernel
  function simdMandelbrot (n) {
    var result = new Array (4);
    var vec0  = SIMD.Float32x4.splat (0.01);
    for (var i = 0; i < n; ++i) {
      var r = mandelx4 (vec0, vec0, 100);
      result [0] = SIMD.Int32x4.extractLane(r, 0);
      result [1] = SIMD.Int32x4.extractLane(r, 1);
      result [2] = SIMD.Int32x4.extractLane(r, 2);
      result [3] = SIMD.Int32x4.extractLane(r, 3);
    }
    return result;
  }

} ());
