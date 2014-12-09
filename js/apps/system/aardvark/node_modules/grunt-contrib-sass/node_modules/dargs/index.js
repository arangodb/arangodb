'use strict';

function createArg(key, val) {
	key = key.replace(/[A-Z]/g, '-$&').toLowerCase();
	return '--' + key + (val ? '=' + val : '');
};

module.exports = function (opts, excludes, includes) {
	var args = [];

	Object.keys(opts).forEach(function (key) {
		var val = opts[key];

		if (Array.isArray(excludes) && excludes.indexOf(key) !== -1) {
			return;
		}

		if (Array.isArray(includes) && includes.indexOf(key) === -1) {
			return;
		}

		if (val === true) {
			args.push(createArg(key));
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
