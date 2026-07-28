/*
 * Webpack configuration for the aardvark web interface.
 *
 * This replaces the previous `react-scripts` (Create React App) + `react-app-rewired`
 * toolchain, both of which are deprecated and pulled in a large tree of vulnerable
 * transitive dependencies. The output contract is intentionally identical to what CRA
 * produced, because it is consumed verbatim by the Foxx app manifest
 * (`../manifest.json`) and the ArangoDB server:
 *   - build/static/js/*, build/static/css/*, build/static/media/*
 *   - build/assets/**            (copied verbatim from public/assets, incl. pre-gzipped files)
 *   - build/favicon.ico, build/manifest.json
 *   - build/index.html (+ .gz)   (with <include> expanded and %REACT_APP_*% interpolated)
 *   - *.gz variants for compressible assets
 * Production assets use a relative public path ("./") because the app is served under a
 * dynamic, database-scoped URL.
 */
const fs = require("node:fs");
const path = require("node:path");
const webpack = require("webpack");
const HtmlWebpackPlugin = require("html-webpack-plugin");
const MiniCssExtractPlugin = require("mini-css-extract-plugin");
const CssMinimizerPlugin = require("css-minimizer-webpack-plugin");
const CopyPlugin = require("copy-webpack-plugin");
const CompressionPlugin = require("compression-webpack-plugin");
const ReactRefreshWebpackPlugin = require("@pmmmwh/react-refresh-webpack-plugin");
const ForkTsCheckerWebpackPlugin = require("fork-ts-checker-webpack-plugin");

const appDirectory = __dirname;
const resolveApp = (relativePath) => path.resolve(appDirectory, relativePath);

// Load .env.{mode} and .env, mirroring the subset of CRA's env loading this app uses.
function loadEnv(mode) {
  [`.env.${mode}`, ".env"].forEach((file) => {
    const p = resolveApp(file);
    if (fs.existsSync(p)) {
      // dotenv-expand v5 exports a function; v6+ exports { expand }.
      const dotenvExpand = require("dotenv-expand");
      const expand = dotenvExpand.expand || dotenvExpand;
      expand(require("dotenv").config({ path: p }));
    }
  });
}

// Collect NODE_ENV, PUBLIC_URL and every REACT_APP_* variable for injection.
function collectClientEnv(publicUrl) {
  const raw = { NODE_ENV: process.env.NODE_ENV, PUBLIC_URL: publicUrl };
  Object.keys(process.env)
    .filter((key) => /^REACT_APP_/.test(key))
    .forEach((key) => {
      raw[key] = process.env[key];
    });
  const stringified = { "process.env": {} };
  Object.keys(raw).forEach((key) => {
    stringified["process.env"][key] = JSON.stringify(raw[key]);
  });
  return { raw, stringified };
}

/*
 * Replaces %PUBLIC_URL% / %REACT_APP_*% placeholders in index.html after the template
 * is rendered. Reimplements react-dev-utils/InterpolateHtmlPlugin (removed with CRA) in
 * ~15 lines against the same html-webpack-plugin hook.
 */
class InterpolateHtmlPlugin {
  constructor(htmlWebpackPlugin, replacements) {
    this.htmlWebpackPlugin = htmlWebpackPlugin;
    this.replacements = replacements;
  }
  apply(compiler) {
    compiler.hooks.compilation.tap("InterpolateHtmlPlugin", (compilation) => {
      this.htmlWebpackPlugin
        .getHooks(compilation)
        .afterTemplateExecution.tap("InterpolateHtmlPlugin", (data) => {
          Object.keys(this.replacements).forEach((key) => {
            data.html = data.html.replace(
              new RegExp("%" + key + "%", "g"),
              this.replacements[key]
            );
          });
          return data;
        });
    });
  }
}

module.exports = (_webpackEnv, argv) => {
  const isEnvProduction = argv.mode === "production";
  const isEnvDevelopment = !isEnvProduction;

  // Make NODE_ENV/BABEL_ENV available to babel-loader (runs in this process) so that,
  // e.g., react-refresh is only injected in development regardless of how webpack is invoked.
  process.env.NODE_ENV = isEnvProduction ? "production" : "development";
  process.env.BABEL_ENV = process.env.NODE_ENV;

  loadEnv(process.env.NODE_ENV);

  // Relative public path in production (app is served under a dynamic URL), root in dev.
  // Mirrors CRA's getPublicUrlOrPath(PUBLIC_URL=".") => "./" (prod) / "/" (dev).
  let publicUrlOrPath = isEnvProduction ? process.env.PUBLIC_URL || "/" : "/";
  if (!publicUrlOrPath.endsWith("/")) publicUrlOrPath += "/";

  const clientEnv = collectClientEnv(publicUrlOrPath.replace(/\/$/, ""));

  // Style loader chain shared by .css and .scss rules. `preProcessor` (e.g. sass-loader)
  // is appended for .scss, preceded by resolve-url-loader so that url() references
  // declared in imported partials resolve relative to the declaring file (fontawesome
  // fonts live outside the entry scss's directory). resolve-url-loader needs the
  // preprocessor's source map, hence sourceMap: true on both.
  const styleLoaders = (preProcessor) => {
    const loaders = [
      isEnvDevelopment
        ? require.resolve("style-loader")
        : {
            loader: MiniCssExtractPlugin.loader,
            // Relative url() rewriting so extracted CSS resolves media from static/css.
            options: publicUrlOrPath === "./" ? { publicPath: "../../" } : {},
          },
      {
        loader: require.resolve("css-loader"),
        options: { importLoaders: preProcessor ? 3 : 1 },
      },
      {
        loader: require.resolve("postcss-loader"),
        options: {
          postcssOptions: { plugins: [require.resolve("autoprefixer")] },
        },
      },
    ];
    if (preProcessor) {
      loaders.push(
        {
          loader: require.resolve("resolve-url-loader"),
          options: { sourceMap: true },
        },
        { loader: preProcessor, options: { sourceMap: true } }
      );
    }
    return loaders;
  };

  return {
    mode: isEnvProduction ? "production" : "development",
    bail: isEnvProduction,
    devtool: isEnvProduction ? "source-map" : "cheap-module-source-map",
    entry: resolveApp("src/index.tsx"),
    output: {
      path: resolveApp("build"),
      filename: isEnvProduction
        ? "static/js/[name].[contenthash:8].js"
        : "static/js/bundle.js",
      chunkFilename: isEnvProduction
        ? "static/js/[name].[contenthash:8].chunk.js"
        : "static/js/[name].chunk.js",
      assetModuleFilename: "static/media/[name].[hash][ext]",
      publicPath: publicUrlOrPath,
      clean: true,
    },
    infrastructureLogging: { level: "none" },
    optimization: {
      minimize: isEnvProduction,
      // Terser is bundled with webpack; only CSS needs an explicit minimizer.
      minimizer: ["...", new CssMinimizerPlugin()],
    },
    resolve: {
      extensions: [".js", ".mjs", ".jsx", ".ts", ".tsx", ".json"],
      // Allow "img/xyz" references from the legacy frontend to resolve to its img dir.
      alias: { "./img": path.resolve(appDirectory, "../frontend/img") },
      // Node core-module fallbacks for browser bundling (from the old config-overrides).
      fallback: {
        path: require.resolve("path-browserify"),
        querystring: require.resolve("querystring-es3"),
      },
    },
    module: {
      strictExportPresence: true,
      rules: [
        // Our vendored copy of sigma relies on "this" being an alias for "window".
        {
          test: /sigma.*/,
          use: {
            loader: require.resolve("imports-loader"),
            options: { wrapper: "window" },
          },
        },
        // index.html (and other .html) — expand <include src="..."> before html-webpack-plugin.
        // The html-loader no longer supports interpolation natively; see
        // https://github.com/webpack-contrib/html-loader/issues/291#issuecomment-671686973
        {
          test: /\.html$/,
          loader: require.resolve("html-loader"),
          options: {
            preprocessor: (content, loaderContext) =>
              content.replace(
                /<include src="(.+)"\/?>(?:<\/include>)?/gi,
                (_match, src) =>
                  fs.readFileSync(
                    path.resolve(loaderContext.context, src),
                    "utf-8"
                  )
              ),
          },
        },
        {
          oneOf: [
            // EJS templates: raw string in dev (compiled client-side), precompiled in prod.
            isEnvProduction
              ? {
                  test: /\.ejs$/,
                  loader: require.resolve("underscore-template-loader"),
                  options: { attributes: [] },
                }
              : { test: /\.ejs$/i, use: require.resolve("raw-loader") },
            // Application TypeScript/JavaScript (babel.config.js).
            {
              test: /\.(js|mjs|jsx|ts|tsx)$/,
              include: resolveApp("src"),
              loader: require.resolve("babel-loader"),
              options: { cacheDirectory: true },
            },
            { test: /\.css$/, use: styleLoaders() },
            {
              test: /\.scss$/,
              use: styleLoaders(require.resolve("sass-loader")),
            },
            {
              test: [/\.bmp$/, /\.gif$/, /\.jpe?g$/, /\.png$/],
              type: "asset",
              parser: { dataUrlCondition: { maxSize: 10000 } },
            },
            // Everything else (fonts, svg, ...) → emitted to static/media.
            {
              exclude: [/^$/, /\.(js|mjs|jsx|ts|tsx)$/, /\.html$/, /\.json$/],
              type: "asset/resource",
            },
          ],
        },
      ],
    },
    plugins: [
      new HtmlWebpackPlugin({
        inject: true,
        template: resolveApp("public/index.html"),
        ...(isEnvProduction && {
          minify: {
            removeComments: true,
            collapseWhitespace: true,
            removeRedundantAttributes: true,
            useShortDoctype: true,
            removeEmptyAttributes: true,
            removeStyleLinkTypeAttributes: true,
            keepClosingSlash: true,
            minifyJS: true,
            minifyCSS: true,
            minifyURLs: true,
          },
        }),
      }),
      new InterpolateHtmlPlugin(HtmlWebpackPlugin, clientEnv.raw),
      new webpack.DefinePlugin(clientEnv.stringified),
      new CopyPlugin({
        patterns: [
          {
            from: resolveApp("public"),
            to: resolveApp("build"),
            globOptions: { ignore: ["**/index.html"] },
            noErrorOnMissing: true,
          },
        ],
      }),
      isEnvProduction &&
        new MiniCssExtractPlugin({
          filename: "static/css/[name].[contenthash:8].css",
          chunkFilename: "static/css/[name].[contenthash:8].chunk.css",
        }),
      // Compress only webpack-generated output. public/ is copied verbatim (incl. its
      // own pre-gzipped files), so compressing it here would collide with those .gz files.
      isEnvProduction &&
        new CompressionPlugin({ test: /^(static\/|index\.html$)/ }),
      isEnvDevelopment && new ReactRefreshWebpackPlugin({ overlay: false }),
      new ForkTsCheckerWebpackPlugin({
        typescript: { configFile: resolveApp("tsconfig.json") },
      }),
    ].filter(Boolean),
    devServer: {
      port: 3000,
      hot: true,
      // disableDotRule so the served path (…/aardvark/index.html, which contains a dot)
      // is rewritten to the generated index.html instead of 404ing.
      historyApiFallback: { disableDotRule: true },
      static: { directory: resolveApp("public") },
      // Reuse the existing proxy/redirect rules from src/setupProxy.js. Registering them
      // on devServer.app before returning the middleware array runs the /_db/** proxy and
      // the "/" redirect first, while the excluded aardvark/index.html falls through to
      // historyApiFallback. (setupMiddlewares replaced onBeforeSetupMiddleware in wds v5.)
      setupMiddlewares: (middlewares, devServer) => {
        require("./src/setupProxy")(devServer.app);
        return middlewares;
      },
    },
    performance: false,
  };
};
