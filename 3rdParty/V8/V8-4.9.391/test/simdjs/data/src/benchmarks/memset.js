// Simple performance test memset using SIMD.
// Author: Moh Haghighat 
// December 10, 2014

(function () {

  // Kernel configuration
  var kernelConfig = {
    kernelName:       "Memset",
    kernelInit:       initArray,
    kernelCleanup:    cleanup,
    kernelSimd:       simdMemset,
    kernelNonSimd:    memset,
    kernelIterations: 1000
  };

  // Hook up to the harness
  benchmarks.add(new Benchmark(kernelConfig));

  // Benchmark data, initialization and kernel functions
  var TOTAL_MEMORY = 4096*32;
  var buffer = new ArrayBuffer(TOTAL_MEMORY);
  var HEAP8 = new Int8Array(buffer);
  var HEAP32 = new Int32Array(buffer);
  var HEAPU8 = new Uint8Array(buffer);

  var LEN  = TOTAL_MEMORY/16;
  var ptr1 = 0;
  var ptr2 = ptr1 + LEN; 
  var VAL  = 200;

  function sanityCheck() {
    for (var j = 0; j < LEN; ++j) {
      if (HEAP8[ptr1+j] != HEAP8[ptr2+j]) {
        return false; 
      } 
    }
    return true; 
  }

  function initArray() {
    return true;
  }

  function cleanup() {
    return sanityCheck();
  }

  function NonSimdAsmjsModule (global, imp, buffer) {
    "use asm"

    var HEAP8 = new global.Int8Array(buffer);
    var HEAP32 = new global.Int32Array(buffer);

    function _memset(ptr, value, num) {
      ptr = ptr|0;
      value = value|0;
      num = num|0;
      var stop = 0, value4 = 0, stop4 = 0, unaligned = 0;
      stop = (ptr + num)|0;
      if ((num|0) >= 20) {
        // This is unaligned, but quite large, so work hard to get to aligned settings
        value = value & 0xff;
        unaligned = ptr & 3;
        value4 = value | (value << 8) | (value << 16) | (value << 24);
        stop4 = stop & ~3;
        if (unaligned) {
          unaligned = (ptr + 4 - unaligned)|0;
          while ((ptr|0) < (unaligned|0)) { // no need to check for stop, since we have large num
            HEAP8[((ptr)>>0)]=value;
            ptr = (ptr+1)|0;
          }
        }
        while ((ptr|0) < (stop4|0)) {
          HEAP32[((ptr)>>2)]=value4;
          ptr = (ptr+4)|0;
        }
      }
      while ((ptr|0) < (stop|0)) {
        HEAP8[((ptr)>>0)]=value;
        ptr = (ptr+1)|0;
      }
      return (ptr-num)|0;
    }

    return _memset;
  }

  function SimdAsmjsModule (global, imp, buffer) {
    "use asm"

    var HEAP8 = new global.Int8Array(buffer);
    var HEAP32 = new global.Int32Array(buffer);
    var HEAPU8 = new global.Uint8Array(buffer);
    var i4 = global.SIMD.Int32x4;
    var i4splat = i4.splat;
    var i4store = i4.store;

    function _simdMemset(ptr, value, num) {
      ptr = ptr|0;
      value = value|0;
      num = num|0;

      var value2 = 0, value4 = 0, value16 = i4(0, 0, 0, 0), stop = 0, stop4 = 0, stop16 = 0, unaligned = 0;

      stop = (ptr + num)|0;
      if ((num|0) >= 16) {
        // This is unaligned, but quite large, so work hard to get to aligned settings
        value = value & 0xff;

        unaligned = ptr & 0xf;
        if (unaligned) {
          // Initialize the 16-byte unaligned leading part 
          unaligned = (ptr + 16 - unaligned)|0;
          while ((ptr|0) < (unaligned|0)) { // no need to check for stop, since we have large num
            HEAP8[((ptr)>>0)]=value;
            ptr = (ptr+1)|0;
          }
        }

        value2 = (value  | (value  << 8))|0;  
        value4 = (value2 | (value2 << 16))|0;
        value16 =i4splat(value4);
        stop16 = stop & ~15;


        while ((ptr|0) < (stop16|0)) {
          i4store(HEAPU8, ((ptr)>>0), value16);
          ptr = (ptr+16)|0;
        }

        stop4 = stop & ~3;
        while ((ptr|0) < (stop4|0)) {
          HEAP32[((ptr)>>2)]=value4;
          ptr = (ptr+4)|0;
        }
      }
      while ((ptr|0) < (stop|0)) {
        HEAP8[((ptr)>>0)]=value;
        ptr = (ptr+1)|0;
      }
      return (ptr-num)|0;
    }

    return _simdMemset;
  }

  function memset(n) {
    var func = NonSimdAsmjsModule(this, {}, buffer);
    for (var i = 0; i < n; ++i) {
      func (ptr1, VAL, LEN);
    }
    return true;
  }

  function simdMemset(n) {
    var func = SimdAsmjsModule(this, {}, buffer);
    for (var i = 0; i < n; ++i) {
      func (ptr2, VAL, LEN);
    }
    return true;
  }

} ());
