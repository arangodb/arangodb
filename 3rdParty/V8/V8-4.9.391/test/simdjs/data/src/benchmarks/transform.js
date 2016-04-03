// Transform vertex by 4x4 transformation matrix.
// Author: John McCutchan

(function () {

  // Kernel configuration
  var kernelConfig = {
    kernelName:       "VertexTransform",
    kernelInit:       init,
    kernelCleanup:    cleanup,
    kernelSimd:       simdVertexTransform,
    kernelNonSimd:    vertexTransform,
    kernelIterations: 1000
  };

  // Hook up to the harness
  benchmarks.add(new Benchmark(kernelConfig));

  // Benchmark data, initialization and kernel functions
  var T = new Float32Array(16);
  var V = new Float32Array(4);
  var Out = new Float32Array(4);
  var Tx = new Float32Array(16);
  var Vx = new Float32Array(4);
  var Outx = new Float32Array(4);

  function init() {
    T[0] = 1.0;
    T[5] = 1.0;
    T[10] = 1.0;
    T[15] = 1.0;
    V[0] = 1.0;
    V[1] = 2.0;
    V[2] = 3.0;
    V[3] = 1.0;
    SIMD.Float32x4.store(Tx, 0,  SIMD.Float32x4(1.0, 0.0, 0.0, 0.0));
    SIMD.Float32x4.store(Tx, 4,  SIMD.Float32x4(0.0, 1.0, 0.0, 0.0));
    SIMD.Float32x4.store(Tx, 8,  SIMD.Float32x4(0.0, 0.0, 1.0, 0.0));
    SIMD.Float32x4.store(Tx, 12, SIMD.Float32x4(0.0, 0.0, 0.0, 1.0));
    SIMD.Float32x4.store(Vx, 0, SIMD.Float32x4(1.0, 2.0, 3.0, 1.0));
    simdVertexTransform(1);
    vertexTransform(1);
    return (SIMD.Float32x4.extractLane(SIMD.Float32x4.load(Outx, 0), 0) == Out[0]) &&
        (SIMD.Float32x4.extractLane(SIMD.Float32x4.load(Outx, 0), 1) == Out[1]) &&
        (SIMD.Float32x4.extractLane(SIMD.Float32x4.load(Outx, 0), 2) == Out[2]) &&
        (SIMD.Float32x4.extractLane(SIMD.Float32x4.load(Outx, 0), 3) == Out[3]);
  }

  function cleanup() {
    return init(); // Sanity checking before and after are the same
  }

  function vertexTransform(n) {
    for (var i = 0; i < n; i++) {
      var x = V[0];
      var y = V[1];
      var z = V[2];
      var w = V[3];
      var m0 = T[0];
      var m4 = T[4];
      var m8 = T[8];
      var m12 = T[12];
      Out[0] = (m0 * x + m4 * y + m8 * z + m12 * w);
      var m1 = T[1];
      var m5 = T[5];
      var m9 = T[9];
      var m13 = T[13];
      Out[1] = (m1 * x + m5 * y + m9 * z + m13 * w);
      var m2 = T[2];
      var m6 = T[6];
      var m10 = T[10];
      var m14 = T[14];
      Out[2] = (m2 * x + m6 * y + m10 * z + m14 * w);
      var m3 = T[3];
      var m7 = T[7];
      var m11 = T[11];
      var m15 = T[15];
      Out[3] = (m3 * x + m7 * y + m11 * z + m15 * w);
    }
  }

  function simdVertexTransform(n) {
    for (var i = 0; i < n; i++) {
      var xxxx = SIMD.Float32x4.swizzle(SIMD.Float32x4.load(Vx, 0), 0, 0, 0, 0);
      var z = SIMD.Float32x4.splat(0.0);
      z = SIMD.Float32x4.add(z, SIMD.Float32x4.mul(xxxx, SIMD.Float32x4.load(Tx, 0)));
      var yyyy = SIMD.Float32x4.swizzle(SIMD.Float32x4.load(Vx, 0), 1, 1, 1, 1);
      z = SIMD.Float32x4.add(z, SIMD.Float32x4.mul(yyyy, SIMD.Float32x4.load(Tx, 4)));
      var zzzz = SIMD.Float32x4.swizzle(SIMD.Float32x4.load(Vx, 0), 2, 2, 2, 2);
      z = SIMD.Float32x4.add(z, SIMD.Float32x4.mul(zzzz, SIMD.Float32x4.load(Tx, 8)));
      var wwww = SIMD.Float32x4.swizzle(SIMD.Float32x4.load(Vx, 0), 3, 3, 3, 3);
      z = SIMD.Float32x4.add(z, SIMD.Float32x4.mul(wwww, SIMD.Float32x4.load(Tx, 12)));
      SIMD.Float32x4.store(Outx, 0, z);
    }
  }

} ());
