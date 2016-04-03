// To specifically test the p(r)olyfill.

if (typeof SIMD != 'undefined')
  SIMD = void 0;

load('./shell_test_runner.js');
