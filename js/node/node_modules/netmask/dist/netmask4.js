"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.Netmask4Impl = void 0;
exports.ip2long = ip2long;
exports.long2ip = long2ip;
function long2ip(long) {
    const a = (long & (0xff << 24)) >>> 24;
    const b = (long & (0xff << 16)) >>> 16;
    const c = (long & (0xff << 8)) >>> 8;
    const d = long & 0xff;
    return [a, b, c, d].join('.');
}
const chr0 = '0'.charCodeAt(0);
const chra = 'a'.charCodeAt(0);
const chrA = 'A'.charCodeAt(0);
function parseNum(s) {
    let n = 0;
    let base = 10;
    let dmax = '9';
    let i = 0;
    if (s.length > 1 && s[i] === '0') {
        if (s[i + 1] === 'x' || s[i + 1] === 'X') {
            i += 2;
            base = 16;
        }
        else if ('0' <= s[i + 1] && s[i + 1] <= '9') {
            i++;
            base = 8;
            dmax = '7';
        }
    }
    const start = i;
    while (i < s.length) {
        if ('0' <= s[i] && s[i] <= dmax) {
            n = (n * base + (s.charCodeAt(i) - chr0)) >>> 0;
        }
        else if (base === 16) {
            if ('a' <= s[i] && s[i] <= 'f') {
                n = (n * base + (10 + s.charCodeAt(i) - chra)) >>> 0;
            }
            else if ('A' <= s[i] && s[i] <= 'F') {
                n = (n * base + (10 + s.charCodeAt(i) - chrA)) >>> 0;
            }
            else {
                break;
            }
        }
        else {
            break;
        }
        if (n > 0xFFFFFFFF) {
            throw new Error('too large');
        }
        i++;
    }
    if (i === start) {
        throw new Error('empty octet');
    }
    return [n, i];
}
function ip2long(ip) {
    const b = [];
    for (let i = 0; i <= 3; i++) {
        if (ip.length === 0) {
            break;
        }
        if (i > 0) {
            if (ip[0] !== '.') {
                throw new Error('Invalid IP');
            }
            ip = ip.substring(1);
        }
        const [n, c] = parseNum(ip);
        ip = ip.substring(c);
        b.push(n);
    }
    if (ip.length !== 0) {
        throw new Error('Invalid IP');
    }
    switch (b.length) {
        case 1:
            if (b[0] > 0xFFFFFFFF) {
                throw new Error('Invalid IP');
            }
            return b[0] >>> 0;
        case 2:
            if (b[0] > 0xFF || b[1] > 0xFFFFFF) {
                throw new Error('Invalid IP');
            }
            return (b[0] << 24 | b[1]) >>> 0;
        case 3:
            if (b[0] > 0xFF || b[1] > 0xFF || b[2] > 0xFFFF) {
                throw new Error('Invalid IP');
            }
            return (b[0] << 24 | b[1] << 16 | b[2]) >>> 0;
        case 4:
            if (b[0] > 0xFF || b[1] > 0xFF || b[2] > 0xFF || b[3] > 0xFF) {
                throw new Error('Invalid IP');
            }
            return (b[0] << 24 | b[1] << 16 | b[2] << 8 | b[3]) >>> 0;
        default:
            throw new Error('Invalid IP');
    }
}
class Netmask4Impl {
    constructor(net, mask) {
        if (typeof net !== 'string') {
            throw new Error("Missing `net' parameter");
        }
        let maskStr = mask;
        if (!maskStr) {
            const parts = net.split('/', 2);
            net = parts[0];
            maskStr = parts[1];
        }
        if (!maskStr) {
            maskStr = 32;
        }
        if (typeof maskStr === 'string' && maskStr.indexOf('.') > -1) {
            try {
                this.maskLong = ip2long(maskStr);
            }
            catch (error) {
                throw new Error("Invalid mask: " + maskStr);
            }
            this.bitmask = NaN;
            for (let i = 32; i >= 0; i--) {
                if (this.maskLong === (0xffffffff << (32 - i)) >>> 0) {
                    this.bitmask = i;
                    break;
                }
            }
        }
        else if (maskStr || maskStr === 0) {
            this.bitmask = parseInt(maskStr, 10);
            this.maskLong = 0;
            if (this.bitmask > 0) {
                this.maskLong = (0xffffffff << (32 - this.bitmask)) >>> 0;
            }
        }
        else {
            throw new Error("Invalid mask: empty");
        }
        try {
            this.netLong = (ip2long(net) & this.maskLong) >>> 0;
        }
        catch (error) {
            throw new Error("Invalid net address: " + net);
        }
        if (!(this.bitmask <= 32)) {
            throw new Error("Invalid mask for ip4: " + maskStr);
        }
        this.size = Math.pow(2, 32 - this.bitmask);
        this.base = long2ip(this.netLong);
        this.mask = long2ip(this.maskLong);
        this.hostmask = long2ip(~this.maskLong);
        this.first = this.bitmask <= 30 ? long2ip(this.netLong + 1) : this.base;
        this.last = this.bitmask <= 30 ? long2ip(this.netLong + this.size - 2) : long2ip(this.netLong + this.size - 1);
        this.broadcast = this.bitmask <= 30 ? long2ip(this.netLong + this.size - 1) : undefined;
    }
    contains(ip) {
        if (typeof ip === 'string' && (ip.indexOf('/') > 0 || ip.split('.').length !== 4)) {
            ip = new Netmask4Impl(ip);
        }
        if (ip instanceof Netmask4Impl) {
            return this.contains(ip.base) && this.contains((ip.broadcast || ip.last));
        }
        else {
            return (ip2long(ip) & this.maskLong) >>> 0 === (this.netLong & this.maskLong) >>> 0;
        }
    }
    next(count = 1) {
        return new Netmask4Impl(long2ip(this.netLong + (this.size * count)), this.mask);
    }
    forEach(fn) {
        let long = ip2long(this.first);
        const lastLong = ip2long(this.last);
        let index = 0;
        while (long <= lastLong) {
            fn(long2ip(long), long, index);
            index++;
            long++;
        }
    }
    toString() {
        return this.base + "/" + this.bitmask;
    }
}
exports.Netmask4Impl = Netmask4Impl;
