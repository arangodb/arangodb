// Simple performance test memcpy using SIMD.
// Author: Moh Haghighat 
// January 20, 2015

(function () {

  // Kernel configuration
  var kernelConfig = {
    kernelName:       "Memcpy",
    kernelInit:       initArray,
    kernelCleanup:    cleanup,
    kernelSimd:       simdMemcpy,
    kernelNonSimd:    memcpy,
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

  var LEN  = TOTAL_MEMORY/32;
  var ptr1 = 0;
  var ptr2 = ptr1 + 2 * LEN; 
  var ptr3 = ptr2 + 2 * LEN; 
  var VAL  = 200;

  function sanityCheck() {
    for (var j = 0; j < LEN; ++j) {
      if (HEAP8[ptr2+j] != HEAP8[ptr3+j]) {
        return false; 
      } 
    }
    return true; 
  }

  function initArray() {
    for (var j = 0; j < LEN; ++j) {
      HEAP8[ptr1+j] = (VAL+1*j)|0;
      HEAP8[ptr2+j] = (VAL+2*j)|0;
      HEAP8[ptr3+j] = (VAL+3*j)|0;
    }
    return true;
  }

  function cleanup() {
    return sanityCheck();
  }

  function _emscripten_memcpy_big(dest, src, num) {
    dest = dest; src = src; num = num; 
    HEAPU8.set(HEAPU8.subarray(src, src+num), dest);
    return dest;
  }

  function NonSimdAsmjsModule (global, imp, buffer) {
    "use asm"

    var HEAP8 = new global.Int8Array(buffer);
    var HEAP32 = new global.Int32Array(buffer);
    var _emscripten_memcpy_big = imp._emscripten_memcpy_big;

    function _memcpy(dest, src, num) {
      dest = dest|0; src = src|0; num = num|0;
      var ret = 0;
      if ((num|0) >= 4096) return _emscripten_memcpy_big(dest|0, src|0, num|0)|0;
      ret = dest|0;
      if ((dest&3) == (src&3)) {
        while (dest & 3) {
          if ((num|0) == 0) return ret|0;
          HEAP8[((dest)>>0)]=((HEAP8[((src)>>0)])|0);
          dest = (dest+1)|0;
          src = (src+1)|0;
          num = (num-1)|0;
        }
        while ((num|0) >= 4) {
          HEAP32[((dest)>>2)]=((HEAP32[((src)>>2)])|0);
          dest = (dest+4)|0;
          src = (src+4)|0;
          num = (num-4)|0;
        }
      }
      while ((num|0) > 0) {
        HEAP8[((dest)>>0)]=((HEAP8[((src)>>0)])|0);
        dest = (dest+1)|0;
        src = (src+1)|0;
        num = (num-1)|0;
      }
      return ret|0;
    }

    return _memcpy;
  }

  function SimdAsmjsModule (global, imp, buffer) {
    "use asm"

    var HEAP8 = new global.Int8Array(buffer);
    var HEAP32 = new global.Int32Array(buffer);
    var HEAPU8 = new global.Uint8Array(buffer);
    var _emscripten_memcpy_big = imp._emscripten_memcpy_big;
    var i4 = global.SIMD.Int32x4;
    var i4load  = i4.load;
    var i4store = i4.store;

    function _memcpy(dest, src, num) {
      dest = dest|0; src = src|0; num = num|0;
      var ret = 0;
      if ((num|0) >= 4096) return _emscripten_memcpy_big(dest|0, src|0, num|0)|0;
      ret = dest|0;

      if ((num|0) >= 16) { 
        while (dest & 15) {
          if ((num|0) == 0) return ret|0;
          HEAP8[((dest)>>0)]=((HEAP8[((src)>>0)])|0);
          dest = (dest+1)|0;
          src = (src+1)|0;
          num = (num-1)|0;
        }
        while ((num|0) >= 16) {
          i4store(HEAPU8, ((dest)>>0), i4load(HEAPU8, ((src)>>0))); 
          dest = (dest+16)|0;
          src = (src+16)|0;
          num = (num-16)|0;
        }
        if ((num|0) == 0) return ret|0;
      } 

      if ((dest&3) == (src&3)) {
        while (dest & 3) {
          if ((num|0) == 0) return ret|0;
          HEAP8[((dest)>>0)]=((HEAP8[((src)>>0)])|0);
          dest = (dest+1)|0;
          src = (src+1)|0;
          num = (num-1)|0;
        }
        while ((num|0) >= 4) {
          HEAP32[((dest)>>2)]=((HEAP32[((src)>>2)])|0);
          dest = (dest+4)|0;
          src = (src+4)|0;
          num = (num-4)|0;
        }
      } 

      while ((num|0) > 0) {
        HEAP8[((dest)>>0)]=((HEAP8[((src)>>0)])|0);
        dest = (dest+1)|0;
        src = (src+1)|0;
        num = (num-1)|0;
      }

      return ret|0;
    }

    return _memcpy;
  }

  function memcpy(n) {
    var func = NonSimdAsmjsModule(this, {"_emscripten_memcpy_big": _emscripten_memcpy_big}, buffer);
    for (var i = 0; i < n; ++i) {
      // try memcpy of variable lengths, from 0 to LEN
      for (var j = 0; j < LEN; ++j) {
        // try different (alignment mod 16) from 0 to 15
        for (var k = 0; k < 16; k++){
          func (ptr2+k, ptr1, j);
        }
      }
    }
    return true;
  }

  function simdMemcpy(n) {
    var func = SimdAsmjsModule(this, {"_emscripten_memcpy_big": _emscripten_memcpy_big}, buffer);
    for (var i = 0; i < n; ++i) {
      // try memcpy of variable lengths, from 0 to LEN
      for (var j = 0; j < LEN; ++j) {
        // try different (alignment mod 16) from 0 to 15
        for (var k = 0; k < 16; k++){
          func (ptr3+k, ptr1, j);
        }
      }
    }
    return true;
  }

} ());
