var dashboardView = Backbone.View.extend({
  el: '#content',
  updateInterval: 6000,
  clients: [],
  systems: [],
  convertedData: {},
  oldSum: {},

  initialize: function () {
    var self = this;

    this.collection.fetch({
      success: function() {
        self.renderCharts();
        window.setInterval(function() {
          self.collection.fetch({
            success: function() {
              self.updateSystems();
              self.updateCharts();
            }
          });
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
    if (this.collection.models[0] === undefined) {
      this.collection.fetch({
        success: function() {
          self.renderCharts();
        }
      });
    }
    else {
      self.renderCharts();
    }

    return this;
  },
  renderCharts: function () {
    var self = this;
    this.generateClientData();
    var label;

    $.each(self.clients, function(key, client) {
      var client = client;
      nv.addGraph(function() {
        var chart = nv.models.lineChart();

        chart.xAxis
        .axisLabel('Time')

        $.each(self.options.description.models[0].attributes.figures, function(k,v) {
          if (v.identifier === client) {
            label = v.units;
          }
        });

        chart.yAxis
        .axisLabel(label)

        d3.select("#"+client+"Chart svg")
        .datum(self.generateD3Input(self.convertedData[client], client, "#8AA051"))
        .transition().duration(500)
        .call(chart)

        nv.utils.windowResize(function() {
          d3.select('#httpConnectionsChart svg').call(chart);
        });

        return chart;
      });

    });
  },

  updateCharts: function () {
    var self = this;
    this.generateClientData();

    var label;

    $.each(self.clients, function(key, client) {
      var client = client;
      var chart = nv.models.lineChart();

      chart.xAxis
      .axisLabel("Time (ms)")
      $.each(self.options.description.models[0].attributes.figures, function(k,v) {
        if (v.identifier === client) {
          label = v.units;
        }
      });
      chart.yAxis
      .axisLabel(label)

      d3.select("#"+client+"Chart svg")
      .datum(self.generateD3Input(self.convertedData[client], client, "#8AA051"))
      .transition().duration(500)
      .call(chart)

      d3.selectAll('.nv-x text').text(function(d){
        if (isNaN(d)) {
          return d;
        }
        else {
          var date = new Date(d*1000);
          var hours = date.getHours();
          var minutes = date.getMinutes();
          var seconds = date.getSeconds();
          return hours+":"+minutes+":"+seconds;
        }
      });

    });


  },

  generateClientData: function () {
    var self = this;
    var x = []; //Time
    var y = []; //Value
    var tempName;
    var tempArray = [];
    $.each(this.collection.models[0].attributes.client, function(key, val) {
      tempName = key;
      if (self.convertedData[tempName] === undefined) {
        self.convertedData[tempName] = [];
      }
      if (self.convertedData[tempName].length === 0) {
        self.oldSum[tempName] = val.sum;
        tempArray = self.convertedData[tempName];
        var timeStamp = Math.round(+new Date()/1000);
        tempArray.push({x:timeStamp, y:0});
      }
      if (self.convertedData[tempName].length !== 0)  {
        tempArray = self.convertedData[tempName];

        var timeStamp = Math.round(+new Date()/1000);

        if (val.sum === undefined) {
          tempArray.push({x:timeStamp, y:val});
          return;
        }

        var calculatedSum = val.sum - self.oldSum[tempName];
        self.oldSum[tempName] = val.sum;
        tempArray.push({x:timeStamp, y:calculatedSum});
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
