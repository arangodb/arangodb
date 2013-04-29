var dashboardView = Backbone.View.extend({
  el: '#content',
  updateInterval: 3000,
  clients: [],
  systems: [],
  convertedData: {},

  initialize: function () {
    //notes
    //this.collection.fetch();
    //this.options.description.fetch();
    var self = this;

    this.collection.fetch({
      success: function() {
        self.renderCharts();
        window.setInterval(function() {
          self.collection.fetch({success: function() {self.updateSystems()}});
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
        self.systems.push(this.identifier);
      }
      else if (this.group === 'client') {
        self.renderClient(this.identifier, this.name, this.description, this.type, this.units);
        self.clients.push(this.identifier);
      }
    });

    return this;
  },
  renderCharts: function () {
    var self = this;
    this.generateClientData();


    $.each(self.clients, function(key, client) {
      var client = client;
    var test1 = self.sinAndCos();
    var test2 = self.generateD3Input(self.convertedData[client], client, "#ff7f0e");
 //console.log(test1);
 console.log(test2);
      nv.addGraph(function() {
        var chart = nv.models.lineChart();

        chart.xAxis
          .axisLabel('Time (ms)')
          .tickFormat(d3.format(',r'));
        chart.yAxis
          .axisLabel('Voltage (v)')
          .tickFormat(d3.format('.02f'));

        d3.select("#"+client+"Chart svg")
          .datum(self.generateD3Input(self.convertedData[client], client, "#ff7f0e"))
          //.datum(self.sinAndCos())
          .transition().duration(500)
          .call(chart)

        nv.utils.windowResize(function() { d3.select('#httpConnectionsChart svg').call(chart) });

        return chart;
      });

    });
  },

  generateClientData: function () {
    var self = this;
    var x = []; //Value
    var y = []; //Time
    var tempName;
    var tempArray = [];
    //FOR TESTING
    var i = 0;
    $.each(this.collection.models[0].attributes.client, function(key, val) {
      tempName = key;
      if (val.counts === undefined) {
        console.log("undefined");
      }
      else {
        tempArray = [];
        $.each(val.counts, function(k,v) {
          tempArray.push({x:i, y:v});
          self.convertedData[tempName] = tempArray;
          i++;
        });
      }
    });
  },

  generateD3Input: function (values, key, color) {
    if (values === undefined) {
      return;
    }
    return [
      {
        values: values,
        key: key,
        color: color
      }
    ]
  },

  sinAndCos: function () {
    var sin = [],
    cos = [];

    for (var i = 0; i < 50; i++) {
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
