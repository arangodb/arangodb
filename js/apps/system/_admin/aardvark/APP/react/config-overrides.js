const fs = require("node:fs");
const path = require("node:path");
const CompressionPlugin = require("compression-webpack-plugin");

module.exports = {
  webpack: (config, env) => {
    Object.assign(config.resolve.alias, {
      "old-frontend": path.resolve(__dirname, "../frontend"),
      "/img": path.resolve(__dirname, "../frontend/img"),
      "./img": path.resolve(__dirname, "../frontend/img"),
      img: path.resolve(__dirname, "../frontend/img"),
    });
    config.resolve.plugins = [];
    config.resolve.fallback = {
      path: require.resolve("path-browserify"),
      querystring: require.resolve("querystring-es3"),
      url: require.resolve("url/"),
    };
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
    const loaders = config.module.rules.at(-1).oneOf;
    loaders.splice(-1, 0, {
      test: /\.css$/,
      use: ["style-loader", "css-loader"],
    });
    loaders.at(-1).exclude = [
      /^$/,
      /\.(js|mjs|jsx|ts|tsx|ejs)$/,
      /\.html$/,
      /\.json$/,
    ];
    if (env === "production") {
      config.module.rules.unshift({
        test: /\.ejs$/,
        loader: "underscore-template-loader",
        options: {
          attributes: [],
        },
      });
      config.plugins.push(new CompressionPlugin());
    } else {
      config.module.rules.unshift({
        test: /\.ejs$/i,
        use: "raw-loader",
      });
    }
    return config;
  },
};
