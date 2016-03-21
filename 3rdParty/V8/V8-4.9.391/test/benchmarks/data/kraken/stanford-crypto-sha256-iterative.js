/**
 * Test SHA-256 using an ad-hoc iterative technique.
 * This uses a string buffer which has n characters on the nth iteration.
 * Each iteration, the buffer is hashed and the hash is converted to a string.
 * The first two characters of the string are prepended to the buffer, then the
 * last character of the buffer is removed.  This way, neither the beginning nor
 * the end of the buffer is fixed.
 *
 * The hashes from each output step are also hashed together into one final hash.
 * This is compared against a final hash which was computed with SSL.
 */

new sjcl.test.TestCase("SHA-256 iterative", function (cb) {
  if (!sjcl.hash.sha256) {
    this.unimplemented();
    cb && cb();
    return;
  }
  
  var toBeHashed = "", cumulative = new sjcl.hash.sha256(), hash, thiz=this;
  browserUtil.cpsIterate(function (i, cbb) {
    for (var n=100*i; n<100*(i+1); n++) {
      hash = sjcl.hash.sha256.hash(toBeHashed);
      hash = sjcl.codec.hex.fromBits(hash);
      cumulative.update(hash);
      toBeHashed = (hash.substring(0,2)+toBeHashed).substring(0,n+1);
    }
    cbb && cbb();
  }, 0, 10, true, function () {
    hash = sjcl.codec.hex.fromBits(cumulative.finalize());
    thiz.require(hash === "f305c76d5d457ddf04f1927166f5e13429407049a5c5f29021916321fcdcd8b4");
    cb && cb();
  });
});

sjcl.test.run();
