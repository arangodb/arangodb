/*
 * Proxy Configuration (see routes array for details)
 */

const ARANGODB_HOST = process.env.ARANGODB_HOST || "localhost";
const ARANGODB_PORT = !isNaN(process.env.ARANGODB_PORT)
  ? Number(process.env.ARANGODB_PORT) : 8529;

const { createProxyMiddleware } = require("http-proxy-middleware");
module.exports = function (app) {
  app.get("/", (_req, res) => {
    res.redirect("/_db/_system/_admin/aardvark/index.html");
  });
  app.use(
    createProxyMiddleware(["/_db/**", "!/_db/*/_admin/aardvark/index.html"], {
      target: `http://${ARANGODB_HOST}:${ARANGODB_PORT}`,
      changeOrigin: true,
    })
  );
};
