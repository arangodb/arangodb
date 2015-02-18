#!/usr/bin/env node
'use strict';
var fs = require('fs');
var zlib = require('zlib');
var concat = require('concat-stream');
var pkg = require('./package.json');
var argv = process.argv.slice(2);
var input = argv[0];

function help() {
	console.log([
		'',
		'  ' + pkg.description,
		'',
		'  Usage',
		'    gzip-size <file>',
		'    cat <file> | gzip-size',
		'',
		'  Example',
		'    gzip-size index.js',
		'    211'
	].join('\n'));
}

function report(data) {
	console.log(data.length);
}

if (argv.indexOf('--help') !== -1) {
	help();
	return;
}

if (argv.indexOf('--version') !== -1) {
	console.log(pkg.version);
	return;
}

if (process.stdin.isTTY) {
	if (!input) {
		help();
		return;
	}

	fs.createReadStream(input).pipe(zlib.createGzip()).pipe(concat(report));
} else {
	process.stdin.pipe(zlib.createGzip()).pipe(concat(report));
}
