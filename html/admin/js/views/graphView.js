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
    var ecol,
      ncol,
      aaconfig,
      undirected,
      label,
      config;
    
    
    ecol = $("#edgeCollection").val();
    ncol = $("#nodeCollection").val();
    undirected = !!$("#undirected").attr("checked");
    label = $("#nodeLabel").val();
    
    aaconfig = {
      type: "arango",
      nodeCollection: ncol,
      edgeCollection: ecol,
      undirected: undirected
    };
    
    if (label !== undefined && label !== "") {
      console.log(label);
      config = {
        nodeShaper: {
          label: label
        }
      }
      console.log(config);
    }

    $("#creationDialog").remove();
    ui = new GraphViewerUI(document.getElementById("content"), aaconfig, 940, 680, config);
  },


  render: function() {
    $(this.el).html(this.template.text);
    
    return this;
  }

});
