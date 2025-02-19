// Version: swagger-ui 5.4.1
window.onload = function() {
  //<editor-fold desc="Changeable Configuration Block">

  // the following lines will be replaced by docker/configurator, when it runs in a docker-container
  window.ui = SwaggerUIBundle({
    //#region ArangoDB-specific changes
    // Infer location of swagger.json from `location.href` instead of
    // defaulting to the pet store example.
    url: location.href.replace("index.html", "swagger.json").replace(/#.*/, "").replace(/\?collapsed$/, ""),
    //#endregion
    dom_id: '#swagger-ui',
    deepLinking: true,
    presets: [
      SwaggerUIBundle.presets.apis,
      SwaggerUIStandalonePreset
    ],
    plugins: [
      SwaggerUIBundle.plugins.DownloadUrl
    ],
    layout: "StandaloneLayout",
    //#region ArangoDB-specific changes
    validatorUrl: null,
    docExpansion: new URL(location).searchParams.has("collapsed") ? "none" : "list",
    requestInterceptor: function (request) {
      // Remove fragment identifier from URL, which is between the path and the
      // query parameters if present. It is merely used to disambiguate
      // polymorphic endpoints in the OpenAPI descriptions.
      request.url = request.url.replace(/#[^?]*/, "");
      // Inject the web interface JWT for bearer authentication if the
      // request would otherwise not send an authorization header.
      // This is necessary for the ArangoDB HTTP API documentation to work
      // and for the swagger.json files to be loaded correctly in the
      // API documentation mounted by the ArangoDB web interface in an iframe.
      var jwt = sessionStorage.getItem("jwt");
      if (jwt && !request.headers.Authorization && !request.headers.authorization) {
        request.headers.Authorization = 'bearer ' + jwt;
      }
      return request;
    }
    //#endregion
  });

  //</editor-fold>
};
