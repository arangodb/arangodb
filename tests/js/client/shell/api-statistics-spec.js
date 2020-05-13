/* global describe, it, beforeEach */
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

  // Initial statistics counter - this is initialized as part of
  // beforeEach when we try to establish that statistics have stabilized.
  let initialStats;

  beforeEach(function () {
    // We need to make sure all pending statistic updates have been processed,
    // so the tests have a stable initialStats value. To this end, we perform an
    // OPTIONS request (as these are rather rare) and wait for max. 30sec that
    // the requestsOptions counter has been increased.
    const initialRequestOptions = getStatistics().http.requestsOptions;
    arango.OPTIONS("/_dummy", "dummy");
    for (let i = 0; i < 60; ++i) {
      internal.sleep(0.5);
      const stats = getStatistics();
      if (stats.http.requestsOptions > initialRequestOptions) {
        initialStats = stats;
        return;
      }
    }
    throw "Failed to verify stable statistics counters - aborting";
  });

  it('should be updated by GET requests', function () {
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
