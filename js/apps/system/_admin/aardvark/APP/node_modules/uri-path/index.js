'use strict';

module.exports = function(path) {
	return path.split(/[\\\/]/g).map(encodeURIComponent).join('/');
};
