'use strict';

var spawn = require('child_process').spawn;
var isJpg = require('is-jpg');
var jpegtran = require('jpegtran-bin');
var through = require('through2');

module.exports = function (opts) {
	opts = opts || {};

	return through.ctor({objectMode: true}, function (file, enc, cb) {
		if (file.isNull()) {
			cb(null, file);
			return;
		}

		if (file.isStream()) {
			cb(new Error('Streaming is not supported'));
			return;
		}

		if (!isJpg(file.contents)) {
			cb(null, file);
			return;
		}

		var args = ['-copy', 'none', '-optimize'];
		var ret = [];
		var len = 0;
		var err = '';

		if (opts.progressive) {
			args.push('-progressive');
		}

		if (opts.arithmetic) {
			args.push('-arithmetic');
		}

		var cp = spawn(jpegtran, args);

		cp.stderr.setEncoding('utf8');
		cp.stderr.on('data', function (data) {
			err += data;
		});

		cp.stdout.on('data', function (data) {
			ret.push(data);
			len += data.length;
		});

		cp.on('error', function (err) {
			err.fileName = file.path;
			cb(err);
			return;
		});

		cp.on('close', function () {
			var contents = Buffer.concat(ret, len);

			if (err && (err.code !== 'EPIPE' || !isJpg(contents))) {
				err = typeof err === 'string' ? new Error(err) : err;
				err.fileName = file.path;
				cb(err);
				return;
			}

			if (len < file.contents.length) {
				file.contents = contents;
			}

			cb(null, file);
		});

		cp.stdin.on('error', function (stdinErr) {
			if (!err) {
				err = stdinErr;
			}
		});

		cp.stdin.end(file.contents);
	});
};
