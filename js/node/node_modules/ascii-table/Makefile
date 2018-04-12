SHELL := /bin/bash

test:
	@mocha -R spec test.js

hint:
	@jshint ascii-table.js test.js package.json

# UglifyJS v2
min:
	@echo -n ';' > ascii-table.min.js; uglifyjs ascii-table.js -o ascii-table.min.js -c -m;

.PHONY: test hint min 