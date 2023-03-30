const fs = require("node:fs");
const path = require("node:path");

module.exports = {
  webpack: (config, _env) => {
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
    config.module.rules.unshift(
      {
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
      },
      {
        test: /\.ejs$/i,
        use: "raw-loader",
      }
    );
    const catchAllRule = config.module.rules.at(-1);
    catchAllRule.oneOf.splice(-1, 0, {
      test: /\.css$/,
      use: ["style-loader", "css-loader"],
    });
    catchAllRule.oneOf.at(-1).exclude = [
      /^$/,
      /\.(js|mjs|jsx|ts|tsx|ejs)$/,
      /\.html$/,
      /\.json$/,
    ];
    return config;
  },
};
