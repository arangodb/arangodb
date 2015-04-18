/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, arangoHelper, $, _, window, templateEngine, AmCharts*/

(function () {
  "use strict";

  window.DemoView = Backbone.View.extend({

    //MAP SPECIFIC VARIABLES
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
    MAPcolor: "black",

    keyToLongLat: {},

    //QUERIES SECTION
    queries: [
      {
        name: "i am just a dummy name",
        value: "here is the place of the actual query statement"
      }
    ],

    el: '#content',

    initialize: function () {
      this.airportCollection = new window.Airports();
      window.HASS = this;
    },

    events: {
      "change #flightQuerySelect" : "runSelectedQuery"
    },

    template: templateEngine.createTemplate("demoView.ejs"),

    render: function () {
      $(this.el).html(this.template.render({}));
      //TODO: this.renderMap([]);
      this.renderAvailableQueries();

      var callback = function() {

        var airport, airports = [];

        this.airportCollection.each(function(model) {
          airport = model.toJSON();
          airports.push({
            _key: airport._key,
            Latitude: airport.Latitude,
            Longitude: airport.Longitude
          });
        });
        var preparedData = this.prepareData(airports);
        this.renderMap(preparedData);
        // this.renderDummy();

      }.bind(this);

      this.airportCollection.getAirports(callback);

      return this;
    },

    renderAvailableQueries: function() {
      var position = 0;
      _.each(this.queries, function(query) {
        $('#flightQuerySelect').append('<option position="' + position + '">' + query.name + '<option>');
        position++;
      });
    },

    runSelectedQuery: function() {
      //TODO: RUN SELECTED QUERY IF USER SELECTS QUERY

      var currentQueryPos = $( "#flightQuerySelect option:selected" ).attr('position');
      var currentQuery = this.queries[parseInt(currentQueryPos)].value;

    },

    prepareData: function (data) {

      var self = this, imageData = [];

      _.each(data, function(airport) {
        imageData.push({
          id: airport._key,
          latitude: airport.Latitude,
          longitude: airport.Longitude,
          type: "circle", // CIRCLE INSTEAD OF ICON
          //svgPath: self.MAPplaneicon, //ICON if wanted
          color: self.MAPcolor,
          scale: 0.2, //ICON SCALE
          //TODO: LABEL TEMP. DISABLED BECAUSE OF READABILITY
          label: airport._key,
          labelShiftY: 2
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

    createFlightEntry: function(from, to, weight) {
      return {
        longitudes: [
          this.keyToLongLat[from].lon,
          this.keyToLongLat[to].lon,
        ],
        latitudes: [
          this.keyToLongLat[from].lat,
          this.keyToLongLat[to].lat
        ]
      };
    },

    renderMap: function(imageData) {

      var self = this;
      self.lines = [];

      AmCharts.theme = AmCharts.themes.light;
      self.map = AmCharts.makeChart("demo-mapdiv", {
        type: "map",
        theme: "light",
        pathToImages: "img/ammap/",
        dataProvider: {
          map: "worldLow",
          lines: self.lines,
          images: imageData,
          getAreasFromMap: true,
          zoomLevel: 2.25,
          zoomLatitude: 48.22,
          zoomLongitude: -100.00
        },
        areaSettings: {
          autoZoom: true,
          selectedColor: self.MAPcolor
        },
        linesSettings: {
          color: "#FF0000",
          alpha: 1
        },
        linesAboveImages: true
      });
    },

    addFlightLine: function(from, to) {
      this.lines.push(this.createFlightEntry(from, to));
      this.map.validateData();
    },

    renderDummy: function() {

      // svg path for target icon
      var targetSVG = (
        "M9,0C4.029,0,0,4.029,0,9s4.029,9,9,9s9-4.029,9-9S13.971,0,9,0z M9,"
        + "15.93 c-3.83,0-6.93-3.1-6.93-6.93S5.17,2.07,9,2.07s6.93,3.1,6.93,"
        + "6.93S12.83,15.93,9,15.93 M12.5,9c0,1.933-1.567,3.5-3.5,3.5S5.5,"
        + "10.933,5.5,9S7.067,5.5,9,5.5 S12.5,7.067,12.5,9z"
      );
      // svg path for plane icon
      var planeSVG = (
        "M19.671,8.11l-2.777,2.777l-3.837-0.861c0.362-0.505,0.916-1.683,"
        + "0.464-2.135c-0.518-0.517-1.979,0.278-2.305,0.604l-0.913,"
        + "0.913L7.614,8.804l-2.021,2.021l2.232,1.061l-0.082,0.082l1.701,"
        + "1.701l0.688-0.687l3.164,1.504L9.571,18.21H6.413l-1.137,1.138l3.6,"
        + "0.948l1.83,1.83l0.947,3.598l1.137-1.137V21.43l3.725-3.725l1.504,"
        + "3.164l-0.687,0.687l1.702,1.701l0.081-0.081l1.062,"
        + "2.231l2.02-2.02l-0.604-2.689l0.912-0.912c0.326-0.326,"
        + "1.121-1.789,0.604-2.306c-0.452-0.452-1.63,0.101-2.135,"
        + "0.464l-0.861-3.838l2.777-2.777c0.947-0.947,3.599-4.862,2.62-5.839C24.533,"
        + "4.512,20.618,7.163,19.671,8.11z"
      );

      var map = AmCharts.makeChart( "demo-mapdiv", {
        type: "map",
        "theme": "light",
        pathToImages: "http://www.amcharts.com/lib/3/images/",

        dataProvider: {
          map: "worldLow",
          linkToObject: "london",
          images: [ {
              id: "london",
              color: "#000000",
              svgPath: targetSVG,
              title: "London",
              latitude: 51.5002,
              longitude: -0.1262,
              scale: 1.5,
              zoomLevel: 2.74,
              zoomLongitude: -20.1341,
              zoomLatitude: 49.1712,

              lines: [ {
                latitudes: [ 51.5002, 50.4422 ],
                longitudes: [ -0.1262, 30.5367 ]
              }, {
                latitudes: [ 51.5002, 46.9480 ],
                longitudes: [ -0.1262, 7.4481 ]
              }, {
                latitudes: [ 51.5002, 59.3328 ],
                longitudes: [ -0.1262, 18.0645 ]
              }, {
                latitudes: [ 51.5002, 40.4167 ],
                longitudes: [ -0.1262, -3.7033 ]
              }, {
                latitudes: [ 51.5002, 46.0514 ],
                longitudes: [ -0.1262, 14.5060 ]
              }, {
                latitudes: [ 51.5002, 48.2116 ],
                longitudes: [ -0.1262, 17.1547 ]
              }, {
                latitudes: [ 51.5002, 44.8048 ],
                longitudes: [ -0.1262, 20.4781 ]
              }, {
                latitudes: [ 51.5002, 55.7558 ],
                longitudes: [ -0.1262, 37.6176 ]
              }, {
                latitudes: [ 51.5002, 38.7072 ],
                longitudes: [ -0.1262, -9.1355 ]
              }, {
                latitudes: [ 51.5002, 54.6896 ],
                longitudes: [ -0.1262, 25.2799 ]
              }, {
                latitudes: [ 51.5002, 64.1353 ],
                longitudes: [ -0.1262, -21.8952 ]
              }, {
                latitudes: [ 51.5002, 40.4300 ],
                longitudes: [ -0.1262, -74.0000 ]
              } ],

              images: [ {
                label: "Flights from London",
                svgPath: planeSVG,
                left: 100,
                top: 45,
                labelShiftY: 5,
                color: "#CC0000",
                labelColor: "#CC0000",
                labelRollOverColor: "#CC0000",
                labelFontSize: 20
              }, {
                label: "show flights from Vilnius",
                left: 106,
                top: 70,
                labelColor: "#000000",
                labelRollOverColor: "#CC0000",
                labelFontSize: 11,
                linkToObject: "vilnius"
              } ]
            },

            {
              id: "vilnius",
              color: "#000000",
              svgPath: targetSVG,
              title: "Vilnius",
              latitude: 54.6896,
              longitude: 25.2799,
              scale: 1.5,
              zoomLevel: 4.92,
              zoomLongitude: 15.4492,
              zoomLatitude: 50.2631,

              lines: [ {
                latitudes: [ 54.6896, 50.8371 ],
                longitudes: [ 25.2799, 4.3676 ]
              }, {
                latitudes: [ 54.6896, 59.9138 ],
                longitudes: [ 25.2799, 10.7387 ]
              }, {
                latitudes: [ 54.6896, 40.4167 ],
                longitudes: [ 25.2799, -3.7033 ]
              }, {
                latitudes: [ 54.6896, 50.0878 ],
                longitudes: [ 25.2799, 14.4205 ]
              }, {
                latitudes: [ 54.6896, 48.2116 ],
                longitudes: [ 25.2799, 17.1547 ]
              }, {
                latitudes: [ 54.6896, 44.8048 ],
                longitudes: [ 25.2799, 20.4781 ]
              }, {
                latitudes: [ 54.6896, 55.7558 ],
                longitudes: [ 25.2799, 37.6176 ]
              }, {
                latitudes: [ 54.6896, 37.9792 ],
                longitudes: [ 25.2799, 23.7166 ]
              }, {
                latitudes: [ 54.6896, 54.6896 ],
                longitudes: [ 25.2799, 25.2799 ]
              }, {
                latitudes: [ 54.6896, 51.5002 ],
                longitudes: [ 25.2799, -0.1262 ]
              }, {
                latitudes: [ 54.6896, 53.3441 ],
                longitudes: [ 25.2799, -6.2675 ]
              } ],

              images: [ {
                label: "Flights from Vilnius",
                svgPath: planeSVG,
                left: 100,
                top: 45,
                labelShiftY: 5,
                color: "#CC0000",
                labelColor: "#CC0000",
                labelRollOverColor: "#CC0000",
                labelFontSize: 20
              }, {
                label: "show flights from London",
                left: 106,
                top: 70,
                labelColor: "#000000",
                labelRollOverColor: "#CC0000",
                labelFontSize: 11,
                linkToObject: "london"
              } ]
            }, {
              svgPath: targetSVG,
              title: "Brussels",
              latitude: 50.8371,
              longitude: 4.3676
            }, {
              svgPath: targetSVG,
              title: "Prague",
              latitude: 50.0878,
              longitude: 14.4205
            }, {
              svgPath: targetSVG,
              title: "Athens",
              latitude: 37.9792,
              longitude: 23.7166
            }, {
              svgPath: targetSVG,
              title: "Reykjavik",
              latitude: 64.1353,
              longitude: -21.8952
            }, {
              svgPath: targetSVG,
              title: "Dublin",
              latitude: 53.3441,
              longitude: -6.2675
            }, {
              svgPath: targetSVG,
              title: "Oslo",
              latitude: 59.9138,
              longitude: 10.7387
            }, {
              svgPath: targetSVG,
              title: "Lisbon",
              latitude: 38.7072,
              longitude: -9.1355
            }, {
              svgPath: targetSVG,
              title: "Moscow",
              latitude: 55.7558,
              longitude: 37.6176
            }, {
              svgPath: targetSVG,
              title: "Belgrade",
              latitude: 44.8048,
              longitude: 20.4781
            }, {
              svgPath: targetSVG,
              title: "Bratislava",
              latitude: 48.2116,
              longitude: 17.1547
            }, {
              svgPath: targetSVG,
              title: "Ljubljana",
              latitude: 46.0514,
              longitude: 14.5060
            }, {
              svgPath: targetSVG,
              title: "Madrid",
              latitude: 40.4167,
              longitude: -3.7033
            }, {
              svgPath: targetSVG,
              title: "Stockholm",
              latitude: 59.3328,
              longitude: 18.0645
            }, {
              svgPath: targetSVG,
              title: "Bern",
              latitude: 46.9480,
              longitude: 7.4481
            }, {
              svgPath: targetSVG,
              title: "Kiev",
              latitude: 50.4422,
              longitude: 30.5367
            }, {
              svgPath: targetSVG,
              title: "Paris",
              latitude: 48.8567,
              longitude: 2.3510
            }, {
              svgPath: targetSVG,
              title: "New York",
              latitude: 40.43,
              longitude: -74
            }
          ]
        },

        areasSettings: {
          unlistedAreasColor: "#FFCC00"
        },

        imagesSettings: {
          color: "#CC0000",
          rollOverColor: "#CC0000",
          selectedColor: "#000000"
        },

        linesSettings: {
          color: "#CC0000",
          alpha: 0.4
        },


        backgroundZoomsToTop: true,
        linesAboveImages: true,

        "export": {
          "enabled": true,
          "libs": {
            "path": "ammapLibs"
          }
        }
      } );
          }

        });
      }());

