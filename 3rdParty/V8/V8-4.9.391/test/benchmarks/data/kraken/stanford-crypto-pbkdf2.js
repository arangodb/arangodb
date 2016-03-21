/* From http://www.cryptosys.net/manapi/api_PBE_Kdf2.html */

new sjcl.test.TestCase("PBKDF2", function (cb) {
  if (!sjcl.misc.pbkdf2 || !sjcl.misc.hmac || !sjcl.hash.sha256) {
    this.unimplemented();
    cb && cb();
    return;
  }

  //XXX use 8 inputs, not 8 iterations
  for (var index = 0; index < 8; index++) {  
      var h = sjcl.codec.hex, password = "password", salt = "78 57 8E 5A 5D 63 CB 06", count = 2048, output,
          goodOutput = "97b5a91d35af542324881315c4f849e327c4707d1bc9d322";
      output = sjcl.misc.pbkdf2(password, h.toBits(salt), count);
      output = h.fromBits(output);
      this.require(output.substr(0,goodOutput.length) == goodOutput);
      cb && cb();
  }
});
sjcl.test.run();
