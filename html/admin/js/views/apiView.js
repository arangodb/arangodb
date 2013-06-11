/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, EJS, $*/

window.apiView = Backbone.View.extend({
  el: '#content',
  init: function () {
  },

  template: new EJS({url: 'js/templates/apiView.ejs'}),

  initialize: function() {
    this.swaggerUi = new SwaggerUi({
        discoveryUrl:"api-docs.json",

        apiKey: false,
        dom_id:"swagger-ui-container",
        supportHeaderParams: true,
        supportedSubmitMethods: ['get', 'post', 'put', 'delete', 'patch', 'head'],
        onComplete: function(swaggerApi, swaggerUi){
          $('pre code').each(function(i, e) {hljs.highlightBlock(e);});
        },
        onFailure: function(data) {
          var div = document.createElement("div"),
            strong = document.createElement("strong");
          strong.appendChild(document.createTextNode("Sorry the code is not documented properly"));
          div.appendChild(strong);
          div.appendChild(document.createElement("br"));
          div.appendChild(document.createTextNode(JSON.stringify(data)));
          $("#swagger-ui-container").append(div);
        },
        docExpansion: "none"
    });
  },
  
  render: function(){
    $(this.el).html(this.template.render({}));
    this.swaggerUi.load();
    return this;
  }

});
