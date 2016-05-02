// Transpose a 4x4 matrix
// Author: Peter Jensen
(function () {

  // Kernel configuration
  var kernelConfig = {
    kernelName:       "Transpose4x4",
    kernelInit:       init,
    kernelCleanup:    cleanup,
    kernelSimd:       simdTransposeN,
    kernelNonSimd:    transposeN,
    kernelIterations: 100000000
  };

  // Hook up to the harness
  benchmarks.add (new Benchmark (kernelConfig));

  // Global object allocations

  var src    = new Float32Array(16);
  var dst    = new Float32Array(16);
  var tsrc   = new Float32Array(16);

  var sel_ttff = SIMD.Bool32x4(true, true, false, false);

  function initMatrix(matrix, matrixTransposed) {
    for (var r = 0; r < 4; ++r) {
      var r4 = 4*r;
      for (var c = 0; c < 4; ++c) {
        matrix[r4 + c]            = r4 + c;
        matrixTransposed[r + c*4] = r4 + c;
      }
    }
  }

  function printMatrix(matrix) {
    for (var r = 0; r < 4; ++r) {
      var str = "";
      var ri = r*4;
      for (var c = 0; c < 4; ++c) {
        var value = matrix[ri + c];
        str += " " + value.toFixed(2);
      }
      print(str);
    }
  }

  function compareEqualMatrix(m1, m2) {
    for (var i = 0; i < 16; ++i) {
      if (m1[i] !== m2[i]) {
        return false;
      }
    }
    return true;
  }

  // Kernel Initializer
  function init () {
    initMatrix(src, tsrc);
    transposeN(1);
    if (!compareEqualMatrix (tsrc, dst)) {
      return false;
    }

    simdTransposeN(1);
//    printMatrix(dst);
    if (!compareEqualMatrix (tsrc, dst)) {
      return false;
    }

    return true;
  }

  // Kernel Cleanup
  function cleanup () {
    return init();
  }

  // SIMD version of the kernel with SIMD.Float32x4.shuffle operation
  function simdTransposeMix() {
    var src0     = SIMD.Float32x4.load(src, 0);
    var src1     = SIMD.Float32x4.load(src, 4);
    var src2     = SIMD.Float32x4.load(src, 8);
    var src3     = SIMD.Float32x4.load(src, 12);
    var dst0;
    var dst1;
    var dst2;
    var dst3;
    var tmp01;
    var tmp23;

    tmp01 = SIMD.Float32x4.shuffle(src0, src1, 0, 1, 4, 5);
    tmp23 = SIMD.Float32x4.shuffle(src2, src3, 0, 1, 4, 5);
    dst0  = SIMD.Float32x4.shuffle(tmp01, tmp23, 0, 2, 4, 6);
    dst1  = SIMD.Float32x4.shuffle(tmp01, tmp23, 1, 3, 5, 7);

    tmp01 = SIMD.Float32x4.shuffle(src0, src1, 2, 3, 6, 7);
    tmp23 = SIMD.Float32x4.shuffle(src2, src3, 2, 3, 6, 7);
    dst2  = SIMD.Float32x4.shuffle(tmp01, tmp23, 0, 2, 4, 6);
    dst3  = SIMD.Float32x4.shuffle(tmp01, tmp23, 1, 3, 5, 7);

    SIMD.Float32x4.store(dst, 0,  dst0);
    SIMD.Float32x4.store(dst, 4,  dst1);
    SIMD.Float32x4.store(dst, 8,  dst2);
    SIMD.Float32x4.store(dst, 12, dst3);
  }

  // SIMD version of the kernel
  function simdTranspose() {
    var src0     = SIMD.Float32x4.load(src, 0);
    var src1     = SIMD.Float32x4.load(src, 4);
    var src2     = SIMD.Float32x4.load(src, 8);
    var src3     = SIMD.Float32x4.load(src, 12);
    var dst0;
    var dst1;
    var dst2;
    var dst3;
    var tmp01;
    var tmp23;

    tmp01 = SIMD.Float32x4.select(sel_ttff, src0, SIMD.Float32x4.swizzle(src1, 0, 0, 0, 1));
    tmp23 = SIMD.Float32x4.select(sel_ttff, src2, SIMD.Float32x4.swizzle(src3, 0, 0, 0, 1));
    dst0  = SIMD.Float32x4.select(sel_ttff, SIMD.Float32x4.swizzle(tmp01, 0, 2, 0, 0), SIMD.Float32x4.swizzle(tmp23, 0, 0, 0, 2));
    dst1  = SIMD.Float32x4.select(sel_ttff, SIMD.Float32x4.swizzle(tmp01, 1, 3, 0, 0), SIMD.Float32x4.swizzle(tmp23, 0, 0, 1, 3));

    tmp01 = SIMD.Float32x4.select(sel_ttff, SIMD.Float32x4.swizzle(src0, 2, 3, 0, 0), src1);
    tmp23 = SIMD.Float32x4.select(sel_ttff, SIMD.Float32x4.swizzle(src2, 2, 3, 0, 0), src3);
    dst2  = SIMD.Float32x4.select(sel_ttff, SIMD.Float32x4.swizzle(tmp01, 0, 2, 0, 0), SIMD.Float32x4.swizzle(tmp23, 0, 0, 0, 2));
    dst3  = SIMD.Float32x4.select(sel_ttff, SIMD.Float32x4.swizzle(tmp01, 1, 3, 0, 0), SIMD.Float32x4.swizzle(tmp23, 0, 0, 1, 3));

    SIMD.Float32x4.store(dst, 0,  dst0);
    SIMD.Float32x4.store(dst, 4,  dst1);
    SIMD.Float32x4.store(dst, 8,  dst2);
    SIMD.Float32x4.store(dst, 12, dst3);
  }

  // Non SIMD version of the kernel
  function transpose() {
    dst[0]  = src[0];
    dst[1]  = src[4];
    dst[2]  = src[8];
    dst[3]  = src[12];
    dst[4]  = src[1];
    dst[5]  = src[5];
    dst[6]  = src[9];
    dst[7]  = src[13];
    dst[8]  = src[2];
    dst[9]  = src[6];
    dst[10] = src[10];
    dst[11] = src[14];
    dst[12] = src[3];
    dst[13] = src[7];
    dst[14] = src[11];
    dst[15] = src[15];
  }

  function simdTransposeN(n) {
    for (var i = 0; i < n; ++i) {
      var src0 = SIMD.Float32x4.load(src, 0);
      var src1 = SIMD.Float32x4.load(src, 4);
      var src2 = SIMD.Float32x4.load(src, 8);
      var src3 = SIMD.Float32x4.load(src, 12);
      var dst0;
      var dst1;
      var dst2;
      var dst3;
      var tmp01;
      var tmp23;

      tmp01 = SIMD.Float32x4.shuffle(src0, src1, 0, 1, 4, 5);
      tmp23 = SIMD.Float32x4.shuffle(src2, src3, 0, 1, 4, 5);
      dst0 = SIMD.Float32x4.shuffle(tmp01, tmp23, 0, 2, 4, 6);
      dst1 = SIMD.Float32x4.shuffle(tmp01, tmp23, 1, 3, 5, 7);

      tmp01 = SIMD.Float32x4.shuffle(src0, src1, 2, 3, 6, 7);
      tmp23 = SIMD.Float32x4.shuffle(src2, src3, 2, 3, 6, 7);
      dst2 = SIMD.Float32x4.shuffle(tmp01, tmp23, 0, 2, 4, 6);
      dst3 = SIMD.Float32x4.shuffle(tmp01, tmp23, 1, 3, 5, 7);

      SIMD.Float32x4.store(dst, 0,  dst0);
      SIMD.Float32x4.store(dst, 4,  dst1);
      SIMD.Float32x4.store(dst, 8,  dst2);
      SIMD.Float32x4.store(dst, 12, dst3);
    }
  }

  function transposeN(n) {
    for (var i = 0; i < n; ++i) {
      dst[0] = src[0];
      dst[1] = src[4];
      dst[2] = src[8];
      dst[3] = src[12];
      dst[4] = src[1];
      dst[5] = src[5];
      dst[6] = src[9];
      dst[7] = src[13];
      dst[8] = src[2];
      dst[9] = src[6];
      dst[10] = src[10];
      dst[11] = src[14];
      dst[12] = src[3];
      dst[13] = src[7];
      dst[14] = src[11];
      dst[15] = src[15];
    }
  }

} ());
