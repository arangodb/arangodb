'use strict';

function createArg(key, val) {
	key = key.replace(/[A-Z]/g, '-$&').toLowerCase();
	return '--' + key + (val ? '=' + val : '');
};

module.exports = function (input, opts) {
	var args = [];

	opts = opts || {};

	Object.keys(input).forEach(function (key) {
		var val = input[key];

		if (Array.isArray(opts.excludes) && opts.excludes.indexOf(key) !== -1) {
			return;
		}

		if (Array.isArray(opts.includes) && opts.includes.indexOf(key) === -1) {
			return;
		}

		if (val === true) {
			args.push(createArg(key));
		}

		if (val === false && !opts.ignoreFalse) {
			args.push(createArg('no-' + key));
		}

		if (typeof val === 'string') {
			args.push(createArg(key, val));
		}

		if (typeof val === 'number' && isNaN(val) === false) {
			args.push(createArg(key, '' + val));
		}

		if (Array.isArray(val)) {
			val.forEach(function (arrVal) {
				args.push(createArg(key, arrVal));
			});
		}
	});

	return args;
};
