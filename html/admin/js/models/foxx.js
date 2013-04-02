window.Foxx = Backbone.Model.extend({
  defaults: {
    "title": "",
    "version": "",
    "mount": "",
    "description": "",
    "git": ""
  },
  
  url: function() {
    return "../../aardvark/foxxes/" + this.get("_key");
  },
  
  isNew: function() {
    return false;
  }
  
});
