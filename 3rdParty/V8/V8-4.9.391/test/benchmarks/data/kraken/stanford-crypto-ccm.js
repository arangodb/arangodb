new sjcl.test.TestCase("CCM mode tests", function (cb) {
  if (!sjcl.cipher.aes || !sjcl.mode.ccm) {
    this.unimplemented();
    cb && cb();
    return;
  }

  var i, kat = sjcl.test.vector.ccm, tv, iv, ct, aes, len, tlen, thiz=this, w=sjcl.bitArray, pt, h=sjcl.codec.hex, ad;
  browserUtil.cpsIterate(function (j, cbb) {
    for (i=100*j; i<kat.length && i<100*(j+1); i++) {
      tv = kat[i];
      len = 32 * tv.key.length;
      aes = new sjcl.cipher.aes(h.toBits(tv.key));

      // Convert from strings
      iv = h.toBits(tv.iv);
      ad = h.toBits(tv.adata);
      pt = h.toBits(tv.pt);
      ct = h.toBits(tv.ct + tv.tag);
      tlen = tv.tag.length * 4;

      thiz.require(w.equal(sjcl.mode.ccm.encrypt(aes, pt, iv, ad, tlen), ct), "aes-"+len+"-ccm-encrypt #"+i);
      try {
        thiz.require(w.equal(sjcl.mode.ccm.decrypt(aes, ct, iv, ad, tlen), pt), "aes-"+len+"-ccm-decrypt #"+i);
      } catch (e) {
        thiz.fail("aes-ccm-decrypt #"+i+" (exn "+e+")");
      }
    }
    cbb();
  }, 0, kat.length / 100, true, cb);
});

sjcl.test.run();
