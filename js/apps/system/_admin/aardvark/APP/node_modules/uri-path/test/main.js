'use strict';

var URIpath = require('..');
require('should');

describe('uri-path', function() {
	it('should convert relative file system paths into properly encoded URIs', function() {
		var paths = {
			// file system path: expected URI path
			'../abc/@#$%¨&()[]{}-_=+ß/môòñ 月 قمر': '../abc/%40%23%24%25%C2%A8%26()%5B%5D%7B%7D-_%3D%2B%C3%9F/m%C3%B4%C3%B2%C3%B1%20%E6%9C%88%20%D9%82%D9%85%D8%B1',
			'a\\b\\c': 'a/b/c',
		};
		Object.keys(paths).forEach(function(filePath) {
			URIpath(filePath).should.equal(paths[filePath]);
		});
	});
});
