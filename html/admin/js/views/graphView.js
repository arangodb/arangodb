/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true, forin: true */
/*global Backbone, $, window, EJS, GraphViewerUI */

window.graphView = Backbone.View.extend({
  el: '#content',

  template: new EJS({url: 'js/templates/graphView.ejs'}),

  initialize: function () {
    var self = this;
    this.newLineTmpl = new EJS({url: "js/templates/graphViewGroupByEntry.ejs"});
    this.graphs = [];
    this.i = 1;
  },

  events: {
    "click input[type='radio'][name='loadtype']": "toggleLoadtypeDisplay",
    "click #createViewer": "createViewer",
    "click #add_group_by": "insertNewAttrLine",
    "click .gv_internal_remove_line": "removeAttrLine"
  },

  removeAttrLine: function(e) {
    var g = $(e.currentTarget)
      .parent()
      .parent(),
      set = g.parent();
    set.get(0).removeChild(g.get(0));
  }, 


  insertNewAttrLine: function() {
    this.i++;
    var next = this.newLineTmpl.render({id: this.i});
    $("#group_by_list").append(next);
  },

  toggleLoadtypeDisplay: function() {
    var selected = $("input[type='radio'][name='loadtype']:checked").attr("id");
    if (selected === "collections") {
      $("#collection_config").css("display", "block");
      $("#graph_config").css("display", "none");
    } else {
      $("#collection_config").css("display", "none");
      $("#graph_config").css("display", "block");
    }
  },

  createViewer: function() {
    var ecol,
    ncol,
    aaconfig,
    undirected,
    randomStart,
    groupByList,
    groupByAttribute,
    label,
    config,
    ui;

    ecol = $("#edgeCollection").val();
    ncol = $("#nodeCollection").val();
    undirected = !!$("#undirected").attr("checked");
    label = $("#nodeLabel").val();
    randomStart = !!$("#randomStart").attr("checked");

    groupByAttribute = [];
    $("#group_by_list input").each(function() {
      var a = $(this).val();
      if (a) {
        groupByAttribute.push(a);
      }
    });

    aaconfig = {
      type: "arango",
      nodeCollection: ncol,
      edgeCollection: ecol,
      undirected: undirected
    };
    
    if (groupByAttribute.length > 0) {
      aaconfig.prioList = groupByAttribute;
    }

    if (label !== undefined && label !== "") {
      config = {
        nodeShaper: {
          label: label
        }
      };
    }

    $("#background").remove();
    if (randomStart) {
      $.ajax({
        cache: false,
        type: 'PUT',
        url: '/_api/simple/any',
        data: JSON.stringify({
          collection: ncol
        }),
        contentType: "application/json",
        success: function(data) {
          ui = new GraphViewerUI($("#content")[0], aaconfig, 940, 680, config, data.document._id);
        }
      });
    } else {
      ui = new GraphViewerUI($("#content")[0], aaconfig, 940, 680, config);
    }
  },


  render: function() {
    var self = this;
    $.ajax({
      cache: false,
      type: 'GET',
      url: "/_api/graph",
      contentType: "application/json",
      success: function(data) {
        self.graphs = _.pluck(data.graphs, "_key");
        $(self.el).html(self.template.render({col: self.collection, gs: self.graphs}));
      }
    });
    return this;
  }

});
