/*jshint browser: true */
/*global SwaggerUi, $, hljs, arangoHelper */
(function() {
  "use strict";
  var query = window.location.search.substring(1);
  var vars = query.split("&");
  var appUrl = "";
  var i;
  for (i = 0; i < vars.length; ++i) {
    if (vars[i].split("=")[0] === "path") {
      appUrl = vars[i].split("=")[1];
    }
  }
  if (appUrl === "") {
    var div = document.createElement("div"),
      strong = document.createElement("strong");
    strong.appendChild(
      document.createTextNode(
        "Sorry App not found. Please use the query param path and submit the urlEncoded mount point of an App."
      )
    );
    div.appendChild(strong);
    $("#swagger-ui-container").append(div);
    return;
  }
  var url = arangoHelper.databaseUrl("/_admin/aardvark/foxxes/billboard?mount=" + appUrl);
  var swaggerUi = new SwaggerUi({
    discoveryUrl: url,
    apiKey: false,
    dom_id: "swagger-ui-container",
    supportHeaderParams: true,
    supportedSubmitMethods: ['get', 'post', 'put', 'delete', 'patch', 'head'],
    onComplete: function(){
      $('pre code').each(function(i, e) {hljs.highlightBlock(e);});
    },
    onFailure: function(data) {
      var div = document.createElement("div"),
        strong = document.createElement("strong");
      strong.appendChild(
        document.createTextNode("Sorry the code is not documented properly")
      );
      div.appendChild(strong);
      div.appendChild(document.createElement("br"));
      div.appendChild(document.createTextNode(JSON.stringify(data)));
      $("#swagger-ui-container").append(div);
    },
    docExpansion: "list"
  });
  swaggerUi.load();
}());
