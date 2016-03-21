new sjcl.test.TestCase("AES official known-answer tests", function (cb) {
  if (!sjcl.cipher.aes) {
    this.unimplemented();
    cb && cb();
    return;
  }
  
  var i, kat = sjcl.test.vector.aes, tv, len, aes;

  //XXX add more vectors instead of looping
  for (var index = 0; index < 8; index++) {
      for (i=0; i<kat.length; i++) {
        tv = kat[i];
        len = 32 * tv.key.length;
        aes = new sjcl.cipher.aes(tv.key);
        this.require(sjcl.bitArray.equal(aes.encrypt(tv.pt), tv.ct), "encrypt "+len+" #"+i);
        this.require(sjcl.bitArray.equal(aes.decrypt(tv.ct), tv.pt), "decrypt "+len+" #"+i);
      }
  }
  cb && cb();
});

sjcl.test.run();
