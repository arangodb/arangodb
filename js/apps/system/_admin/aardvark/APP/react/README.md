This is the React-based aardvark web interface. It is built with a hand-maintained
[webpack](https://webpack.js.org/) configuration ([webpack.config.js](webpack.config.js))
and [Babel](https://babeljs.io/) ([babel.config.js](babel.config.js)). It was previously
built with Create React App (`react-scripts`) via `react-app-rewired`; both were removed
because they are deprecated and pulled in a large tree of vulnerable dependencies.

## Available Scripts

In the project directory, you can run:

### `yarn start`

Runs the app in development mode via `webpack serve` on
[http://localhost:3000](http://localhost:3000), with Fast Refresh. Requests to `/_db/**`
are proxied to a local ArangoDB (`ARANGODB_HOST`/`ARANGODB_PORT`/`ARANGODB_SSL`, defaults
`localhost:8529`, non-SSL); see [src/setupProxy.js](src/setupProxy.js).

### `yarn run build`

Builds the app for production into the `build` folder: minified, content-hashed, and
gzip-compressed. This output is served verbatim by the Foxx app (see `../manifest.json`),
so its layout (`static/{js,css,media}`, copied `assets/`, `favicon.ico`, `index.html`
plus `.gz` variants) is a contract — change it only in step with the manifest.

### `yarn lint`

Runs ESLint over the sources.
