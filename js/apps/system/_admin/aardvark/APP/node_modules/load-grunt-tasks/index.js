'use strict';
var path = require('path');
var pkgUp = require('pkg-up');
var multimatch = require('multimatch');
var arrify = require('arrify');
var resolvePkg = require('resolve-pkg');

module.exports = function (grunt, opts) {
	opts = opts || {};

	var pattern = arrify(opts.pattern || ['grunt-*', '@*/grunt-*']);
	var config = opts.config || pkgUp.sync();
	var scope = arrify(opts.scope || ['dependencies', 'devDependencies', 'peerDependencies', 'optionalDependencies']);

	if (typeof config === 'string') {
		config = require(path.resolve(config));
	}

	pattern.push('!grunt', '!grunt-cli');

	var names = scope.reduce(function (result, prop) {
		var deps = config[prop] || [];
		return result.concat(Array.isArray(deps) ? deps : Object.keys(deps));
	}, []);

	multimatch(names, pattern).forEach(function (pkgName) {
		if (opts.requireResolution === true) {
			try {
				grunt.loadTasks(resolvePkg(path.join(pkgName, 'tasks')));
			} catch (err) {
				grunt.log.error('npm package "' + pkgName + '" not found. Is it installed?');
			}
		} else {
			grunt.loadNpmTasks(pkgName);
		}
	});
};
