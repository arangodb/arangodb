/* global describe, it */

// ATM these tests are disabled for clusters because of the internal foxx-queue requests,
// which can cause test failures due to unexpected changes in the request counters.

'use strict';

const expect = require('chai').expect;
const internal = require('internal');
const arango = require('@arangodb').arango;

const _ = require('lodash');

const METRIC_NAMES = {
  uptime: 'arangodb_server_statistics_server_uptime_total',
  totalRequests: 'arangodb_http_request_statistics_total_requests_total',
  httpGet: 'arangodb_http_request_statistics_http_get_requests_total',
  httpPost: 'arangodb_http_request_statistics_http_post_requests_total',
  httpDelete: 'arangodb_http_request_statistics_http_delete_requests_total',
  httpHead: 'arangodb_http_request_statistics_http_head_requests_total',
  httpPut: 'arangodb_http_request_statistics_http_put_requests_total',
  httpPatch: 'arangodb_http_request_statistics_http_patch_requests_total',
  httpOptions: 'arangodb_http_request_statistics_http_options_requests_total',
  httpOther: 'arangodb_http_request_statistics_other_http_requests_total',
  httpAsync: 'arangodb_http_request_statistics_async_requests_total',
  clientTotalTimeSum: 'arangodb_client_connection_statistics_total_time_sum',
  clientTotalTimeCount: 'arangodb_client_connection_statistics_total_time_count',
  clientRequestTimeSum: 'arangodb_client_connection_statistics_request_time_sum',
  clientRequestTimeCount: 'arangodb_client_connection_statistics_request_time_count',
  clientQueueTimeSum: 'arangodb_client_connection_statistics_queue_time_sum',
  clientQueueTimeCount: 'arangodb_client_connection_statistics_queue_time_count',
  clientIoTimeSum: 'arangodb_client_connection_statistics_io_time_sum',
  clientIoTimeCount: 'arangodb_client_connection_statistics_io_time_count',
  clientBytesSentSum: 'arangodb_client_connection_statistics_bytes_sent_sum',
  clientBytesSentCount: 'arangodb_client_connection_statistics_bytes_sent_count',
  clientBytesReceivedSum: 'arangodb_client_connection_statistics_bytes_received_sum',
  clientBytesReceivedCount: 'arangodb_client_connection_statistics_bytes_received_count',
};

function parseMetric(body, name) {
  const v = internal.parsePrometheusMetric(body, name);
  return (typeof v === 'number' && !Number.isNaN(v)) ? v : 0;
}

function getMetrics() {
  const res = arango.GET_RAW('/_admin/metrics');
  expect(res.code).to.equal(200);
  const body = typeof res.body === 'string' ? res.body : String(res.body);
  return {
    uptime: parseMetric(body, METRIC_NAMES.uptime),
    totalRequests: parseMetric(body, METRIC_NAMES.totalRequests),
    httpGet: parseMetric(body, METRIC_NAMES.httpGet),
    httpPost: parseMetric(body, METRIC_NAMES.httpPost),
    httpDelete: parseMetric(body, METRIC_NAMES.httpDelete),
    httpHead: parseMetric(body, METRIC_NAMES.httpHead),
    httpPut: parseMetric(body, METRIC_NAMES.httpPut),
    httpPatch: parseMetric(body, METRIC_NAMES.httpPatch),
    httpOptions: parseMetric(body, METRIC_NAMES.httpOptions),
    httpOther: parseMetric(body, METRIC_NAMES.httpOther),
    httpAsync: parseMetric(body, METRIC_NAMES.httpAsync),
    clientTotalTimeSum: parseMetric(body, METRIC_NAMES.clientTotalTimeSum),
    clientTotalTimeCount: parseMetric(body, METRIC_NAMES.clientTotalTimeCount),
    clientRequestTimeSum: parseMetric(body, METRIC_NAMES.clientRequestTimeSum),
    clientRequestTimeCount: parseMetric(body, METRIC_NAMES.clientRequestTimeCount),
    clientQueueTimeSum: parseMetric(body, METRIC_NAMES.clientQueueTimeSum),
    clientQueueTimeCount: parseMetric(body, METRIC_NAMES.clientQueueTimeCount),
    clientIoTimeSum: parseMetric(body, METRIC_NAMES.clientIoTimeSum),
    clientIoTimeCount: parseMetric(body, METRIC_NAMES.clientIoTimeCount),
    clientBytesSentSum: parseMetric(body, METRIC_NAMES.clientBytesSentSum),
    clientBytesSentCount: parseMetric(body, METRIC_NAMES.clientBytesSentCount),
    clientBytesReceivedSum: parseMetric(body, METRIC_NAMES.clientBytesReceivedSum),
    clientBytesReceivedCount: parseMetric(body, METRIC_NAMES.clientBytesReceivedCount),
  };
}

function checkCommonStatisticsChanges(initialMetrics, finalMetrics) {
  const greater = [
    'clientTotalTimeSum',
    'clientRequestTimeSum',
    'clientQueueTimeSum',
    'clientIoTimeSum',
    'clientBytesSentSum',
    'clientBytesReceivedSum',
  ];
  for (const path of greater) {
    expect(_.get(finalMetrics, path), path).to.be.above(_.get(initialMetrics, path));
  }

  const increaseByAtLeastTwo = [
    'clientTotalTimeCount',
    'clientRequestTimeCount',
    'clientQueueTimeCount',
    'clientIoTimeCount',
    'clientBytesSentCount',
    'clientBytesReceivedCount',
    'totalRequests',
  ];
  for (const path of increaseByAtLeastTwo) {
    expect(_.get(finalMetrics, path), path).to.be.at.least(_.get(initialMetrics, path) + 2);
  }
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
  arango.POST('/_api/cursor', {query: 'RETURN 1'});
}

function performDELETERequest() {
  arango.DELETE('/_api/cursor/nonexistent-id');
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
    const initialStats = getMetrics();
    performGETRequest();
    const finalStats = waitForMetricIncrease(
      () => getMetrics().httpGet,
      initialStats.httpGet + 2
    );
    const final = getMetrics();
    expect(final.httpGet, 'httpGet').to.be.at.least(initialStats.httpGet + 2);

    checkCommonStatisticsChanges(initialStats, final);

    expect(final.httpGet, 'httpGet').to.be.at.least(initialStats.httpGet + 2);

    const equal = [
      'httpAsync',
      'httpHead',
      'httpPost',
      'httpPut',
      'httpPatch',
      'httpDelete',
      'httpOptions',
      'httpOther',
    ];
    for (const path of equal) {
      expect(_.get(final, path), path).to.not.be.undefined;
      expect(_.get(final, path), path).to.equal(_.get(initialStats, path));
    }
  });

  it('should be updated by POST requests', function () {
    const initialStats = getMetrics();
    performPOSTRequest();
    waitForMetricIncrease(
      () => getMetrics().httpPost,
      initialStats.httpPost + 1
    );
    const final = getMetrics();

    checkCommonStatisticsChanges(initialStats, final);

    expect(final.httpGet, 'httpGet').to.be.at.least(initialStats.httpGet + 1);
    expect(final.httpPost, 'httpPost').to.be.at.least(initialStats.httpPost + 1);

    const equal = [
      'httpAsync',
      'httpHead',
      'httpPut',
      'httpPatch',
      'httpDelete',
      'httpOptions',
      'httpOther',
    ];
    for (const path of equal) {
      expect(_.get(final, path), path).to.not.be.undefined;
      expect(_.get(final, path), path).to.equal(_.get(initialStats, path));
    }
  });

  it('should be updated by DELETE requests', function () {
    const initialStats = getMetrics();
    performDELETERequest();
    waitForMetricIncrease(
      () => getMetrics().httpDelete,
      initialStats.httpDelete + 1
    );
    const final = getMetrics();

    checkCommonStatisticsChanges(initialStats, final);

    expect(final.httpGet, 'httpGet').to.be.at.least(initialStats.httpGet + 1);
    expect(final.httpDelete, 'httpDelete').to.be.at.least(initialStats.httpDelete + 1);

    const equal = [
      'httpAsync',
      'httpHead',
      'httpPost',
      'httpPut',
      'httpPatch',
      'httpOptions',
      'httpOther',
    ];
    for (const path of equal) {
      expect(_.get(final, path), path).to.not.be.undefined;
      expect(_.get(final, path), path).to.equal(_.get(initialStats, path));
    }
  });
});