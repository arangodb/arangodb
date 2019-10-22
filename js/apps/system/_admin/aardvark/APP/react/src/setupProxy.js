'use strict';

const proxy = require('http-proxy-middleware');
module.exports = function(app) {
  app.use(
    '*',
    proxy({
      target: 'http://localhost:8529',
      changeOrigin: true
    })
  );
};
