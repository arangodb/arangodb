'use strict';
var gzip = require('zlib').gzip;
var gzipSync = require('browserify-zlib').gzipSync;

module.exports = function (str, cb) {
	if (!str) {
		cb(null, 0);
		return;
	}

	gzip(str, function (err, data) {
		if (err) {
			cb(err, 0);
			return;
		}

		cb(err, data.length);
	});
};

module.exports.sync = function (str) {
	return gzipSync(str).length;
};
