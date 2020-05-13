/* global describe, it, afterEach */
'use strict';

const expect = require('chai').expect;
const internal = require('internal');
const arango = require('@arangodb').arango;
const _ = require("lodash");

function getStatistics() {
  return arango.GET("/_db/_system/_admin/statistics");
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

  const increaseByTwo = [ // the request for initialStats is also counted
    "client.totalTime.count",
    "client.requestTime.count",
    "client.queueTime.count",
    "client.ioTime.count",
    "client.bytesSent.count",
    "client.bytesReceived.count",
    "http.requestsTotal",
  ];

  for (const path of increaseByTwo) {
    expect(_.get(finalStats, path), path).to.be.equal(_.get(initialStats, path) + 2);
  }
}

describe('request statistics', function () {
  afterEach(function () {
    // need to wait a bit for pending statistic updates to be processed, so subsequent test
    // have a stable initialStats value
    internal.sleep(1);
  });

  it('should be updated by GET requests', function () {
    const initialStats = getStatistics();
    performGETRequest();
    internal.sleep(1); // need to wait a bit for the statistic updates to be processed
    const finalStats = getStatistics();

    checkCommonStatisticsChanges(initialStats, finalStats);

    const increaseByTwo = [
      "http.requestsGet", // the first statistics request is also counted
    ];
    for (const path of increaseByTwo) {
      expect(_.get(finalStats, path), path).to.be.equal(_.get(initialStats, path) + 2);
    }

    const equal = [
      "http.requestsAsync",
      "http.requestsHead",
      "http.requestsPost",
      "http.requestsPut",
      "http.requestsPatch",
      "http.requestsDelete",
      "http.requestsOptions",
      "http.requestsOther",
    ];
    for (const path of equal) {
      expect(_.get(finalStats, path), path).to.not.be.undefined;
      expect(_.get(finalStats, path), path).to.be.equal(_.get(initialStats, path));
    }

  });

  it('should be updated by POST requests', function () {
    const initialStats = getStatistics();
    performPOSTRequest();
    internal.sleep(1); // need to wait a bit for the statistic updates to be processed...
    const finalStats = getStatistics();

    checkCommonStatisticsChanges(initialStats, finalStats);

    const increaseByOne = [
      "http.requestsGet", // the first statistics request is also counted
      "http.requestsPost",
    ];
    for (const path of increaseByOne) {
      expect(_.get(finalStats, path), path).to.be.equal(_.get(initialStats, path) + 1);
    }

    const equal = [
      "http.requestsAsync",
      "http.requestsHead",
      "http.requestsPut",
      "http.requestsPatch",
      "http.requestsDelete",
      "http.requestsOptions",
      "http.requestsOther",
    ];
    for (const path of equal) {
      expect(_.get(finalStats, path), path).to.not.be.undefined;
      expect(_.get(finalStats, path), path).to.be.equal(_.get(initialStats, path));
    }
  });

  it('should be updated by DELETE requests', function () {
    const initialStats = getStatistics();
    performDELETERequest();
    internal.sleep(1); // need to wait a bit for the statistic updates to be processed...
    const finalStats = getStatistics();

    checkCommonStatisticsChanges(initialStats, finalStats);

    const increaseByOne = [
      "http.requestsGet", // the first statistics request is also counted
      "http.requestsDelete",
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
      "http.requestsOptions",
      "http.requestsOther",
    ];
    for (const path of equal) {
      expect(_.get(finalStats, path), path).to.not.be.undefined;
      expect(_.get(finalStats, path), path).to.be.equal(_.get(initialStats, path));
    }
  });
});
