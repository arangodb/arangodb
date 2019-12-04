'use strict';

function isBinary(buf) {
	var isBuf = Buffer.isBuffer(buf);

	for (var i = 0; i < 24; i++) {
		var charCode = isBuf ? buf[i] : buf.charCodeAt(i);

		if (charCode === 65533 || charCode <= 8) {
			return true;
		}
	}

	return false;
}

module.exports = function (buf) {
	return !isBinary(buf) && /<svg[^>]*>[^]*<\/svg>\s*$/.test(buf);
};
