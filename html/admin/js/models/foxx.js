window.Foxx = Backbone.Model.extend({
  defaults: {
    "title": "",
    "version": "",
    "mount": "",
    "description": "",
    "git": ""
  },
  
  url: function() {
    if (this.get("_key")) {
      return "../aardvark/foxxes/" + this.get("_key");
    }
    return "../aardvark/foxxes/install";
  },
  
  isNew: function() {
    return false;
  }
  
});
