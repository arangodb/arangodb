/* global describe, it, beforeEach */
'use strict';

const expect = require('chai').expect;
const internal = require('internal');
const arango = require('@arangodb').arango;
const _ = require("lodash");

function getStatistics() {
  return arango.GET("/_db/_system/_admin/statistics?dont-count-this-request=true");
}

function getStableStatistics() {
  // We need to make sure all pending statistic updates have been processed.
  // To this end, we perform an OPTIONS request (as these are rather rare) and
  // wait for max. 30sec that the requestsOptions counter has been increased.
  const initialRequestOptions = getStatistics().http.requestsOptions;
  arango.OPTIONS("/_dummy", "dummy");
  for (let i = 0; i < 60; ++i) {
    internal.sleep(0.5);
    const stats = getStatistics();
    if (stats.http.requestsOptions > initialRequestOptions) {
      return stats;
    }
  }
  throw "Failed to verify stable statistics counters - aborting";
}

function performGETRequest() {
  arango.GET('/_api/version');
}

function performPOSTRequest() {
  arango.POST("/_admin/execute", "return 'test!'");
}

function performDELETERequest() {
  arango.DELETE("/_dummy");
}

function checkCommonStatisticsChanges(initialStats, finalStats) {
  const greater = [
    "client.totalTime.sum",
    "client.requestTime.sum",
    "client.queueTime.sum",
    "client.ioTime.sum",
    "client.bytesSent.sum",
    "client.bytesReceived.sum",
  ];

  for (const path of greater) {
    expect(_.get(finalStats, path), path).to.be.above(_.get(initialStats, path));
  }

  const increaseByTwo = [ // the OPTIONS request for the sync indication is also counted
    "client.totalTime.count",
    "client.requestTime.count",
    "client.ioTime.count",
    "client.bytesSent.count",
    "client.bytesReceived.count",
    "http.requestsTotal",
  ];

  for (const path of increaseByTwo) {
    expect(_.get(finalStats, path), path).to.be.equal(_.get(initialStats, path) + 2);
  }

  // OPTIONS requests are not queue and therefore do not show up in queueTime
  const increaseByOne = [
    "client.queueTime.count",
  ];

  for (const path of increaseByOne) {
    expect(_.get(finalStats, path), path).to.be.equal(_.get(initialStats, path) + 1);
  }
}

describe('request statistics', function () {
  it('should be updated by GET requests', function () {
    const initialStats = getStableStatistics();
    performGETRequest();
    const finalStats = getStableStatistics();
    
    checkCommonStatisticsChanges(initialStats, finalStats);

    const increaseByOne = [
      "http.requestsGet",
      "http.requestsOptions", // this is the sync indicator
    ];
    for (const path of increaseByOne) {
      expect(_.get(finalStats, path), path).to.be.equal(_.get(initialStats, path) + 1);
    }

    const equal = [
      "http.requestsAsync",
      "http.requestsHead",
      "http.requestsPost",
      "http.requestsPut",
      "http.requestsPatch",
      "http.requestsDelete",
      "http.requestsOther",
    ];
    for (const path of equal) {
      expect(_.get(finalStats, path), path).to.not.be.undefined;
      expect(_.get(finalStats, path), path).to.be.equal(_.get(initialStats, path));
    }

  });

  it('should be updated by POST requests', function () {
    const initialStats = getStableStatistics();
    performPOSTRequest();
    const finalStats = getStableStatistics();

    checkCommonStatisticsChanges(initialStats, finalStats);

    const increaseByOne = [
      "http.requestsPost",
      "http.requestsOptions", // this is the sync indicator
    ];
    for (const path of increaseByOne) {
      expect(_.get(finalStats, path), path).to.be.equal(_.get(initialStats, path) + 1);
    }

    const equal = [
      "http.requestsAsync",
      "http.requestsGet",
      "http.requestsHead",
      "http.requestsPut",
      "http.requestsPatch",
      "http.requestsDelete",
      "http.requestsOther",
    ];
    for (const path of equal) {
      expect(_.get(finalStats, path), path).to.not.be.undefined;
      expect(_.get(finalStats, path), path).to.be.equal(_.get(initialStats, path));
    }
  });

  it('should be updated by DELETE requests', function () {
    const initialStats = getStableStatistics();
    performDELETERequest();
    const finalStats = getStableStatistics();

    checkCommonStatisticsChanges(initialStats, finalStats);

    const increaseByOne = [
      "http.requestsDelete",
      "http.requestsOptions", // this is the sync indicator
    ];
    for (const path of increaseByOne) {
      expect(_.get(finalStats, path), path).to.be.equal(_.get(initialStats, path) + 1);
    }

    const equal = [
      "http.requestsAsync",
      "http.requestsGet",
      "http.requestsHead",
      "http.requestsPost",
      "http.requestsPut",
      "http.requestsPatch",
      "http.requestsOther",
    ];
    for (const path of equal) {
      expect(_.get(finalStats, path), path).to.not.be.undefined;
      expect(_.get(finalStats, path), path).to.be.equal(_.get(initialStats, path));
    }
  });
});
