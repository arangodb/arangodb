/* global describe, it */

// ATM these tests are disabled for clusters because of the internal foxx-queue requests,
// which can cause test failures due to unexpected changes in the request counters.

'use strict';

const expect = require('chai').expect;
const internal = require('internal');
const arango = require('@arangodb').arango;

const METRIC_NAMES = {
  uptime: 'arangodb_server_statistics_server_uptime_total',
  totalRequests: 'arangodb_http_request_statistics_total_requests_total',
  httpGet: 'arangodb_http_request_statistics_http_get_requests_total',
  httpPost: 'arangodb_http_request_statistics_http_post_requests_total',
  httpDelete: 'arangodb_http_request_statistics_http_delete_requests_total',
};

function getMetrics() {
  const res = arango.GET_RAW('/_admin/metrics');
  expect(res.code).to.equal(200);
  const body = typeof res.body === 'string' ? res.body : String(res.body);
  return {
    uptime: internal.parsePrometheusMetric(body, METRIC_NAMES.uptime),
    totalRequests: internal.parsePrometheusMetric(body, METRIC_NAMES.totalRequests),
    httpGet: internal.parsePrometheusMetric(body, METRIC_NAMES.httpGet),
    httpPost: internal.parsePrometheusMetric(body, METRIC_NAMES.httpPost),
    httpDelete: internal.parsePrometheusMetric(body, METRIC_NAMES.httpDelete),
  };
}

function waitForMetricIncrease(getValue, expectedMin, maxWaitMs) {
  maxWaitMs = maxWaitMs || 10000;
  const stepMs = 250;
  const deadline = Date.now() + maxWaitMs;
  let value;
  while (Date.now() < deadline) {
    value = getValue();
    if (typeof value === 'number' && value >= expectedMin) {
      return value;
    }
    internal.sleep(stepMs / 1000);
  }
  return value;
}

function performGETRequest() {
  arango.GET('/_api/version');
}

function performPOSTRequest() {
  arango.POST('/_api/version', {'detail': true});
}

function performDELETERequest() {
  arango.DELETE("/_dummy");
}

describe('request statistics', function () {
  it('should expose uptime and request metrics via /_admin/metrics', function () {
    const metrics = getMetrics();
    expect(metrics.uptime).to.not.be.undefined;
    expect(metrics.uptime).to.be.at.least(0);
    expect(metrics.totalRequests).to.not.be.undefined;
    expect(metrics.httpGet).to.not.be.undefined;
  });

  it('should be updated by GET requests', function () {
    const initial = getMetrics();
    performGETRequest();
    const final = waitForMetricIncrease(
      () => getMetrics().httpGet,
      initial.httpGet + 2
    );
    expect(final, 'httpGet').to.be.at.least(initial.httpGet + 2);
    expect(getMetrics().totalRequests, 'totalRequests').to.be.at.least(initial.totalRequests + 2);
  });

  it('should be updated by POST requests', function () {
    const initial = getMetrics();
    performPOSTRequest();
    const final = waitForMetricIncrease(
      () => getMetrics().httpGet,
      initial.httpGet + 1
    );
    expect(final, 'httpGet').to.be.at.least(initial.httpGet + 1);
    expect(getMetrics().httpPost, 'httpPost').to.be.at.least(initial.httpPost + 1);
    expect(getMetrics().totalRequests, 'totalRequests').to.be.at.least(initial.totalRequests + 2);
  });

  it('should be updated by DELETE requests', function () {
    const initial = getMetrics();
    performDELETERequest();
    const final = waitForMetricIncrease(
      () => getMetrics().httpGet,
      initial.httpGet + 1
    );
    expect(final, 'httpGet').to.be.at.least(initial.httpGet + 1);
    expect(getMetrics().httpDelete, 'httpDelete').to.be.at.least(initial.httpDelete + 1);
    expect(getMetrics().totalRequests, 'totalRequests').to.be.at.least(initial.totalRequests + 2);
  });
});