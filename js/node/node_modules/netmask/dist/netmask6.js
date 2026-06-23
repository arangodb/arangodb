"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.Netmask6Impl = void 0;
exports.ip6bigint = ip6bigint;
exports.bigint2ip6 = bigint2ip6;
const netmask4_1 = require("./netmask4");
const MAX_IPV6 = (1n << 128n) - 1n;
function ip6bigint(ip) {
    // Strip zone ID (e.g. %eth0)
    const zoneIdx = ip.indexOf('%');
    if (zoneIdx !== -1) {
        ip = ip.substring(0, zoneIdx);
    }
    // Handle mixed IPv4-mapped (e.g. ::ffff:192.168.1.1)
    const lastColon = ip.lastIndexOf(':');
    if (lastColon !== -1 && ip.indexOf('.', lastColon) !== -1) {
        const ipv4Part = ip.substring(lastColon + 1);
        const ipv4Long = (0, netmask4_1.ip2long)(ipv4Part);
        // IPv4 part replaces last 2 groups (32 bits), expand prefix to 6 groups
        const ipv6Prefix = ip.substring(0, lastColon + 1) + '0:0';
        const prefixVal = parseIPv6Pure(ipv6Prefix);
        return (prefixVal & ~0xffffffffn) | BigInt(ipv4Long);
    }
    return parseIPv6Pure(ip);
}
function parseIPv6Pure(ip) {
    const doubleColonIdx = ip.indexOf('::');
    let groups;
    if (doubleColonIdx !== -1) {
        const left = ip.substring(0, doubleColonIdx);
        const right = ip.substring(doubleColonIdx + 2);
        const leftGroups = left === '' ? [] : left.split(':');
        const rightGroups = right === '' ? [] : right.split(':');
        const missing = 8 - leftGroups.length - rightGroups.length;
        if (missing < 0) {
            throw new Error('Invalid IPv6: too many groups');
        }
        groups = [...leftGroups, ...Array(missing).fill('0'), ...rightGroups];
    }
    else {
        groups = ip.split(':');
    }
    if (groups.length !== 8) {
        throw new Error('Invalid IPv6: expected 8 groups, got ' + groups.length);
    }
    let result = 0n;
    for (let i = 0; i < 8; i++) {
        const g = groups[i];
        if (g.length === 0 || g.length > 4) {
            throw new Error('Invalid IPv6: bad group "' + g + '"');
        }
        const val = parseInt(g, 16);
        if (isNaN(val) || val < 0 || val > 0xffff) {
            throw new Error('Invalid IPv6: bad group "' + g + '"');
        }
        result = (result << 16n) | BigInt(val);
    }
    return result;
}
function bigint2ip6(n) {
    if (n < 0n || n > MAX_IPV6) {
        throw new Error('Invalid IPv6 address value');
    }
    const groups = [];
    for (let i = 0; i < 8; i++) {
        groups.unshift(Number(n & 0xffffn));
        n >>= 16n;
    }
    // RFC 5952: find longest run of consecutive zero groups
    let bestStart = -1;
    let bestLen = 0;
    let curStart = -1;
    let curLen = 0;
    for (let i = 0; i < 8; i++) {
        if (groups[i] === 0) {
            if (curStart === -1) {
                curStart = i;
                curLen = 1;
            }
            else {
                curLen++;
            }
        }
        else {
            if (curLen > bestLen && curLen >= 2) {
                bestStart = curStart;
                bestLen = curLen;
            }
            curStart = -1;
            curLen = 0;
        }
    }
    if (curLen > bestLen && curLen >= 2) {
        bestStart = curStart;
        bestLen = curLen;
    }
    if (bestStart !== -1 && bestStart + bestLen === 8 && bestStart > 0) {
        const before = groups.slice(0, bestStart).map(g => g.toString(16));
        return before.join(':') + '::';
    }
    else if (bestStart === 0) {
        const after = groups.slice(bestLen).map(g => g.toString(16));
        return '::' + after.join(':');
    }
    else if (bestStart > 0) {
        const before = groups.slice(0, bestStart).map(g => g.toString(16));
        const after = groups.slice(bestStart + bestLen).map(g => g.toString(16));
        return before.join(':') + '::' + after.join(':');
    }
    else {
        return groups.map(g => g.toString(16)).join(':');
    }
}
class Netmask6Impl {
    constructor(net, mask) {
        if (typeof net !== 'string') {
            throw new Error("Missing `net' parameter");
        }
        let prefixLen = mask;
        if (prefixLen === undefined || prefixLen === null) {
            const slashIdx = net.indexOf('/');
            if (slashIdx !== -1) {
                prefixLen = parseInt(net.substring(slashIdx + 1), 10);
                net = net.substring(0, slashIdx);
            }
            else {
                prefixLen = 128;
            }
        }
        if (isNaN(prefixLen) || prefixLen < 0 || prefixLen > 128) {
            throw new Error('Invalid mask for IPv6: ' + prefixLen);
        }
        this.bitmask = prefixLen;
        if (this.bitmask === 0) {
            this.maskBigint = 0n;
        }
        else {
            this.maskBigint = (MAX_IPV6 >> BigInt(128 - this.bitmask)) << BigInt(128 - this.bitmask);
        }
        try {
            this.netBigint = ip6bigint(net) & this.maskBigint;
        }
        catch (error) {
            throw new Error('Invalid IPv6 net address: ' + net);
        }
        this.size = Number(1n << BigInt(128 - this.bitmask));
        this.base = bigint2ip6(this.netBigint);
        this.mask = bigint2ip6(this.maskBigint);
        this.hostmask = bigint2ip6(~this.maskBigint & MAX_IPV6);
        this.first = this.base;
        this.last = bigint2ip6(this.netBigint + (1n << BigInt(128 - this.bitmask)) - 1n);
        this.broadcast = undefined;
    }
    contains(ip) {
        if (typeof ip === 'string') {
            if (ip.indexOf('/') > 0) {
                ip = new Netmask6Impl(ip);
            }
        }
        if (ip instanceof Netmask6Impl) {
            return this.contains(ip.base) && this.contains(ip.last);
        }
        else {
            const addr = ip6bigint(ip);
            return (addr & this.maskBigint) === this.netBigint;
        }
    }
    next(count = 1) {
        const sizeBig = 1n << BigInt(128 - this.bitmask);
        return new Netmask6Impl(bigint2ip6(this.netBigint + sizeBig * BigInt(count)), this.bitmask);
    }
    forEach(fn) {
        let addr = this.netBigint;
        const sizeBig = 1n << BigInt(128 - this.bitmask);
        const lastAddr = this.netBigint + sizeBig - 1n;
        let index = 0;
        while (addr <= lastAddr) {
            fn(bigint2ip6(addr), Number(addr), index);
            index++;
            addr++;
        }
    }
    toString() {
        return this.base + '/' + this.bitmask;
    }
}
exports.Netmask6Impl = Netmask6Impl;
