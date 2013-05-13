var dashboardView = Backbone.View.extend({
  el: '#content',
  updateInterval: 1000, // constant
  updateFrequency: 5,
  updateCounter: 0,
  arraySize: 100,
  clients: [],
  systems: [],
  convertedData: {},
  oldSum: {},

  initialize: function () {
    var self = this;

    this.collection.fetch({
      success: function() {
        self.renderCharts("system");
        self.renderCharts("client");

        window.setInterval(function() {
              self.updateCounter++;
              if (self.updateCounter < self.updateFrequency) {
                return false;
              }

              self.updateCounter = 0;

              self.collection.fetch({
                success: function() {
                  self.updateCharts("system");
                  self.updateCharts("client");
                },
                error: function() {
                  // need to flush previous values
                  self.oldSum = { };
                }
              });

        }, self.updateInterval);
      }
    });
  },

  events: {
    "click .dashboard-dropdown li" : "checkEnabled",
    "click .interval-dropdown li" : "checkInterval"
  },

  template: new EJS({url: 'js/templates/dashboardView.ejs'}),

  render: function() {
    var self = this;
    $(this.el).html(this.template.text);

    $.each(this.options.description.models[0].attributes.groups, function(key, val) {
      $('.thumbnails').append(
        '<ul class="statGroups" id="'+this.group+'">' +
        '<h4 class="statsHeader">'+this.name+'</h4>' +
        '</ul>');
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
          self.renderCharts("system");
          self.renderCharts("client");
        }
      });
    }
    else {
      self.renderCharts("system");
      self.renderCharts("client");
    }

    return this;
  },
  checkInterval: function (a) {
    var self = this;
    self.updateFrequency = a.target.value;

  },
  checkEnabled: function (a) {
    var myId = a.target.id;
    var position = myId.search('Checkbox');
    var preparedId = myId.substring(0, position);
    var toCheck = $(a.target).is(':checked');
    if (toCheck === false) {
      $("#"+preparedId).hide();
    }
    else if (toCheck === true) {
      $("#"+preparedId).show();
    }
  },

  renderCharts: function (type) {
    var self = this;
    var dataType;
    if (type === "system") {
      dataType = self.systems;
    }
    else if (type === "client") {
      dataType = self.clients;
    }

    this.generateClientData(type);
    var label;

    $.each(dataType, function(key, client) {
      var client = client;
      nv.addGraph(function() {
        var chart = nv.models.lineChart();

        chart.xAxis
        .axisLabel('')

        $.each(self.options.description.models[0].attributes.figures, function(k,v) {
          if (v.identifier === client) {
            if (v.units === 'bytes') {
              label = 'megabytes';
            }
            else {
              label = v.units;
            }
          }
        });

        chart.yAxis
        .axisLabel(label)


        // restrict to y-range 0...
        var max = 0;
        self.convertedData[client].map(function (value) { 
          if (value.y > max) {
            max = value.y;
          }
        });
        chart.yDomain([0, max]);

        d3.select("#"+client+"Chart svg")
        .datum(self.generateD3Input(self.convertedData[client], client, "#8AA051"))
        .transition().duration(500)
        .call(chart)

        nv.utils.windowResize(function() {
          d3.select('#'+client+'Chart svg').call(chart);
        });

        d3.selectAll('.nv-x text').text(function(d){
          if (isNaN(d)) {
            return d;
          }
          else {
            function pad (value) {
              return (value < 10 ? "0" + String(value) : String(value));
            }

            var date = new Date(d * 1000);
 
            return pad(date.getHours()) + ":" + pad(date.getMinutes()) + ":" + pad(date.getSeconds());
          }
        });

        return chart;
      });

    });
  },

  updateCharts: function (type) {
    var self = this;
    var dataType;
    if (type === "system") {
      dataType = self.systems;
    }
    else if (type === "client") {
      dataType = self.clients;
    }

    this.generateClientData(type);

    var label;

    $.each(dataType, function(key, client) {
      var client = client;
      var chart = nv.models.lineChart();

      chart.xAxis
      //.axisLabel("Time (ms)")
      .axisLabel("");

      $.each(self.options.description.models[0].attributes.figures, function(k,v) {
        if (v.identifier === client) {
            if (v.units === 'bytes') {
              label = 'megabytes';
            }
            else {
              label = v.units;
            }
        }
      });

      chart.yAxis
      .axisLabel(label);

      // restrict to y-range 0...
      var max = 0;
      self.convertedData[client].map(function (value) {
        if (value.y > max) {
          max = value.y;
        }
      });
      chart.yDomain([0, max]);

      d3.select("#"+client+"Chart svg")
      .datum(self.generateD3Input(self.convertedData[client], client, "#8AA051"))
      .transition().duration(500)
      .call(chart)

      d3.selectAll('.nv-x text').text(function(d){
        if (isNaN(d)) {
          return d;
        }
        else {
          function pad (value) {
            return (value < 10 ? "0" + String(value) : String(value));
          }

          var date = new Date(d * 1000);
          return pad(date.getHours()) + ":" + pad(date.getMinutes()) + ":" + pad(date.getSeconds());
        }
      });

    });


  },

  generateClientData: function (type) {
    var self = this;

    var dataType;
    if (type === "system") {
      dataType = this.collection.models[0].attributes.system;
    }
    else if (type === "client") {
      dataType = this.collection.models[0].attributes.client;
    }

    var x = []; //Time
    var y = []; //Value
    var tempName;
    var tempArray = [];
    $.each(dataType, function(key, val) {
      tempName = key;
      if (self.convertedData[tempName] === undefined) {
        self.convertedData[tempName] = [];
      }

      if (self.convertedData[tempName].length === 0) {
        var oldValue;
        if (val.sum === undefined) {
          oldValue = val;
        }
        else {
          oldValue = val.sum;
        }
        if (oldValue < 0) {
          oldValue = 0;
        }

        self.oldSum[tempName] = oldValue;
        tempArray = self.convertedData[tempName];
        var timeStamp = Math.round(+new Date()/1000);
        tempArray.push({x:timeStamp, y:0});
      }
      if (self.convertedData[tempName].length !== 0)  {
        tempArray = self.convertedData[tempName];

        var timeStamp = Math.round(+new Date()/1000);

        var myType;
        $.each(self.options.description.models[0].attributes.figures, function(k,v) {
          if (v.identifier === tempName) {
            myType = v.type;
            if (v.units === 'bytes') {
              if (val.sum === undefined) {
                val = val / 1024 / 1024;
                val = Math.round(val * 100) / 100;
                if (val < 0) {
                  val = 0;
                }
              }
              else {
                val.sum = val.sum / 1024 / 1024;
                val.sum = Math.round(val.sum * 100) / 100;

                if (val.sum < 0) {
                  val.sum = 0;
                }
              }
            }
          }
        });

        if (myType === "current") {
          tempArray.push({x:timeStamp, y:val});
          return;
        }
        else {
          var calcValue = (val.sum === undefined ? val : val.sum);
          if (calcValue < 0) {
            calcValue = 0;
          }

          var calculatedSum = calcValue - self.oldSum[tempName];
          //Round value
          calculatedSum = Math.round(calculatedSum * 100) / 100;
          if (calculatedSum < 0) {
            calculatedSum = 0;
          }
          tempArray.push({x:timeStamp, y:calculatedSum});
          self.oldSum[tempName] = calcValue;
        }
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
      '<li class="statClient" id="'+identifier+'">' +
      '<div class="boxHeader"><h6 class="dashboardH6">' + name + '</h6></div>' +
      '<div class="statChart" id="'+identifier+'Chart"><svg class="svgClass"/></div>' +
      '</li>'
    );
    $('#lastDashDivider').before(
      '<li><a><label class="checkbox">'+
      '<input type="checkbox" id='+identifier+'Checkbox checked>'+name+'</label></a></li>'
    );
  },
  renderSystem: function (identifier, name, desc, type, units) {
    $('#system').append(
      '<li class="statClient" id="'+identifier+'">' +
      '<div class="boxHeader"><h6 class="dashboardH6">' + name + '</h6></div>' +
      '<div class="statChart" id="'+identifier+'Chart"><svg class="svgClass"/></div>' +
      '</li>'
    );
    $('#firstDashDivider').before(
      '<li><a><label class="checkbox">'+
      '<input type="checkbox" id='+identifier+'Checkbox checked>'+name+'</label></a></li>'
    );
  },
  /*
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
*/
updateClient: function (identifier, count, counts) {

}

});
