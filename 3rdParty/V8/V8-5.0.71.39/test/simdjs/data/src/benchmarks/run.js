"use strict"

load ('../ecmascript_simd.js');
load ('base.js');

// load individual benchmarks

load ('kernel-template.js');
load ('averageFloat32x4.js');
load ('averageFloat32x4LoadFromInt8Array.js');
load ('averageFloat32x4LoadX.js');
load ('averageFloat32x4LoadXY.js');
load ('averageFloat32x4LoadXYZ.js');
load ('averageFloat64x2.js');
load ('averageFloat64x2Load.js');
load ('mandelbrot.js');
load ('matrix-multiplication.js');
load ('transform.js');
load ('shiftrows.js');
load ('aobench.js');
load ('transform.js');
load ('transpose4x4.js');
load ('inverse4x4.js');
load ('sinx4.js');
load ('memset.js');
load ('memcpy.js');

function printResult (str) {
  print (str);
}

function printError (str) {
  print (str);
}

function printScore (str) {
  print (str);
}

benchmarks.runAll ({notifyResult: printResult,
                    notifyError:  printError,
                    notifyScore:  printScore},
                   true);
