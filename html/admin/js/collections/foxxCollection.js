window.FoxxCollection = Backbone.Collection.extend({
  model: window.Foxx,
  
  url: "http://localhost:8529/aardvark/aardvark/foxxes"
});
