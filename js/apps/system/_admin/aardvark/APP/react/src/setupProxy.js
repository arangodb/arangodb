/*
 * Proxy Configuration (see routes array for details)
 */

const ARANGODB_PORT = 8529;

const routes = [
  "/_db/_system/_admin/aardvark/img/*", // Images
  "/_db/_system/_admin/aardvark/statistics/*", // Statistics (foxx)
  "/_db/_system/_api/", // ArangoDB API
];

const { createProxyMiddleware } = require("http-proxy-middleware");
module.exports = function (app) {
  app.use(
    routes,
    createProxyMiddleware({
      target: `http://localhost:${ARANGODB_PORT}`,
      changeOrigin: true,
    })
  );
};
