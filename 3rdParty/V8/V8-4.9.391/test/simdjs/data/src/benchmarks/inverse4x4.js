// Kernel for doing a 4x4 Matrix Inverse operation
// Based on Cramer's rule.
// See: ftp://download.intel.com/design/PentiumIII/sml/24504301.pdf
// Author: Peter Jensen
(function () {

  // Kernel configuration
  var kernelConfig = {
    kernelName:       "Matrix4x4Inverse",
    kernelInit:       init,
    kernelCleanup:    cleanup,
    kernelSimd:       simdMatrixInverseN,
    kernelNonSimd:    nonSimdMatrixInverseN,
    kernelIterations: 1000
  };

  // Hook up to the harness
  benchmarks.add (new Benchmark (kernelConfig));

  // Global Variables
  var src    = new Float32Array(16);            // Source matrix
  var dst    = new Float32Array(16);            // Result matrix
  var tsrc   = new Float32Array(16);            // Transposed version of 'src'
  var tmp   = new Float32Array(12);             // Temporary array of multiply results
  var ident = new Float32Array(
                    [1,0,0,0,
                     0,1,0,0,
                     0,0,1,0,
                     0,0,0,1]);

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

  function initMatrix(matrix) {
    // These values were chosen somewhat randomly, but they will at least yield a solution.
    matrix [0]  =  0;  matrix[1] =  1; matrix[2]  =  2; matrix[3]  =  3;
    matrix [4]  = -1; matrix[5]  = -2; matrix[6]  = -3; matrix[7]  = -4;
    matrix [8]  =  0;  matrix[9] =  0; matrix[10] =  2; matrix[11] =  3;
    matrix [12] = -1; matrix[13] = -2; matrix[14] =  0; matrix[15] = -4;
  }

  function mulMatrix(dst, op1, op2) {
    for (var r = 0; r < 4; ++r) {
      for (var c = 0; c < 4; ++c) {
        var ri = 4*r;
        dst[ri + c] = op1[ri]*op2[c] + op1[ri+1]*op2[c+4] + op1[ri+2]*op2[c+8] + op1[ri+3]*op2[c+12]
      }
    }
  }

  function checkMatrix(matrix) {
    // when multiplied with the src matrix it should yield the identity matrix
    mulMatrix(tsrc, src, matrix);
    for (var i = 0; i < 16; ++i) {
      if (Math.abs (tsrc[i] - ident[i]) > 0.00001) {
        return false;
      }
    }
    // printMatrix (tsrc);
    return true;
  }

  // Kernel Initializer
  function init() {
    initMatrix(src);
    // printMatrix(src);
    nonSimdMatrixInverseN(1);
    // printMatrix(dst);
    if (!checkMatrix(dst)) {
      return false;
    }

    initMatrix(src);
    simdMatrixInverseN(1);
    // printMatrix(dst);
    if (!checkMatrix(dst)) {
      return false;
    }

    return true;
  }

  function cleanup() {
    return init();
  }

  function simdMatrixInverse() {
    var src0, src1, src2, src3;
    var row0, row1, row2, row3;
    var tmp1;
    var minor0, minor1, minor2, minor3;
    var det;

    // Load the 4 rows
    var src0 = SIMD.Float32x4.load(src, 0);
    var src1 = SIMD.Float32x4.load(src, 4);
    var src2 = SIMD.Float32x4.load(src, 8);
    var src3 = SIMD.Float32x4.load(src, 16);

    // Transpose the source matrix.  Sort of.  Not a true transpose operation

    tmp1 = SIMD.Float32x4.shuffle(src0, src1, 0, 1, 4, 5);
    row1 = SIMD.Float32x4.shuffle(src2, src3, 0, 1, 4, 5);
    row0 = SIMD.Float32x4.shuffle(tmp1, row1, 0, 2, 4, 6);
    row1 = SIMD.Float32x4.shuffle(row1, tmp1, 1, 3, 5, 7);

    tmp1 = SIMD.Float32x4.shuffle(src0, src1, 2, 3, 6, 7);
    row3 = SIMD.Float32x4.shuffle(src2, src3, 2, 3, 6, 7);
    row2 = SIMD.Float32x4.shuffle(tmp1, row3, 0, 2, 4, 6);
    row3 = SIMD.Float32x4.shuffle(row3, tmp1, 1, 3, 5, 7);

    // This is a true transposition, but it will lead to an incorrect result

    //tmp1 = SIMD.Float32x4.shuffle(src0, src1, 0, 1, 4, 5);
    //tmp2 = SIMD.Float32x4.shuffle(src2, src3, 0, 1, 4, 5);
    //row0  = SIMD.Float32x4.shuffle(tmp1, tmp2, 0, 2, 4, 6);
    //row1  = SIMD.Float32x4.shuffle(tmp1, tmp2, 1, 3, 5, 7);

    //tmp1 = SIMD.Float32x4.shuffle(src0, src1, 2, 3, 6, 7);
    //tmp2 = SIMD.Float32x4.shuffle(src2, src3, 2, 3, 6, 7);
    //row2  = SIMD.Float32x4.shuffle(tmp1, tmp2, 0, 2, 4, 6);
    //row3  = SIMD.Float32x4.shuffle(tmp1, tmp2, 1, 3, 5, 7);

    // ----
    tmp1   = SIMD.Float32x4.mul(row2, row3);
    tmp1   = SIMD.Float32x4.swizzle(tmp1, 1, 0, 3, 2); // 0xB1 = 10110001
    minor0 = SIMD.Float32x4.mul(row1, tmp1);
    minor1 = SIMD.Float32x4.mul(row0, tmp1);
    tmp1   = SIMD.Float32x4.swizzle(tmp1, 2, 3, 0, 1); // 0x4E = 01001110
    minor0 = SIMD.Float32x4.sub(SIMD.Float32x4.mul(row1, tmp1), minor0);
    minor1 = SIMD.Float32x4.sub(SIMD.Float32x4.mul(row0, tmp1), minor1);
    minor1 = SIMD.Float32x4.swizzle(minor1, 2, 3, 0, 1); // 0x4E = 01001110

    // ----
    tmp1   = SIMD.Float32x4.mul(row1, row2);
    tmp1   = SIMD.Float32x4.swizzle(tmp1, 1, 0, 3, 2); // 0xB1 = 10110001
    minor0 = SIMD.Float32x4.add(SIMD.Float32x4.mul(row3, tmp1), minor0);
    minor3 = SIMD.Float32x4.mul(row0, tmp1);
    tmp1   = SIMD.Float32x4.swizzle(tmp1, 2, 3, 0, 1); // 0x4E = 01001110
    minor0 = SIMD.Float32x4.sub(minor0, SIMD.Float32x4.mul(row3, tmp1));
    minor3 = SIMD.Float32x4.sub(SIMD.Float32x4.mul(row0, tmp1), minor3);
    minor3 = SIMD.Float32x4.swizzle(minor3, 2, 3, 0, 1); // 0x4E = 01001110

    // ----
    tmp1   = SIMD.Float32x4.mul(SIMD.Float32x4.swizzle(row1, 2, 3, 0, 1), row3); // 0x4E = 01001110
    tmp1   = SIMD.Float32x4.swizzle(tmp1, 1, 0, 3, 2); // 0xB1 = 10110001
    row2   = SIMD.Float32x4.swizzle(row2, 2, 3, 0, 1);  // 0x4E = 01001110
    minor0 = SIMD.Float32x4.add(SIMD.Float32x4.mul(row2, tmp1), minor0);
    minor2 = SIMD.Float32x4.mul(row0, tmp1);
    tmp1   = SIMD.Float32x4.swizzle(tmp1, 2, 3, 0, 1); // 0x4E = 01001110
    minor0 = SIMD.Float32x4.sub(minor0, SIMD.Float32x4.mul(row2, tmp1));
    minor2 = SIMD.Float32x4.sub(SIMD.Float32x4.mul(row0, tmp1), minor2);
    minor2 = SIMD.Float32x4.swizzle(minor2, 2, 3, 0, 1); // 0x4E = 01001110

    // ----
    tmp1   = SIMD.Float32x4.mul(row0, row1);
    tmp1   = SIMD.Float32x4.swizzle(tmp1, 1, 0, 3, 2); // 0xB1 = 10110001
    minor2 = SIMD.Float32x4.add(SIMD.Float32x4.mul(row3, tmp1), minor2);
    minor3 = SIMD.Float32x4.sub(SIMD.Float32x4.mul(row2, tmp1), minor3);
    tmp1   = SIMD.Float32x4.swizzle(tmp1, 2, 3, 0, 1); // 0x4E = 01001110
    minor2 = SIMD.Float32x4.sub(SIMD.Float32x4.mul(row3, tmp1), minor2);
    minor3 = SIMD.Float32x4.sub(minor3, SIMD.Float32x4.mul(row2, tmp1));

    // ----
    tmp1   = SIMD.Float32x4.mul(row0, row3);
    tmp1   = SIMD.Float32x4.swizzle(tmp1, 1, 0, 3, 2); // 0xB1 = 10110001
    minor1 = SIMD.Float32x4.sub(minor1, SIMD.Float32x4.mul(row2, tmp1));
    minor2 = SIMD.Float32x4.add(SIMD.Float32x4.mul(row1, tmp1), minor2);
    tmp1   = SIMD.Float32x4.swizzle(tmp1, 2, 3, 0, 1); // 0x4E = 01001110
    minor1 = SIMD.Float32x4.add(SIMD.Float32x4.mul(row2, tmp1), minor1);
    minor2 = SIMD.Float32x4.sub(minor2, SIMD.Float32x4.mul(row1, tmp1));

    // ----
    tmp1   = SIMD.Float32x4.mul(row0, row2);
    tmp1   = SIMD.Float32x4.swizzle(tmp1, 1, 0, 3, 2); // 0xB1 = 10110001
    minor1 = SIMD.Float32x4.add(SIMD.Float32x4.mul(row3, tmp1), minor1);
    minor3 = SIMD.Float32x4.sub(minor3, SIMD.Float32x4.mul(row1, tmp1));
    tmp1   = SIMD.Float32x4.swizzle(tmp1, 2, 3, 0, 1); // 0x4E = 01001110
    minor1 = SIMD.Float32x4.sub(minor1, SIMD.Float32x4.mul(row3, tmp1));
    minor3 = SIMD.Float32x4.add(SIMD.Float32x4.mul(row1, tmp1), minor3);

    // Compute determinant
    det   = SIMD.Float32x4.mul(row0, minor0);
    det   = SIMD.Float32x4.add(SIMD.Float32x4.swizzle(det, 2, 3, 0, 1), det); // 0x4E = 01001110
    det   = SIMD.Float32x4.add(SIMD.Float32x4.swizzle(det, 1, 0, 3, 2), det); // 0xB1 = 10110001
    tmp1  = SIMD.Float32x4.reciprocalApproximation(det);
    det   = SIMD.Float32x4.sub(SIMD.Float32x4.add(tmp1, tmp1), SIMD.Float32x4.mul(det, SIMD.Float32x4.mul(tmp1, tmp1)));
    det   = SIMD.Float32x4.swizzle(det, 0, 0, 0, 0);

    // These shuffles aren't necessary if the faulty transposition is done
    // up at the top of this function.
    //minor0 = SIMD.Float32x4.swizzle(minor0, 2, 1, 0, 3);
    //minor1 = SIMD.Float32x4.swizzle(minor1, 2, 1, 0, 3);
    //minor2 = SIMD.Float32x4.swizzle(minor2, 2, 1, 0, 3);
    //minor3 = SIMD.Float32x4.swizzle(minor3, 2, 1, 0, 3);

    // Compute final values by multiplying with 1/det
    minor0 = SIMD.Float32x4.mul(det, minor0);
    minor1 = SIMD.Float32x4.mul(det, minor1);
    minor2 = SIMD.Float32x4.mul(det, minor2);
    minor3 = SIMD.Float32x4.mul(det, minor3);

    SIMD.Float32x4.store(dst, 0,  minor0);
    SIMD.Float32x4.store(dst, 4,  minor1);
    SIMD.Float32x4.store(dst, 8,  minor2);
    SIMD.Float32x4.store(dst, 12, minor3);
  }

  function nonSimdMatrixInverse() {

    // Transpose the source matrix
    for (var i = 0; i < 4; i++) {
      tsrc[i]      = src[i*4];
      tsrc[i + 4]  = src[i*4 + 1];
      tsrc[i + 8]  = src[i*4 + 2];
      tsrc[i + 12] = src[i*4 + 3];
    }

    // Calculate pairs for first 8 elements (cofactors)
    tmp[0] = tsrc[10] * tsrc[15];
    tmp[1] = tsrc[11] * tsrc[14];
    tmp[2] = tsrc[9]  * tsrc[15];
    tmp[3] = tsrc[11] * tsrc[13];
    tmp[4] = tsrc[9]  * tsrc[14];
    tmp[5] = tsrc[10] * tsrc[13];
    tmp[6] = tsrc[8]  * tsrc[15];
    tmp[7] = tsrc[11] * tsrc[12];
    tmp[8] = tsrc[8]  * tsrc[14];
    tmp[9] = tsrc[10] * tsrc[12];
    tmp[10] = tsrc[8] * tsrc[13];
    tmp[11] = tsrc[9] * tsrc[12];

    // calculate first 8 elements (cofactors)
    dst[0]  = tmp[0]*tsrc[5] + tmp[3]*tsrc[6] + tmp[4]*tsrc[7];
    dst[0] -= tmp[1]*tsrc[5] + tmp[2]*tsrc[6] + tmp[5]*tsrc[7];
    dst[1]  = tmp[1]*tsrc[4] + tmp[6]*tsrc[6] + tmp[9]*tsrc[7];
    dst[1] -= tmp[0]*tsrc[4] + tmp[7]*tsrc[6] + tmp[8]*tsrc[7];
    dst[2]  = tmp[2]*tsrc[4] + tmp[7]*tsrc[5] + tmp[10]*tsrc[7];
    dst[2] -= tmp[3]*tsrc[4] + tmp[6]*tsrc[5] + tmp[11]*tsrc[7];
    dst[3]  = tmp[5]*tsrc[4] + tmp[8]*tsrc[5] + tmp[11]*tsrc[6];
    dst[3] -= tmp[4]*tsrc[4] + tmp[9]*tsrc[5] + tmp[10]*tsrc[6];
    dst[4]  = tmp[1]*tsrc[1] + tmp[2]*tsrc[2] + tmp[5]*tsrc[3];
    dst[4] -= tmp[0]*tsrc[1] + tmp[3]*tsrc[2] + tmp[4]*tsrc[3];
    dst[5]  = tmp[0]*tsrc[0] + tmp[7]*tsrc[2] + tmp[8]*tsrc[3];
    dst[5] -= tmp[1]*tsrc[0] + tmp[6]*tsrc[2] + tmp[9]*tsrc[3];
    dst[6]  = tmp[3]*tsrc[0] + tmp[6]*tsrc[1] + tmp[11]*tsrc[3];
    dst[6] -= tmp[2]*tsrc[0] + tmp[7]*tsrc[1] + tmp[10]*tsrc[3];
    dst[7]  = tmp[4]*tsrc[0] + tmp[9]*tsrc[1] + tmp[10]*tsrc[2];
    dst[7] -= tmp[5]*tsrc[0] + tmp[8]*tsrc[1] + tmp[11]*tsrc[2];

    // calculate pairs for second 8 elements (cofactors)
    tmp[0]  = tsrc[2]*tsrc[7];
    tmp[1]  = tsrc[3]*tsrc[6];
    tmp[2]  = tsrc[1]*tsrc[7];
    tmp[3]  = tsrc[3]*tsrc[5];
    tmp[4]  = tsrc[1]*tsrc[6];
    tmp[5]  = tsrc[2]*tsrc[5];
    tmp[6]  = tsrc[0]*tsrc[7];
    tmp[7]  = tsrc[3]*tsrc[4];
    tmp[8]  = tsrc[0]*tsrc[6];
    tmp[9]  = tsrc[2]*tsrc[4];
    tmp[10] = tsrc[0]*tsrc[5];
    tmp[11] = tsrc[1]*tsrc[4];

    // calculate second 8 elements (cofactors)
    dst[8]  = tmp[0]*tsrc[13]  + tmp[3]*tsrc[14]  + tmp[4]*tsrc[15];
    dst[8] -= tmp[1]*tsrc[13]  + tmp[2]*tsrc[14]  + tmp[5]*tsrc[15];
    dst[9]  = tmp[1]*tsrc[12]  + tmp[6]*tsrc[14]  + tmp[9]*tsrc[15];
    dst[9] -= tmp[0]*tsrc[12]  + tmp[7]*tsrc[14]  + tmp[8]*tsrc[15];
    dst[10] = tmp[2]*tsrc[12]  + tmp[7]*tsrc[13]  + tmp[10]*tsrc[15];
    dst[10]-= tmp[3]*tsrc[12]  + tmp[6]*tsrc[13]  + tmp[11]*tsrc[15];
    dst[11] = tmp[5]*tsrc[12]  + tmp[8]*tsrc[13]  + tmp[11]*tsrc[14];
    dst[11]-= tmp[4]*tsrc[12]  + tmp[9]*tsrc[13]  + tmp[10]*tsrc[14];
    dst[12] = tmp[2]*tsrc[10]  + tmp[5]*tsrc[11]  + tmp[1]*tsrc[9];
    dst[12]-= tmp[4]*tsrc[11]  + tmp[0]*tsrc[9]   + tmp[3]*tsrc[10];
    dst[13] = tmp[8]*tsrc[11]  + tmp[0]*tsrc[8]   + tmp[7]*tsrc[10];
    dst[13]-= tmp[6]*tsrc[10]  + tmp[9]*tsrc[11]  + tmp[1]*tsrc[8];
    dst[14] = tmp[6]*tsrc[9]   + tmp[11]*tsrc[11] + tmp[3]*tsrc[8];
    dst[14]-= tmp[10]*tsrc[11] + tmp[2]*tsrc[8]   + tmp[7]*tsrc[9];
    dst[15] = tmp[10]*tsrc[10] + tmp[4]*tsrc[8]   + tmp[9]*tsrc[9];
    dst[15]-= tmp[8]*tsrc[9]   + tmp[11]*tsrc[10] + tmp[5]*tsrc[8];

    // calculate determinant
    var det = tsrc[0]*dst[0] + tsrc[1]*dst[1] + tsrc[2]*dst[2] + tsrc[3]*dst[3];

    // calculate matrix inverse
    det = 1/det;
    for (var j = 0; j < 16; j++) {
      dst[j] *= det;
    }

  }

  // SIMD version of the kernel
  function simdMatrixInverseN(n) {
    for (var iterations = 0; iterations < n; ++iterations) {
      var src0, src1, src2, src3;
      var row0, row1, row2, row3;
      var tmp1;
      var minor0, minor1, minor2, minor3;
      var det;

      // Load the 4 rows
      var src0 = SIMD.Float32x4.load(src, 0);
      var src1 = SIMD.Float32x4.load(src, 4);
      var src2 = SIMD.Float32x4.load(src, 8);
      var src3 = SIMD.Float32x4.load(src, 12);

      // Transpose the source matrix.  Sort of.  Not a true transpose operation

      tmp1 = SIMD.Float32x4.shuffle(src0, src1, 0, 1, 4, 5);
      row1 = SIMD.Float32x4.shuffle(src2, src3, 0, 1, 4, 5);
      row0 = SIMD.Float32x4.shuffle(tmp1, row1, 0, 2, 4, 6);
      row1 = SIMD.Float32x4.shuffle(row1, tmp1, 1, 3, 5, 7);

      tmp1 = SIMD.Float32x4.shuffle(src0, src1, 2, 3, 6, 7);
      row3 = SIMD.Float32x4.shuffle(src2, src3, 2, 3, 6, 7);
      row2 = SIMD.Float32x4.shuffle(tmp1, row3, 0, 2, 4, 6);
      row3 = SIMD.Float32x4.shuffle(row3, tmp1, 1, 3, 5, 7);

      // This is a true transposition, but it will lead to an incorrect result

      //tmp1 = SIMD.Float32x4.shuffle(src0, src1, 0, 1, 4, 5);
      //tmp2 = SIMD.Float32x4.shuffle(src2, src3, 0, 1, 4, 5);
      //row0  = SIMD.Float32x4.shuffle(tmp1, tmp2, 0, 2, 4, 6);
      //row1  = SIMD.Float32x4.shuffle(tmp1, tmp2, 1, 3, 5, 7);

      //tmp1 = SIMD.Float32x4.shuffle(src0, src1, 2, 3, 6, 7);
      //tmp2 = SIMD.Float32x4.shuffle(src2, src3, 2, 3, 6, 7);
      //row2  = SIMD.Float32x4.shuffle(tmp1, tmp2, 0, 2, 4, 6);
      //row3  = SIMD.Float32x4.shuffle(tmp1, tmp2, 1, 3, 5, 7);

      // ----
      tmp1 = SIMD.Float32x4.mul(row2, row3);
      tmp1 = SIMD.Float32x4.swizzle(tmp1, 1, 0, 3, 2); // 0xB1 = 10110001
      minor0 = SIMD.Float32x4.mul(row1, tmp1);
      minor1 = SIMD.Float32x4.mul(row0, tmp1);
      tmp1 = SIMD.Float32x4.swizzle(tmp1, 2, 3, 0, 1); // 0x4E = 01001110
      minor0 = SIMD.Float32x4.sub(SIMD.Float32x4.mul(row1, tmp1), minor0);
      minor1 = SIMD.Float32x4.sub(SIMD.Float32x4.mul(row0, tmp1), minor1);
      minor1 = SIMD.Float32x4.swizzle(minor1, 2, 3, 0, 1); // 0x4E = 01001110

      // ----
      tmp1 = SIMD.Float32x4.mul(row1, row2);
      tmp1 = SIMD.Float32x4.swizzle(tmp1, 1, 0, 3, 2); // 0xB1 = 10110001
      minor0 = SIMD.Float32x4.add(SIMD.Float32x4.mul(row3, tmp1), minor0);
      minor3 = SIMD.Float32x4.mul(row0, tmp1);
      tmp1 = SIMD.Float32x4.swizzle(tmp1, 2, 3, 0, 1); // 0x4E = 01001110
      minor0 = SIMD.Float32x4.sub(minor0, SIMD.Float32x4.mul(row3, tmp1));
      minor3 = SIMD.Float32x4.sub(SIMD.Float32x4.mul(row0, tmp1), minor3);
      minor3 = SIMD.Float32x4.swizzle(minor3, 2, 3, 0, 1); // 0x4E = 01001110

      // ----
      tmp1 = SIMD.Float32x4.mul(SIMD.Float32x4.swizzle(row1, 2, 3, 0, 1), row3); // 0x4E = 01001110
      tmp1 = SIMD.Float32x4.swizzle(tmp1, 1, 0, 3, 2); // 0xB1 = 10110001
      row2 = SIMD.Float32x4.swizzle(row2, 2, 3, 0, 1);  // 0x4E = 01001110
      minor0 = SIMD.Float32x4.add(SIMD.Float32x4.mul(row2, tmp1), minor0);
      minor2 = SIMD.Float32x4.mul(row0, tmp1);
      tmp1 = SIMD.Float32x4.swizzle(tmp1, 2, 3, 0, 1); // 0x4E = 01001110
      minor0 = SIMD.Float32x4.sub(minor0, SIMD.Float32x4.mul(row2, tmp1));
      minor2 = SIMD.Float32x4.sub(SIMD.Float32x4.mul(row0, tmp1), minor2);
      minor2 = SIMD.Float32x4.swizzle(minor2, 2, 3, 0, 1); // 0x4E = 01001110

      // ----
      tmp1 = SIMD.Float32x4.mul(row0, row1);
      tmp1 = SIMD.Float32x4.swizzle(tmp1, 1, 0, 3, 2); // 0xB1 = 10110001
      minor2 = SIMD.Float32x4.add(SIMD.Float32x4.mul(row3, tmp1), minor2);
      minor3 = SIMD.Float32x4.sub(SIMD.Float32x4.mul(row2, tmp1), minor3);
      tmp1 = SIMD.Float32x4.swizzle(tmp1, 2, 3, 0, 1); // 0x4E = 01001110
      minor2 = SIMD.Float32x4.sub(SIMD.Float32x4.mul(row3, tmp1), minor2);
      minor3 = SIMD.Float32x4.sub(minor3, SIMD.Float32x4.mul(row2, tmp1));

      // ----
      tmp1 = SIMD.Float32x4.mul(row0, row3);
      tmp1 = SIMD.Float32x4.swizzle(tmp1, 1, 0, 3, 2); // 0xB1 = 10110001
      minor1 = SIMD.Float32x4.sub(minor1, SIMD.Float32x4.mul(row2, tmp1));
      minor2 = SIMD.Float32x4.add(SIMD.Float32x4.mul(row1, tmp1), minor2);
      tmp1 = SIMD.Float32x4.swizzle(tmp1, 2, 3, 0, 1); // 0x4E = 01001110
      minor1 = SIMD.Float32x4.add(SIMD.Float32x4.mul(row2, tmp1), minor1);
      minor2 = SIMD.Float32x4.sub(minor2, SIMD.Float32x4.mul(row1, tmp1));

      // ----
      tmp1 = SIMD.Float32x4.mul(row0, row2);
      tmp1 = SIMD.Float32x4.swizzle(tmp1, 1, 0, 3, 2); // 0xB1 = 10110001
      minor1 = SIMD.Float32x4.add(SIMD.Float32x4.mul(row3, tmp1), minor1);
      minor3 = SIMD.Float32x4.sub(minor3, SIMD.Float32x4.mul(row1, tmp1));
      tmp1 = SIMD.Float32x4.swizzle(tmp1, 2, 3, 0, 1); // 0x4E = 01001110
      minor1 = SIMD.Float32x4.sub(minor1, SIMD.Float32x4.mul(row3, tmp1));
      minor3 = SIMD.Float32x4.add(SIMD.Float32x4.mul(row1, tmp1), minor3);

      // Compute determinant
      det = SIMD.Float32x4.mul(row0, minor0);
      det = SIMD.Float32x4.add(SIMD.Float32x4.swizzle(det, 2, 3, 0, 1), det); // 0x4E = 01001110
      det = SIMD.Float32x4.add(SIMD.Float32x4.swizzle(det, 1, 0, 3, 2), det); // 0xB1 = 10110001
      tmp1 = SIMD.Float32x4.reciprocalApproximation(det);
      det = SIMD.Float32x4.sub(SIMD.Float32x4.add(tmp1, tmp1), SIMD.Float32x4.mul(det, SIMD.Float32x4.mul(tmp1, tmp1)));
      det = SIMD.Float32x4.swizzle(det, 0, 0, 0, 0);

      // These shuffles aren't necessary if the faulty transposition is done
      // up at the top of this function.
      //minor0 = SIMD.Float32x4.swizzle(minor0, 2, 1, 0, 3);
      //minor1 = SIMD.Float32x4.swizzle(minor1, 2, 1, 0, 3);
      //minor2 = SIMD.Float32x4.swizzle(minor2, 2, 1, 0, 3);
      //minor3 = SIMD.Float32x4.swizzle(minor3, 2, 1, 0, 3);

      // Compute final values by multiplying with 1/det
      minor0 = SIMD.Float32x4.mul(det, minor0);
      minor1 = SIMD.Float32x4.mul(det, minor1);
      minor2 = SIMD.Float32x4.mul(det, minor2);
      minor3 = SIMD.Float32x4.mul(det, minor3);

      SIMD.Float32x4.store(dst, 0,  minor0);
      SIMD.Float32x4.store(dst, 4,  minor1);
      SIMD.Float32x4.store(dst, 8,  minor2);
      SIMD.Float32x4.store(dst, 12, minor3);
    }
  }

  // Non SIMD version of the kernel
  function nonSimdMatrixInverseN(n) {
    for (var iterations = 0; iterations < n; ++iterations) {
      // Transpose the source matrix
      for (var i = 0; i < 4; i++) {
        tsrc[i] = src[i * 4];
        tsrc[i + 4] = src[i * 4 + 1];
        tsrc[i + 8] = src[i * 4 + 2];
        tsrc[i + 12] = src[i * 4 + 3];
      }

      // Calculate pairs for first 8 elements (cofactors)
      tmp[0] = tsrc[10] * tsrc[15];
      tmp[1] = tsrc[11] * tsrc[14];
      tmp[2] = tsrc[9] * tsrc[15];
      tmp[3] = tsrc[11] * tsrc[13];
      tmp[4] = tsrc[9] * tsrc[14];
      tmp[5] = tsrc[10] * tsrc[13];
      tmp[6] = tsrc[8] * tsrc[15];
      tmp[7] = tsrc[11] * tsrc[12];
      tmp[8] = tsrc[8] * tsrc[14];
      tmp[9] = tsrc[10] * tsrc[12];
      tmp[10] = tsrc[8] * tsrc[13];
      tmp[11] = tsrc[9] * tsrc[12];

      // calculate first 8 elements (cofactors)
      dst[0] = tmp[0] * tsrc[5] + tmp[3] * tsrc[6] + tmp[4] * tsrc[7];
      dst[0] -= tmp[1] * tsrc[5] + tmp[2] * tsrc[6] + tmp[5] * tsrc[7];
      dst[1] = tmp[1] * tsrc[4] + tmp[6] * tsrc[6] + tmp[9] * tsrc[7];
      dst[1] -= tmp[0] * tsrc[4] + tmp[7] * tsrc[6] + tmp[8] * tsrc[7];
      dst[2] = tmp[2] * tsrc[4] + tmp[7] * tsrc[5] + tmp[10] * tsrc[7];
      dst[2] -= tmp[3] * tsrc[4] + tmp[6] * tsrc[5] + tmp[11] * tsrc[7];
      dst[3] = tmp[5] * tsrc[4] + tmp[8] * tsrc[5] + tmp[11] * tsrc[6];
      dst[3] -= tmp[4] * tsrc[4] + tmp[9] * tsrc[5] + tmp[10] * tsrc[6];
      dst[4] = tmp[1] * tsrc[1] + tmp[2] * tsrc[2] + tmp[5] * tsrc[3];
      dst[4] -= tmp[0] * tsrc[1] + tmp[3] * tsrc[2] + tmp[4] * tsrc[3];
      dst[5] = tmp[0] * tsrc[0] + tmp[7] * tsrc[2] + tmp[8] * tsrc[3];
      dst[5] -= tmp[1] * tsrc[0] + tmp[6] * tsrc[2] + tmp[9] * tsrc[3];
      dst[6] = tmp[3] * tsrc[0] + tmp[6] * tsrc[1] + tmp[11] * tsrc[3];
      dst[6] -= tmp[2] * tsrc[0] + tmp[7] * tsrc[1] + tmp[10] * tsrc[3];
      dst[7] = tmp[4] * tsrc[0] + tmp[9] * tsrc[1] + tmp[10] * tsrc[2];
      dst[7] -= tmp[5] * tsrc[0] + tmp[8] * tsrc[1] + tmp[11] * tsrc[2];

      // calculate pairs for second 8 elements (cofactors)
      tmp[0] = tsrc[2] * tsrc[7];
      tmp[1] = tsrc[3] * tsrc[6];
      tmp[2] = tsrc[1] * tsrc[7];
      tmp[3] = tsrc[3] * tsrc[5];
      tmp[4] = tsrc[1] * tsrc[6];
      tmp[5] = tsrc[2] * tsrc[5];
      tmp[6] = tsrc[0] * tsrc[7];
      tmp[7] = tsrc[3] * tsrc[4];
      tmp[8] = tsrc[0] * tsrc[6];
      tmp[9] = tsrc[2] * tsrc[4];
      tmp[10] = tsrc[0] * tsrc[5];
      tmp[11] = tsrc[1] * tsrc[4];

      // calculate second 8 elements (cofactors)
      dst[8] = tmp[0] * tsrc[13] + tmp[3] * tsrc[14] + tmp[4] * tsrc[15];
      dst[8] -= tmp[1] * tsrc[13] + tmp[2] * tsrc[14] + tmp[5] * tsrc[15];
      dst[9] = tmp[1] * tsrc[12] + tmp[6] * tsrc[14] + tmp[9] * tsrc[15];
      dst[9] -= tmp[0] * tsrc[12] + tmp[7] * tsrc[14] + tmp[8] * tsrc[15];
      dst[10] = tmp[2] * tsrc[12] + tmp[7] * tsrc[13] + tmp[10] * tsrc[15];
      dst[10] -= tmp[3] * tsrc[12] + tmp[6] * tsrc[13] + tmp[11] * tsrc[15];
      dst[11] = tmp[5] * tsrc[12] + tmp[8] * tsrc[13] + tmp[11] * tsrc[14];
      dst[11] -= tmp[4] * tsrc[12] + tmp[9] * tsrc[13] + tmp[10] * tsrc[14];
      dst[12] = tmp[2] * tsrc[10] + tmp[5] * tsrc[11] + tmp[1] * tsrc[9];
      dst[12] -= tmp[4] * tsrc[11] + tmp[0] * tsrc[9] + tmp[3] * tsrc[10];
      dst[13] = tmp[8] * tsrc[11] + tmp[0] * tsrc[8] + tmp[7] * tsrc[10];
      dst[13] -= tmp[6] * tsrc[10] + tmp[9] * tsrc[11] + tmp[1] * tsrc[8];
      dst[14] = tmp[6] * tsrc[9] + tmp[11] * tsrc[11] + tmp[3] * tsrc[8];
      dst[14] -= tmp[10] * tsrc[11] + tmp[2] * tsrc[8] + tmp[7] * tsrc[9];
      dst[15] = tmp[10] * tsrc[10] + tmp[4] * tsrc[8] + tmp[9] * tsrc[9];
      dst[15] -= tmp[8] * tsrc[9] + tmp[11] * tsrc[10] + tmp[5] * tsrc[8];

      // calculate determinant
      var det = tsrc[0] * dst[0] + tsrc[1] * dst[1] + tsrc[2] * dst[2] + tsrc[3] * dst[3];

      // calculate matrix inverse
      det = 1 / det;
      for (var j = 0; j < 16; j++) {
        dst[j] *= det;
      }
    }
  }

} ());
