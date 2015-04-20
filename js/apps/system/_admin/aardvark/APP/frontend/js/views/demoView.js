/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, arangoHelper, $, _, window, templateEngine, AmCharts, lunr*/

(function () {
  "use strict";

  window.DemoView = Backbone.View.extend({

    //MAP SPECIFIC VARIABLES
    // svg path for target icon
    MAPtarget: (
      "M9,0C4.029,0,0,4.029,0,9s4.029,9,9,9s9-4.029,9-9S13.971,0,9,0z M9,"
      + "15.93 c-3.83,0-6.93-3.1-6.93-6.93S5.17,2.07,9,2.07s6.93,3.1,6.93,"
      + "6.93S12.83,15.93,9,15.93 M12.5,9c0,1.933-1.567,3.5-3.5,3.5S5.5,"
      + "10.933,5.5,9S7.067,5.5,9,5.5 S12.5,7.067,12.5,9z"
    ),
    /*
    MAPicon: (
      "M23.963,20.834L17.5,9.64c-0.825-1.429-2.175-1.429-3,0L8.037,20.834c-0.825,"
      + "1.429-0.15,2.598,1.5,2.598h12.926C24.113,23.432,24.788,22.263,23.963,20.834z"
    ),
    MAPplaneicon: (
      "M19.671,8.11l-2.777,2.777l-3.837-0.861c0.362-0.505,0.916-1.683,"
      + "0.464-2.135c-0.518-0.517-1.979,0.278-2.305,0.604l-0.913,0.913L7.614,8.804l-2.021,"
      + "2.021l2.232,1.061l-0.082,0.082l1.701,1.701l0.688-0.687l3.164,1.504L9.571,"
      + "18.21H6.413l-1.137,1.138l3.6,0.948l1.83,1.83l0.947,"
      + "3.598l1.137-1.137V21.43l3.725-3.725l1.504,3.164l-0.687,0.687l1.702,"
      + "1.701l0.081-0.081l1.062,2.231l2.02-2.02l-0.604-2.689l0.912-0.912c0.326-0.326,"
      + "1.121-1.789,0.604-2.306c-0.452-0.452-1.63,0.101-2.135,"
      + "0.464l-0.861-3.838l2.777-2.777c0.947-0.947,3.599-4.862,2.62-5.839C24.533,"
      + "4.512,20.618,7.163,19.671,8.11z"
    ),
    //ICON FROM http://raphaeljs.com/icons/#plane
    */

    lineColors: [
      'rgb(255,255,229)',
      'rgb(255,247,188)',
      'rgb(254,227,145)',
      'rgb(254,196,79)',
      'rgb(254,153,41)',
      'rgb(236,112,20)',
      'rgb(204,76,2)',
      'rgb(153,52,4)',
      'rgb(102,37,6)'
    ],

    airportColor: "#222222",
    airportHighlightColor: "#FF4E4E",
    airportHoverColor: "#ff8f35",

    airportScale: 0.5,
    airportHighligthScale: 0.95,

    imageData: [],

    keyToLongLat: {}, 

    //QUERIES SECTION    
    queries: [
      {
        name: "All Flights from SFO"
      },
      {
        name: "All Flights from JFK"
      },
      {
        name: "All Flights from DFW"
      },
      {
        name: "All Flights from ATL"
      },
      {
        name: "All Flights from CWA"
      }
    ],

    el: '#content',

    initialize: function () {
      this.airportCollection = new window.Airports();
      window.HASS = this; // <--- BITTE ?!? ;) <-- zu viel Kontakt mit Jan
    },

    events: {
      "change #flightQuerySelect" : "runSelectedQuery",
      "keyup #demoSearchInput"   : "searchInput"
    },

    template: templateEngine.createTemplate("demoView.ejs"),

    generateIndex: function () {
      var airport, self = this;

      self.index = lunr(function () {
        this.field('Name', { boost: 10 }); //strongest index field
        this.field('City');
        this.field('_key');
      });

      this.airportCollection.each(function(model) {
        airport = model.toJSON();

        self.index.add({
          Name: airport.Name,
          City: airport.City,
          _key: airport._key,
          id: airport._key
        });
      });

    },

    render: function () {

      var self = this;

      $(this.el).html(this.template.render({}));
      this.renderAvailableQueries();

      var callback = function() {

        var airport, airports = [];

        this.airportCollection.each(function(model) {
          airport = model.toJSON();
          airports.push(airport);
        });
        this.imageData = this.prepareData(airports);
        this.renderMap();

        this.generateIndex();

      }.bind(this);

      this.airportCollection.getAirports(callback);

      return this;
    },

    renderAvailableQueries: function() {
      var position = 0;
      _.each(this.queries, function(query) {
        $('#flightQuerySelect').append('<option position="' + position + '">' + query.name + '</option>');
        position++;
      });
    },

    searchInput: function(e) {

      var self = this, airports = this.index.search($(e.currentTarget).val());

      self.resetDataHighlighting();
      self.removeFlightLines(false);

      _.each(airports, function(airport) {
        self.setAirportColor(airport.ref, self.airportHighlightColor, false);
        self.setAirportSize(airport.ref, self.airportHighligthScale, false);
      });

      if (airports.length === 1) {
        //TODO: maybe zoom to airport if found?
        //self.zoomToAirport(airports[0].ref);
        self.showAirportBalloon(airports[0].ref); // <-- only works with single map object, not multiple :(
      }

      //if (airports.length <= 10) {
      //}

      self.map.validateData();


    },

    runSelectedQuery: function() {
      var currentQueryPos = $( "#flightQuerySelect option:selected" ).attr('position');
      if (currentQueryPos === "0") {
        this.loadAirportData("SFO");
      }
      else if (currentQueryPos === "1") {
        this.loadAirportData("JFK");
      }
      else if (currentQueryPos === "2") {
        this.loadAirportData("DFW");
      }
      if (currentQueryPos === "3") {
        this.loadAirportData("ATL");
      }
      if (currentQueryPos === "4") {
        this.loadAirportData("CWA");
      }
    },

    loadAirportData: function(airport) {
      var self = this;
      var timer = new Date();

      var airportData = this.airportCollection.findWhere({_key: airport});
      this.airportCollection.getFlightsForAirport(airport, function(list) {

        var timeTaken = new Date() - timer;

        self.removeFlightLines(false);

        var allFlights = 0;

        var i = 0;

        self.resetDataHighlighting();

        for (i = 0; i < list.length; ++i) {
          self.addFlightLine(
            airport,
            list[i].Dest,
            list[i].count,
            self.calculateFlightColor(list.length, i),
            self.calculateFlightWidth(list.length, i)
          );
          allFlights += list[i].count;
        }
        
        if ($("#demo-mapdiv-info").length === 0) {
          $("#demo-mapdiv").append("<div id='demo-mapdiv-info'></div>");
        }
        
        var tempHTML = "";
        tempHTML = "<b>" + airportData.get("Name") + "</b> - " + airport + "<br>" + 
          "Query needed: <b>" + (timeTaken/1000) + "sec" + "</b><br>" +
          "Number destinations: <b>" + list.length + "</b><br>" + 
          "Number flights: <b>" + allFlights + "</b><br>" +
          "Top 5:<br>";

        for (i = (list.length - 1); i > Math.max(list.length - 6, 0); --i) {
          airportData = self.airportCollection.findWhere({_key: list[i].Dest});
          tempHTML += airportData.get("Name") + " - " + airportData.get("_key") + ": <b>" + list[i].count + "</b>";
          if (i > (list.length - 5)) {
            tempHTML += "<br>";
          }
        }

        $("#demo-mapdiv-info").html(tempHTML);

        self.map.validateData();
      });
    },

    calculateFlightWidth: function(length, pos) {
      //var intervallWidth = length/2;
      // return Math.floor(pos/intervallWidth) + 2;
      //TODO: no custom width for lines wanted?
      return 1.5;
    },

    calculateFlightColor: function(length, pos) {
      var intervallColor = length/this.lineColors.length;
      return this.lineColors[Math.floor(pos/intervallColor)];
    },

    zoomToAirport: function (id) {
      this.map.zoomToSelectedObject(this.map.getObjectById(id));
    },

    showAirportBalloon: function (id) {
      var mapObject = this.map.getObjectById(id);
      this.map.rollOverMapObject(mapObject);
    },

    hideAirportBalloon: function (id) {
      var mapObject = this.map.getObjectById(id);
      this.map.rollOutMapObject(mapObject);
    },

    //Color = HEXCODE e.g. #FFFFFF
    setAirportColor: function(id, color, shouldRender) {

      _.each(this.imageData, function(airport) {
        if (airport.id === id) {
          airport.color = color;
        }
      });

      if (shouldRender) {
        this.map.validateData();
      }
    },

    //size = numeric value e.g. 0.5 or 1.3
    setAirportSize: function(id, size, shouldRender) {

      _.each(this.imageData, function(airport) {
        if (airport.id === id) {
          airport.scale = size;
        }
      });

      if (shouldRender) {
        this.map.validateData();
      }
    },

    resetDataHighlighting: function() {

      var self = this;

      _.each(this.imageData, function(airport) {
          airport.color = self.airportColor;
          airport.scale = self.airportScale;
      });
    },

    prepareData: function (data) {

      var self = this, imageData = [];

      _.each(data, function(airport) {
        imageData.push({
          id: airport._key,
          latitude: airport.Latitude,
          longitude: airport.Longitude,
          svgPath: self.MAPtarget,
          color: self.airportColor,
          scale: self.airportScale,
          selectedScale: 2.5,
          title: airport.City + " [" + airport._key + "]<br>" + airport.Name,
          rollOverColor: self.airportHoverColor,
          selectable: true
        });
        self.keyToLongLat[airport._key] = {
          lon: airport.Longitude,
          lat: airport.Latitude
        };
      });
      imageData.push({
        color: "#FF0000",
        lines: [{
          latitudes: [ 51.5002, 50.4422 ],
          longitudes: [ -0.1262, 30.5367 ]
        }]
      });
      return imageData;
    },

    createFlightEntry: function(from, to, weight, lineColor, lineWidth) {
      return {
        longitudes: [
          this.keyToLongLat[from].lon,
          this.keyToLongLat[to].lon,
        ],
        latitudes: [
          this.keyToLongLat[from].lat,
          this.keyToLongLat[to].lat
        ],
        title: from + " - " + to + "<br>" + weight,
        color: lineColor,
        thickness: lineWidth
      };
    },

    renderMap: function() {

      var self = this;
      self.lines = [];

      AmCharts.theme = AmCharts.themes.light;
      self.map = AmCharts.makeChart("demo-mapdiv", {
        type: "map",
        showDescriptionOnHover: false,
        dragMap: true,
        creditsPosition: "bottom-left",
        pathToImages: "img/ammap/",
        dataProvider: {
          map: "usa2High",
          lines: self.lines,
          images: self.imageData,
          getAreasFromMap: true
        },
        clickMapObject: function(mapObject) {
          console.log(mapObject);
          self.loadAirportData(mapObject.id);
        },
        balloon: {
          adjustBorderColor: true,
          balloonColor: "#ffffff",
          color: "#000000",
          cornerRadius: 5,
          fillColor: "#ffffff",
          fillAlpha: 0.75,
          borderThickness: 1.5,
          borderColor: "#88A049",
          borderAlpha: 0.4,
          shadowAlpha: 0,
          fontSize: 10,
          verticalPadding: 3,
          horizontalPadding: 6
        },
        areasSettings: {
          autoZoom: false,
          balloonText: ""
        },
        linesSettings: {
          color: "#ff8f35", // 7629C4
          alpha: 0.75,
          thickness: 2
        },
        linesAboveImages: false,
      });
    },

    removeFlightLines: function(shouldRender) {
      this.lines.length = 0;

      if (shouldRender) {
        this.map.validateData();
      }
    },

    addFlightLines: function(lines) {
      _.each(lines, function(line) {
        this.addFlightLine(line.from, line.to, line.count, line.lineColor, line.lineWidth, false);
      });
    },

    addFlightLine: function(from, to, count, lineColor, lineWidth, shouldRender) {
      console.log("Adding", from, to, count, lineColor, lineWidth);
      this.lines.push(this.createFlightEntry(from, to, count, lineColor, lineWidth));

      this.setAirportColor(from, "#FFFFFF");
      this.setAirportColor(to, this.airportHighlightColor);

      this.setAirportSize(from, 1.5);
      this.setAirportSize(to, this.airportHighligthScale);
    }

  });
}());
