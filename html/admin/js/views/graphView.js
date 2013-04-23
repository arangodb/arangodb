window.graphView = Backbone.View.extend({
  el: '#content',
  
  template: new EJS({url: 'js/templates/graphView.ejs'}),
  
  initialize: function () {
    var self = this;

  },

  events: {
    "click #createViewer" : "createViewer"
  },

  createViewer: function() {
    var host,
    ecol,
    ncol,
    aaconfig;
    
    host = $("#host")[0].value;
    ecol = $("#edgeCollection")[0].value;
    ncol = $("#nodeCollection")[0].value;
           
    aaconfig = {
      type: "arango",
      nodeCollection: ncol,
      edgeCollection: ecol,
      host: host
    };

    $("#creationDialog").remove();
    ui = new GraphViewerUI(document.getElementById("content"), aaconfig);
  },


  render: function() {
    $(this.el).html(this.template.text);
    
    return this;
  }

});
