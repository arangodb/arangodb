'use strict';
var gzipSize = require('gzip-size');
var prettyBytes = require('pretty-bytes');
var chalk = require('chalk');
var figures = require('figures');
var arrow = ' ' + figures.arrowRight + ' ';

function format(size) {
	return chalk.green(prettyBytes(size));
}

module.exports = function (max, min, useGzip) {
	if (max == null || min == null) {
		throw new Error('`max` and `min` required');
	}

	var ret = format(typeof max === 'number' ? max : max.length) + arrow + format(typeof min === 'number' ? min : min.length);

	if (useGzip === true && typeof min !== 'number') {
		ret += arrow + format(gzipSize.sync(min)) + chalk.gray(' (gzip)');
	}

	return ret;
};
