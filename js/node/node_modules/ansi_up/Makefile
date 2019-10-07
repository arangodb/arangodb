
SOURCE = *.ts
TESTS = test/*.js
REPORTER = dot

typescript:
		./node_modules/.bin/tsc -p .
		cat ./umd.header ./dist/ansi_up.js ./umd.footer > ansi_up.js
		mv  ./dist/ansi_up.js ./dist/ansi_up.js.include
		node ./scripts/fix-typings.js
		
test:
		@NODE_ENV=test ./node_modules/.bin/mocha \
				--require should \
				--reporter $(REPORTER) \
				$(TESTS)

test_verbose:
		@NODE_ENV=test ./node_modules/.bin/mocha \
				--require should \
				--reporter spec \
				$(TESTS)

clean:
		rm -rf ./node_modules ansi_up.js

.PHONY:	test
