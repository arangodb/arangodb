'use strict';

var spawn = require('child_process').spawn;
var gifsicle = require('gifsicle');
var isGif = require('is-gif');
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

		if (!isGif(file.contents)) {
			cb(null, file);
			return;
		}

		var args = ['-w'];
		var ret = [];
		var len = 0;

		if (opts.interlaced) {
			args.push('--interlace');
		}

		var cp = spawn(gifsicle, args);

		cp.stderr.setEncoding('utf8');
		cp.stderr.on('data', function (data) {
			var err = new Error(data);
			err.fileName = file.path;
			cb(err);
			return;
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
			if (len < file.contents.length) {
				file.contents = Buffer.concat(ret, len);
			}

			cb(null, file);
		});

		cp.stdin.end(file.contents);
	});
};
