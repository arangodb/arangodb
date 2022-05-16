const path = require('path');
const paths = require('./paths');
const fs = require('fs');
const PnpWebpackPlugin = require('pnp-webpack-plugin');

// Check if TypeScript is setup
const useTypeScript = fs.existsSync(paths.appTsConfig);

module.exports = {
  // This allows you to set a fallback for where Webpack should look for modules.
  // We placed these paths second because we want `node_modules` to "win"
  // if there are any conflicts. This matches Node resolution mechanism.
  // https://github.com/facebook/create-react-app/issues/253
  modules: ['node_modules'].concat(
    // It is guaranteed to exist because we tweak it in `env.js`
    process.env.NODE_PATH.split(path.delimiter).filter(Boolean)
  ),
  // These are the reasonable defaults supported by the Node ecosystem.
  // We also include JSX as a common component filename extension to support
  // some tools, although we do not recommend using it, see:
  // https://github.com/facebook/create-react-app/issues/290
  // `web` extension prefixes have been added for better support
  // for React Native Web.
  extensions: paths.moduleFileExtensions.map(ext => `.${ext}`).filter(
    ext => useTypeScript || !ext.includes('ts')),
  alias: {
    // Support React Native Web
    // https://www.smashingmagazine.com/2016/08/a-glimpse-into-the-future-with-react-native-for-web/
    'react-native': 'react-native-web',
    'old-frontend': path.resolve(__dirname, '../frontend'),
    '/img': path.resolve(__dirname, '../../frontend/img'),
    './img': path.resolve(__dirname, '../../frontend/img'),
    'img': path.resolve(__dirname, '../../frontend/img')
  },
  plugins: [
    // Adds support for installing with Plug'n'Play, leading to faster installs and adding
    // guards against forgotten dependencies and such.
    PnpWebpackPlugin
    // Prevents users from importing files from outside of src/ (or node_modules/).
    // This often causes confusion because we only process files within src/ with babel.
    // To fix this, we prevent you from importing files out of src/ -- if you'd like to,
    // please link the files into your node_modules/ and let module-resolution kick in.
    // Make sure your source files are compiled, as they will not be processed in any way.
    // new ModuleScopePlugin(paths.appSrc, [paths.appPackageJson]), // TODO: enable after
    // finished refactoring
  ]
};
