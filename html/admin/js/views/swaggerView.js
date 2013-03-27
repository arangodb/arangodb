window.SwaggerView = Backbone.View.extend({

  el: '#content',
  template: new EJS({url: '/_admin/html/js/templates/swaggerView.ejs'}),
  
  initialize: function() {
    window.swaggerUi = new SwaggerUi({
        discoveryUrl:"../../aardvark/swagger",
        apiKey: false,
        dom_id:"swagger-ui-container",
        supportHeaderParams: true,
        supportedSubmitMethods: ['get', 'post', 'put', 'delete', 'patch', 'head'],
        onComplete: function(swaggerApi, swaggerUi){
        	if(console) {
                console.log("Loaded SwaggerUI")
                console.log(swaggerApi);
                console.log(swaggerUi);
            }
          $('pre code').each(function(i, e) {hljs.highlightBlock(e)});
        },
        onFailure: function(data) {
        	if(console) {
                console.log("Unable to Load SwaggerUI");
                console.log(data);
            }
        },
        docExpansion: "none"
    });
  },
  
  render: function(){
    $(this.el).html(this.template.render({}));
    window.swaggerUi.load();
    return this;
  }
});
