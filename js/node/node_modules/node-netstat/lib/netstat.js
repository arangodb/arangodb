"use strict";

var os = require('os');
var activators = require('./activators');
var utils = require('./utils');
var parsers = require('./parsers');
var parserFactories = require('./parser-factories');
var filters = require('./filters');
var pkg = require('../package');
const isWsl = require('is-wsl');

var commands = {
    linux: {
        cmd: 'netstat',
        args: ['-apntu']
    },
    darwin: {
        cmd: 'netstat',
        args: ['-v', '-n', '-p', 'tcp']
    },
    win32: {
        cmd: 'netstat.exe',
        args: ['-a', '-n', '-o']
    }
};

module.exports = function (options, callback) {
    options = options || {};
    var done = options.done || utils.noop;
    var platform = options.platform || (isWsl ? 'win32' : false) || os.platform();
    var command = commands[platform];
    var parser = parsers[platform];
    var handler = callback;
    var activator = options.sync ? activators.sync : activators.async;

    var makeLineHandler = function (stopParsing) {
        return function (line) {
            if (parser(line, handler) === false) {
                stopParsing();
            }
        };
    };

    if (!parser || !command) {
        throw new Error('platform is not supported.');
    }

    if (options.limit && options.limit > 0) {
        handler = filters.limit(handler, options.limit, utils.noop);
    }

    if (options.filter) {
        handler = filters.conditional(handler, options.filter);
    }

    if (options.watch) {
        return activators.continuous(activator, {
            cmd: command.cmd,
            args: command.args,
            makeLineHandler: makeLineHandler,
            done: done
        }, { sync: options.sync });

    } else {
        return activator(command.cmd, command.args, makeLineHandler, done);
    }
};

module.exports.commands = commands;
module.exports.filters = filters;
module.exports.parsers = parsers;
module.exports.utils = utils;
module.exports.parserFactories = parserFactories;
module.exports.version = pkg.version;
