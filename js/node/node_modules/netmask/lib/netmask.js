(function() {
  var Netmask, ip2long, long2ip;

  long2ip = function(long) {
    var a, b, c, d;
    a = (long & (0xff << 24)) >>> 24;
    b = (long & (0xff << 16)) >>> 16;
    c = (long & (0xff << 8)) >>> 8;
    d = long & 0xff;
    return [a, b, c, d].join('.');
  };

  ip2long = function(ip) {
    var b, byte, i, _len;
    b = (ip + '').split('.');
    if (b.length === 0 || b.length > 4) throw new Error('Invalid IP');
    for (i = 0, _len = b.length; i < _len; i++) {
      byte = b[i];
      if (isNaN(parseInt(byte, 10))) throw new Error("Invalid byte: " + byte);
      if (byte < 0 || byte > 255) throw new Error("Invalid byte: " + byte);
    }
    return ((b[0] || 0) << 24 | (b[1] || 0) << 16 | (b[2] || 0) << 8 | (b[3] || 0)) >>> 0;
  };

  Netmask = (function() {

    function Netmask(net, mask) {
      var i, _ref;
      if (typeof net !== 'string') throw new Error("Missing `net' parameter");
      if (!mask) {
        _ref = net.split('/', 2), net = _ref[0], mask = _ref[1];
        if (!mask) {
          switch (net.split('.').length) {
            case 1:
              mask = 8;
              break;
            case 2:
              mask = 16;
              break;
            case 3:
              mask = 24;
              break;
            case 4:
              mask = 32;
          }
        }
      }
      if (typeof mask === 'string' && mask.indexOf('.') > -1) {
        try {
          this.maskLong = ip2long(mask);
        } catch (error) {
          throw new Error("Invalid mask: " + mask);
        }
        for (i = 32; i >= 0; i--) {
          if (this.maskLong === (0xffffffff << (32 - i)) >>> 0) {
            this.bitmask = i;
            break;
          }
        }
      } else if (mask) {
        this.bitmask = parseInt(mask, 10);
        this.maskLong = (0xffffffff << (32 - this.bitmask)) >>> 0;
      } else {
        throw new Error("Invalid mask: empty");
      }
      try {
        this.netLong = (ip2long(net) & this.maskLong) >>> 0;
      } catch (error) {
        throw new Error("Invalid net address: " + net);
      }
      if (!(this.bitmask <= 32)) throw new Error("Invalid mask for ip4: " + mask);
      this.size = Math.pow(2, 32 - this.bitmask);
      this.base = long2ip(this.netLong);
      this.mask = long2ip(this.maskLong);
      this.hostmask = long2ip(~this.maskLong);
      this.first = this.bitmask <= 30 ? long2ip(this.netLong + 1) : this.base;
      this.last = this.bitmask <= 30 ? long2ip(this.netLong + this.size - 2) : long2ip(this.netLong + this.size - 1);
      this.broadcast = this.bitmask <= 30 ? long2ip(this.netLong + this.size - 1) : void 0;
    }

    Netmask.prototype.contains = function(ip) {
      if (typeof ip === 'string' && (ip.indexOf('/') > 0 || ip.split('.').length !== 4)) {
        ip = new Netmask(ip);
      }
      if (ip instanceof Netmask) {
        return this.contains(ip.base) && this.contains(ip.broadcast);
      } else {
        return (ip2long(ip) & this.maskLong) >>> 0 === (this.netLong & this.maskLong) >>> 0;
      }
    };

    Netmask.prototype.next = function(count) {
      if (count == null) count = 1;
      return new Netmask(long2ip(this.netLong + (this.size * count)), this.mask);
    };

    Netmask.prototype.forEach = function(fn) {
      var index, long, range, _i, _len, _ref, _ref2, _results, _results2;
      range = (function() {
        _results = [];
        for (var _i = _ref = ip2long(this.first), _ref2 = ip2long(this.last); _ref <= _ref2 ? _i <= _ref2 : _i >= _ref2; _ref <= _ref2 ? _i++ : _i--){ _results.push(_i); }
        return _results;
      }).apply(this);
      _results2 = [];
      for (index = 0, _len = range.length; index < _len; index++) {
        long = range[index];
        _results2.push(fn(long2ip(long), long, index));
      }
      return _results2;
    };

    Netmask.prototype.toString = function() {
      return this.base + "/" + this.bitmask;
    };

    return Netmask;

  })();

  exports.ip2long = ip2long;

  exports.long2ip = long2ip;

  exports.Netmask = Netmask;

}).call(this);
