/*jshint browser: true */
/*jshint unused: false */
/*global window, Backbone, $, _ */
(function () {

  "use strict";

  window.Airports = Backbone.Collection.extend({

    initialize: function(options) {
      this.collectionName = options.collectionName;
    },

    getAirports: function(callback) {
      var self = this;

      $.ajax({
        type: "POST",
        url: "/_api/cursor",
        data: JSON.stringify({
          query: "for a in airports return {Latitude: a.Latitude, " +
                 "Longitude: a.Longitude, Name: a.Name, City: a.City, _key: a._key}"
        }),
        contentType: "application/json",
        processData: false,
        success: function (data) {
          _.each(data.result, function(airport) {
            self.add(airport);
          });
          if (callback) {
            callback();
          }
        },
        error: function (data) {
        }
      });
    },

    getShortestFlight: function(from, to, callback) {
      $.ajax({
        type: "POST",
        url: "/_api/cursor",
        data: JSON.stringify({
          query: "RETURN SHORTEST_PATH(@@airports,@@flights,@start,@dest,'outbound',{})",
          bindVars: {
            "@flights": this.collectionName,
            "@airports": "airports",
            "start": "airports/" + from,
            "dest": "airports/" + to
          }
        }),
        contentType: "application/json",
        processData: false,
        success: function (data) {
          callback(data.result[0]);
        },
        error: function (data) {
        }
      });
    },

    getFlightDistribution: function(callback) {
      $.ajax({
        type: "POST",
        url: "/_api/cursor",
        data: JSON.stringify({
          query: "FOR f IN @@flights COLLECT dest = f._to " + 
            "WITH COUNT INTO n SORT n RETURN {Dest: SPLIT(dest, '/')[1], count: n}",
          bindVars: {
            "@flights": this.collectionName
          }
        }),
        contentType: "application/json",
        processData: false,
        success: function (data) {
          callback(data.result);
        },
        error: function (data) {
        }
      });
    },

    getFlightsForAirport: function(airport, callback) {
      var self = this;

      $.ajax({
        type: "POST",
        url: "/_api/cursor",
        data: JSON.stringify({
          query: "for f in @@flights filter f._from == @airport COLLECT dest = f._to " + 
            "WITH COUNT INTO n SORT n RETURN {Dest: SPLIT(dest, '/')[1], count: n}",
          bindVars: {
            "airport": "airports/" + airport,
            "@flights": this.collectionName
          }
        }),
        contentType: "application/json",
        processData: false,
        success: function (data) {
          callback(data.result);
        },
        error: function (data) {
        }
      });
    },

    model: window.Airport

  });
}());
