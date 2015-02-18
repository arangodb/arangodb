#!/usr/bin/env node
'use strict';
var stdin = require('get-stdin');
var prettyBytes = require('./pretty-bytes');
var pkg = require('./package.json');
var argv = process.argv.slice(2);
var input = argv[0];

function help() {
	console.log([
		'',
		'  ' + pkg.description,
		'',
		'  Usage',
		'    pretty-bytes <number>',
		'    echo <number> | pretty-bytes',
		'',
		'  Example',
		'    pretty-bytes 1337',
		'    1.34 kB'
	].join('\n'));
}

function init(data) {
	console.log(prettyBytes(Number(data)));
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

	init(input);
} else {
	stdin(init);
}
