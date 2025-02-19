/*
 * Proxy Configuration (see routes array for details)
 */

const ARANGODB_HOST = process.env.ARANGODB_HOST || "localhost";
const ARANGODB_PORT = process.env.ARANGODB_PORT || "8529";
const ARANGODB_SSL = process.env.ARANGODB_SSL === "1";

if (isNaN(ARANGODB_PORT)) {
  console.error("ARANGODB_PORT must be a number or empty");
  process.exit(1);
}

const { createProxyMiddleware } = require("http-proxy-middleware");
module.exports = function (app) {
  app.get("/", (_req, res) => {
    res.redirect("/_db/_system/_admin/aardvark/index.html");
  });
  app.use(
    createProxyMiddleware(["/_db/**", "!/_db/*/_admin/aardvark/index.html"], {
      target: `${
        ARANGODB_SSL ? "https" : "http"
      }://${ARANGODB_HOST}:${ARANGODB_PORT}`,
      changeOrigin: true
    })
  );
};
