"use strict";

var spawn = require('child_process').spawn;
var spawnSync = require('child_process').spawnSync;
var emitLines = require('./utils').emitLines;
var parseLines = require('./utils').parseLines;

exports._spawnSync = spawnSync;
exports._spawn = spawn;

exports.sync = function (cmd, args, makeLineHandler, done) {
    var processing = true;
    var lineHandler = makeLineHandler(function () {
        processing = false;
    });

    var proc = exports._spawnSync(cmd, args);
    if (proc.error) {
        return done(proc.error);
    }

    var lines = parseLines(proc.stdout);
    for (var i = 0; i < lines.length && processing; i++) {
        lineHandler(lines[i]);
    }

    return done(null);
};

exports.async = function (cmd, args, makeLineHandler, done) {
    var killed = false;
    var proc = exports._spawn(cmd, args);

    var lineHandler = makeLineHandler(function () {
        if (!killed) {
            proc.stdout.removeListener('line', lineHandler);
            proc.kill();
            killed = true;
        }
    });

    var doneCheck = function (err) {
        if (err || killed) {
            return done(err);
        }
    };

    emitLines(proc.stdout);
    proc.on('error', done);
    proc.on('close', function () {
        done(null);
    });
    
    proc.stdout.on('line', lineHandler);
    return { cancel:
        function() {
            if (!killed) {
                proc.kill();
                killed = true;
            }
        }
    };
};

exports.continuous = function (activator, activatorOptions, options) {
    var cmd = activatorOptions.cmd;
    var args = activatorOptions.args;
    var makeLineHandler = activatorOptions.makeLineHandler;
    var done = activatorOptions.done;
    var sync = options.sync;

    var completed = false;
    var canceller;
    var makeInterceptHandler = function (stopper) {
        var handler = makeLineHandler(function () {
            completed = true;
            stopper();
        });

        return handler;
    };

    function runActivator() {
        canceller = activator(cmd, args, makeInterceptHandler, function (err) {
            if (err) {
                completed = true;
                return done(err);
            } else if (!completed && !sync) {
                setTimeout(runActivator, 0);
                return;
            } else if (completed) {
                done(null);
            }
        });
    }

    if (!sync) {
        runActivator();
        return { cancel:
            function() {
                completed = true;
                canceller.cancel();
            }
        };
    }

    do {
        runActivator();
    } while(!completed);
};
