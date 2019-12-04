'use strict';
var split = require('split2');
var through = require('through2');
var pumpify = require('pumpify');
var repeating = require('repeating');

module.exports = function (indent, count) {
	indent = indent || ' ';
	count = typeof count === 'number' ? count : 1;

	return pumpify(split(), through(function (data, enc, cb) {
		cb(null, repeating(indent, count) + data + '\n');
	}));
};
