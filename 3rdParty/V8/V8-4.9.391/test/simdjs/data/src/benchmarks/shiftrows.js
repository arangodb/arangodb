// ShiftRows is a hot function in the implementation of the Rijndael cipher
// For documentation see: http://asmaes.sourceforge.net/rijndael/rijndaelImplementation.pdf
// Author: Peter Jensen
(function() {

  // Kernel configuration
  var kernelConfig = {
    kernelName:       "ShiftRows",
    kernelInit:       init,
    kernelCleanup:    cleanup,
    kernelSimd:       simdShiftRowsN,
    kernelNonSimd:    shiftRowsN,
    kernelIterations: 1000
  };

  // Hook up to the harness
  benchmarks.add(new Benchmark(kernelConfig));

  // Do the object allocations globally, so the performance of the kernel
  // functions aren't overshadowed by object creations

  var state   = new Int32Array(16);    // 4x4 state matrix
  var temp    = new Int32Array (1000); // Big enough for 1000 columns

  function printState() {
    for (var r = 0; r < 4; ++r) {
      var str = "";
      var ri = r*4;
      for (var c = 0; c < 4; ++c) {
        var value = state[ri + c];
        if (value < 10) {
          str += " ";
        }
        str += " " + state[ri + c];
      }
      print(str);
    }
  }

  // initialize the 4x4 state matrix
  function initState() {
    for (var i = 0; i < 16; ++i) {
      state[i] = i;
    }
  }

  // Verify the result of calling shiftRows(state, 4)
  function checkState() {
    var expected = new Uint32Array(
      [ 0,  1,  2,  3,
        5,  6,  7,  4,
       10, 11,  8,  9,
       15, 12, 13, 14]);
    for (var i = 0; i < 16; ++i) {
      if (state[i] !== expected[i]) {
        return false;
      }
    }
    return true;
  }

  function init() {
    // Check that shiftRows yields the right result
    initState();
    shiftRowsN(1);
    if (!checkState()) {
      return false;
    }

    // Check that simdShiftRows yields the right result
    initState();
    simdShiftRowsN(1);
    if (!checkState()) {
      return false;
    }
    return true;
  }

  function cleanup() {
    return init(); // Sanity checking before and after are the same
  }

  // This is the typical implementation of the shiftRows function
  function shiftRows(state, Nc) {
    for (var r = 1; r < 4; ++r) {
      var ri = r*Nc; // get the starting index of row 'r'
      var c;
      for (c = 0; c < Nc; ++c) {
        temp[c] = state[ri + ((c + r) % Nc)];
      }
      for (c = 0; c < Nc; ++c) {
        state[ri + c] = temp[c];
      }
    }
  }

  // The SIMD optimized version of the shiftRows function
  // The function is special cased for a 4 column setting (Nc == 4).
  // This is the value used for AES blocks (see documentation for details)
  function simdShiftRows(state, Nc) {
    if (Nc !== 4) {
      shiftRows(state, Nc);
    }
    for (var r = 1; r < 4; ++r) {
      var rx4 = SIMD.Int32x4.load(state, r << 2);
      if (r == 1) {
        SIMD.Int32x4.store(state, 4, SIMD.Int32x4.swizzle(rx4, 1, 2, 3, 0));
      }
      else if (r == 2) {
        SIMD.Int32x4.store(state, 8, SIMD.Int32x4.swizzle(rx4, 2, 3, 0, 1));
      }
      else { // r == 3
        SIMD.Int32x4.store(state, 12, SIMD.Int32x4.swizzle(rx4, 3, 0, 1, 2));
      }
    }
  }

  function shiftRowsN(iterations) {
    for (var i = 0; i < iterations; ++i) {
      shiftRows(state, 4);
    }
  }

  function simdShiftRowsN(iterations) {
    for (var i = 0; i < iterations; ++i) {
      simdShiftRows(state, 4);
    }
  }
} ());
