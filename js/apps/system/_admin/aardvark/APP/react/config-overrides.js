const fs = require("node:fs");
const path = require("node:path");
const CompressionPlugin = require("compression-webpack-plugin");

module.exports = {
  webpack: (config, env) => {
    // Allow using "img/xyz" in the legacy frontend
    config.resolve.alias["./img"] = path.resolve(__dirname, "../frontend/img");

    // Disable the module scope plugin to allow importing from the old frontend
    config.resolve.plugins = [];

    // Add node compatibility fallbacks
    config.resolve.fallback = {
      path: require.resolve("path-browserify"),
      querystring: require.resolve("querystring-es3"),
      url: require.resolve("url/"),
    };

    // Our vendored copy of sigma relies on "this" being an alias for "window"
    config.module.rules.unshift({
      test: /sigma.*/,
      use: {
        loader: "imports-loader",
        options: {
          wrapper: "window",
        },
      },
    });

    // The html-loader no longer supports interpolation, which we use in
    // index.html to inject the legacy frontend scaffolding. Workaround via:
    // https://github.com/webpack-contrib/html-loader/issues/291#issuecomment-671686973
    config.module.rules.unshift({
      test: /\.html$/,
      use: {
        loader: "html-loader",
        options: {
          preprocessor: (content, loaderContext) =>
            content.replace(
              /<include src="(.+)"\/?>(?:<\/include>)?/gi,
              (_, src) =>
                fs.readFileSync(
                  path.resolve(loaderContext.context, src),
                  "utf-8"
                )
            ),
        },
      },
    });

    // Enable ejs template support
    config.module.rules.at(-1).oneOf.at(-1).exclude.push(/\.ejs$/);
    config.module.rules.unshift(
      env === "production"
        ? {
            test: /\.ejs$/,
            loader: "underscore-template-loader",
            options: {
              attributes: [],
            },
          }
        : {
            test: /\.ejs$/i,
            use: "raw-loader",
          }
    );

    if (env === "production") {
      // Generate .gz files in the production build
      config.plugins.push(new CompressionPlugin());
    }
    return config;
  },
};
