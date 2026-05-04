"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.long2ip = exports.ip2long = exports.Netmask = void 0;
const netmask4_1 = require("./netmask4");
Object.defineProperty(exports, "ip2long", { enumerable: true, get: function () { return netmask4_1.ip2long; } });
Object.defineProperty(exports, "long2ip", { enumerable: true, get: function () { return netmask4_1.long2ip; } });
const netmask6_1 = require("./netmask6");
class Netmask {
    constructor(net, mask) {
        if (typeof net !== 'string') {
            throw new Error("Missing `net' parameter");
        }
        // Detect IPv6: check the address part (before any /) for ':'
        const addrPart = net.indexOf('/') !== -1 ? net.substring(0, net.indexOf('/')) : net;
        if (addrPart.indexOf(':') !== -1) {
            this._impl = new netmask6_1.Netmask6Impl(net, mask);
        }
        else {
            this._impl = new netmask4_1.Netmask4Impl(net, mask);
        }
        this.base = this._impl.base;
        this.mask = this._impl.mask;
        this.hostmask = this._impl.hostmask;
        this.bitmask = this._impl.bitmask;
        this.size = this._impl.size;
        this.first = this._impl.first;
        this.last = this._impl.last;
        this.broadcast = this._impl.broadcast;
        if (this._impl instanceof netmask4_1.Netmask4Impl) {
            this.maskLong = this._impl.maskLong;
            this.netLong = this._impl.netLong;
        }
        else {
            this.maskLong = 0;
            this.netLong = 0;
        }
    }
    contains(ip) {
        if (typeof ip === 'string') {
            // If it has a '/', it's a CIDR block — wrap it
            if (ip.indexOf('/') > 0) {
                ip = new Netmask(ip);
            }
            // IPv4 shorthand (fewer than 4 octets, no colons) — wrap it
            else if (ip.indexOf(':') === -1 && ip.split('.').length !== 4) {
                ip = new Netmask(ip);
            }
        }
        if (ip instanceof Netmask) {
            return this.contains(ip.base) && this.contains(ip.broadcast || ip.last);
        }
        // Plain IP string — delegate to impl
        return this._impl.contains(ip);
    }
    next(count = 1) {
        const nextImpl = this._impl.next(count);
        const result = new Netmask(nextImpl.base, nextImpl.bitmask);
        return result;
    }
    /** @deprecated */
    forEach(fn) {
        this._impl.forEach(fn);
    }
    toString() {
        return this._impl.toString();
    }
}
exports.Netmask = Netmask;
