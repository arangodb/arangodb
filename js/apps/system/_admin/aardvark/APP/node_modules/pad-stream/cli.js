#!/usr/bin/env node
'use strict';
var fs = require('fs');
var meow = require('meow');
var padStream = require('./');

var cli = meow({
	help: [
		'Usage',
		'  pad <filename>',
		'  echo <text> | pad',
		'',
		'Example',
		'  echo \'foo\\nbar\' | pad --count=4',
		'      foo',
		'      bar',
		'',
		'Options',
		'  --indent  Indent string',
		'  --count   Times to repeat indent string'
	].join('\n')
});

var file = cli.input[0];

if (!file && process.stdin.isTTY) {
	console.error('Specify a filename');
	process.exit(2);
	return;
}

var pad = padStream(cli.flags.indent, cli.flags.count);

if (file) {
	fs.createReadStream(file).pipe(pad).pipe(process.stdout);
} else {
	process.stdin.pipe(pad).pipe(process.stdout);
}
