var dashboardView = Backbone.View.extend({
  el: '#content',
  updateInterval: 3000,

  initialize: function () {
    //notes
    //this.collection.fetch();
    //this.options.description.fetch();
    var self = this;

    this.collection.fetch({
      success: function() {
        window.setInterval(function() {
          self.updateSystems();
          self.collection.fetch();
        }, self.updateInterval);
      }
    });


  },

  template: new EJS({url: 'js/templates/dashboardView.ejs'}),

  render: function() {
    var self = this;
    $(this.el).html(this.template.text);

    $.each(this.options.description.models[0].attributes.groups, function(key, val) {
      $('#content').append(
        '<div class="statGroups" id="'+this.group+'">' +
          '<h4>'+this.name+'</h4>' +
        '</div>');
    });
    $.each(this.options.description.models[0].attributes.figures, function(key, val) {
      if (this.group === 'system') {
        self.renderSystem(this.identifier, this.name, this.description, this.type, this.units);
      }
      else if (this.group === 'client') {
        self.renderClient(this.identifier, this.name, this.description, this.type, this.units);
      }
    });

    this.renderCharts();
    return this;
  },
  renderCharts: function () {
    var self = this;
    nv.addGraph(function() {
      var chart = nv.models.lineWithFocusChart();

      chart.xAxis
        .axisLabel('Time (ms)')
        .tickFormat(d3.format(',r'));
      chart.yAxis
        .axisLabel('Voltage (v)')
        .tickFormat(d3.format('.02f'));

      d3.select("#httpConnectionsChart svg")
      .datum(self.sinAndCos())
      .transition().duration(500)
      .call(chart)

      nv.utils.windowResize(function() { d3.select('#httpConnectionsChart svg').call(chart) });

      return chart;
    });
  },

    sinAndCos: function () {
      var sin = [],
    cos = [];

    for (var i = 0; i < 100; i++) {
      sin.push({x: i, y: Math.sin(i/10)});
      cos.push({x: i, y: .5 * Math.cos(i/10)});
    }

    return [
      {
      values: sin,
      key: 'Sine Wave',
      color: '#ff7f0e'
    },
    {
      values: cos,
      key: 'Cosine Wave',
      color: '#2ca02c'
    }
    ];
  },

  renderClient: function (identifier, name, desc, type, units) {
    $('#client').append(
      '<div class="statClient" id="'+identifier+'">' +
        '<h5>' + name + '</h5>' +
        '<div class="statChart" id="'+identifier+'Chart"><svg class="svgClass"/></div>' +
      '</div>'
    );
  },
  renderSystem: function (identifier, name, desc, type, units) {
    $('#system').append(
      '<div class="statSystem" id="'+identifier+'">' +
      '<table><tr>' +
      '<th>'+ name +'</th>' +
      '<th class="updateValue">'+ 'counting...' +'</th>' +
      '</tr></table>' +
      '</div>'
    );
  },
  updateClient: function (identifier, count, counts) {

  },
  updateSystems: function () {
    $.each(this.collection.models[0].attributes.system, function(key, val) {
      $('#'+key+' .updateValue').html(val);
    });

  }

});
