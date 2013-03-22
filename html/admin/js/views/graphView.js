var graphView = Backbone.View.extend({
  el: '#content',
  svgWidth: 898,
  svgHeight: 350,
  scaleFactor: 1,
  NODERADIUS: 12, // Constant Node Radius
  LINKDIST: 100, // Constant Link Distance
  handles: {},
  
  template: new EJS({url: '/_admin/html/js/templates/graphView.ejs'}),
  
  initialize: function () {
    var self = this;
    self.force = d3.layout.force();
    self.force.size([self.svgWidth, self.svgHeight]);
    self.force.charge(-300);
  },

  events: {
    'click #submitQueryIcon'   : 'submitQuery',
    'click #submitQueryButton' : 'submitQuery',
    'click .clearicon': 'clearOutput'
  },

  clearOutput: function() {
    $('#graphOutput').empty();
  },

  submitQuery: function() {
    // TODO!
    var self = this;
    //self.renderGraph("js/graph/testdata/friends.json");
    self.renderGraph("js/graph/testdata/big.json");
    /*
    var editor = ace.edit("aqlEditor");
    var data = {query: editor.getValue()};

    var editor2 = ace.edit("queryOutput");

    $.ajax({
      type: "POST",
      url: "/_api/cursor",
      data: JSON.stringify(data),
      contentType: "application/json",
      processData: false,
      success: function(data) {
        editor2.setValue(self.FormatJSON(data.result));
      },
      error: function(data) {
        try {
          var temp = JSON.parse(data.responseText);
          editor2.setValue('[' + temp.errorNum + '] ' + temp.errorMessage);
        }
        catch (e) {
          editor2.setValue('ERROR');
        }
      }
    });
    */
  },

  renderGraph: function(jsonURL) {
    var self = this;
    if (self.force) {
      self.force.stop();
    }
    $('#graphSVG').empty();
    d3.json(jsonURL, function(error, graph) {
      var s = (typeof(self.scaleFactor) != "undefined" ? self.scaleFactor : 1);
      var r = s * self.NODERADIUS;
      var nodes = graph.nodes;
      var links = graph.links;
      var svg = d3.select("svg");
      
      window.graph.startForcebasedLayout(self.force, nodes, links, self.LINKDIST * s, self.forceTick.bind(self));
      var edge = window.graph.drawEdgeWithSimpleArrowHead(svg, links, self.NODERADIUS, s);
      var edgeLabel = window.graph.appendLabelToEdges(svg, links, "label");
      var node = window.graph.drawNodes(svg, nodes);
      window.graph.makeNodesCircular(node, r);
      window.graph.makeNodesDragable(node, self.force);
      window.graph.makeNodesClickable(node, self.nodeClicked);
      window.graph.appendLabelToNodes(node, "name");
      
      self.handles.edge = edge;
      self.handles.node = node;
      self.handles.edgeLabel = edgeLabel;
      self.handles.nodes = nodes;
    });
  },

  forceTick: function() {
    var self = this;
    var changedDistance = window.graph.tick.nodePosition(self.handles.node, self.handles.nodes, self.x_scale);
    window.graph.tick.edgePosition(self.handles.edge);
    window.graph.tick.edgeLabelPosition(self.handles.edgeLabel, "label");
    if (changedDistance < 0.5) {
      self.force.stop();
    }
  },

  nodeClicked: function(node) {
    console.log(node);
  },

  render: function() {
    $(this.el).html(this.template.text);
    var aqlEditor = ace.edit("aqlEditor");
    aqlEditor.getSession().setMode("ace/mode/javascript");
    aqlEditor.setTheme("ace/theme/merbivore_soft");
    aqlEditor.resize();
    
    $('#graphOutput').resizable({
      handles: "s",
      ghost: true,
      stop: function () {
        
      }
    });
    $('#aqlEditor').resizable({
      handles: "n, s",
      ghost: true,
      helper: "resizable-helper",
      stop: function () {
        var aqlEditor = ace.edit("aqlEditor");
        aqlEditor.resize();
      }
    });

    $('#aqlEditor .ace_text-input').focus();
    return this;
  }

});
