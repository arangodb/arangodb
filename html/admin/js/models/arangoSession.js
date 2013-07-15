window.Session = Backbone.Model.extend({
  defaults: {
    sessionId: "",
    userName: "",
    password: "",
    userId: "",
    data: {}
  },

  initialize: function () {
  },

  isAuthorized: function () {
    //return Boolean(this.get("sessionId");
    return true;
  },

  isNotAuthorized: function () {
    return false;
  }

});
