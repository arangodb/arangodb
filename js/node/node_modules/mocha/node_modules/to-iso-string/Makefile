
build: components node_modules
	@component build --dev

clean:
	@rm -rf components build node_modules

components: component.json
	@component install --dev

node_modules: package.json
	@npm install

test: build
	@./node_modules/.bin/mocha --reporter spec

.PHONY: clean test