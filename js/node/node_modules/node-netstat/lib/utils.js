"use strict";

// only care about complete lines, return remainder and let callee handle it
var forEachLine = function (text, handler) {
    var n = text.indexOf('\n');
    while (n > -1) {
        handler(text.substring(0, n));
        text = text.substring(n + 1);
        n = text.indexOf('\n');
    }

    return text;
};

var parseAddress = exports.parseAddress = function (raw) {
    var port = null,
        address = null;
    if (raw[0] == '[') {
        port = raw.substring(raw.lastIndexOf(':') + 1);
        address = raw.substring(1, raw.indexOf(']'));
    } else if (raw.indexOf(':') != raw.lastIndexOf(':')) {
        port = raw.substring(raw.lastIndexOf(':') + 1);
        address = raw.substring(0, raw.lastIndexOf(':'));
    } else if (raw.indexOf(':') == raw.lastIndexOf(':') && raw.indexOf(':') != -1) {
        var parts = raw.split(':');
        port = parts[1];
        address = parts[0] || null;
    } else if (raw.indexOf('.') != raw.lastIndexOf('.')) {
        port = raw.substring(raw.lastIndexOf('.') + 1);
        address = raw.substring(0, raw.lastIndexOf('.'));
    }

    if (address && (address == '::' || address == '0.0.0.0')) {
        address = null;
    }

    return {
        port: port ? parseInt(port) : null,
        address: address
    };
};

exports.normalizeValues = function (item) {
    item.protocol = item.protocol.toLowerCase();
    var parts = item.local.split(':');
    item.local = parseAddress(item.local);
    item.remote = parseAddress(item.remote);

    if (item.protocol == 'tcp' && item.local.address && ~item.local.address.indexOf(':')) {
        item.protocol = 'tcp6';
    }

    if (item.pid == '-') {
        item.pid = 0;
    } else if (~item.pid.indexOf('/')) {
        parts = item.pid.split('/');
        item.pid = parts.length > 1 ? parts[0] : 0;
    } else if (isNaN(item.pid)) {
        item.pid = 0;
    }

    item.pid = parseInt(item.pid);
    return item;
};

exports.emitLines = function (stream) {
    var backlog = '';
    stream.on('data', function (data) {
        backlog = forEachLine(backlog + data, function (line) {
            stream.emit('line', line);
        });
    });

    stream.on('end', function () {
        if (backlog) {
            stream.emit('line', backlog);
        }
    });
};

exports.parseLines = function (source) {
    var lines = [];
    var r = forEachLine(source.toString(), lines.push.bind(lines));
    if (r.length) {
        lines.push(r);
    }

    return lines;
};

exports.noop = function () {};
