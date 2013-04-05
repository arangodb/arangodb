window.AppDocumentationView = Backbone.View.extend({

  el: '#content',
  template: new EJS({url: 'js/templates/appDocumentationView.ejs'}),
  
  initialize: function() {
    this.swaggerUi = new SwaggerUi({
        discoveryUrl:"../aardvark/swagger",
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
    this.swaggerUi.load();
    return this;
  }
});
