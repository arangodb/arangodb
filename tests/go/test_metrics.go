//////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Max Neunhoeffer
//////////////////////////////////////////////////////////////////////////////

package main

func main() {
	testGreeting("Protocol specific metrics move")
	config := configFromEnv()

	client := makeHttp1Client(config)
	m, err := getMetrics(client)
	assertNil(err, "could not retrieve metrics")
	http1ReqCount := m.readIntMetric("arangodb_request_body_size_http1_count")
	http2ReqCount := m.readIntMetric("arangodb_request_body_size_http2_count")
	vstReqCount := m.readIntMetric("arangodb_request_body_size_vst_count")

	// Do a few HTTP/1 requests:
	for i := 1; i <= 10; i++ {
		_, err := client.Version(nil)
		assertNil(err, "Could not retrieve version via HTTP/1")
	}

	m2, err := getMetrics(client)
	assertNil(err, "could not retrieve metrics 2")
	http1ReqCount2 := m2.readIntMetric("arangodb_request_body_size_http1_count")
	http2ReqCount2 := m2.readIntMetric("arangodb_request_body_size_http2_count")
	vstReqCount2 := m2.readIntMetric("arangodb_request_body_size_vst_count")

	assert(http1ReqCount2 > http1ReqCount, "http1 req count did not move")
	assert(http2ReqCount2 == http2ReqCount, "http2 req count did move")
	assert(vstReqCount2 == vstReqCount, "vst req count did move")
	http1ReqCount = http1ReqCount2

	// Do a few VST requests:
	client2 := makeVSTClient(config)
	for i := 1; i <= 10; i++ {
		_, err := client2.Version(nil)
		assertNil(err, "Could not retrieve version via VST")
	}

	m2, err = getMetrics(client)
	assertNil(err, "could not retrieve metrics 3")
	http1ReqCount2 = m2.readIntMetric("arangodb_request_body_size_http1_count")
	http2ReqCount2 = m2.readIntMetric("arangodb_request_body_size_http2_count")
	vstReqCount2 = m2.readIntMetric("arangodb_request_body_size_vst_count")

	assert(http1ReqCount2 > http1ReqCount, "http1 req count did not move")
	//the metrics calls are via HTTP/1, so it is expected to move
	assert(http2ReqCount2 == http2ReqCount, "http2 req count has moved")
	assert(vstReqCount2 >= vstReqCount+10, "vst req count did not move")
	http1ReqCount = http1ReqCount2
	vstReqCount = vstReqCount2

	// Do a few HTTP/2 requests:
	client3 := makeHttp2Client(config)
	for i := 1; i <= 10; i++ {
		_, err := client3.Version(nil)
		assertNil(err, "Could not retrieve version via HTTP/2")
	}

	m2, err = getMetrics(client)
	assertNil(err, "could not retrieve metrics 4")
	http1ReqCount2 = m2.readIntMetric("arangodb_request_body_size_http1_count")
	http2ReqCount2 = m2.readIntMetric("arangodb_request_body_size_http2_count")
	vstReqCount2 = m2.readIntMetric("arangodb_request_body_size_vst_count")

	assert(http1ReqCount2 > http1ReqCount, "http1 req count did not move")
	//the metrics calls are via HTTP/1, so it is expected to move
	assert(http2ReqCount2 >= http2ReqCount+10, "http2 req count did not move")
	assert(vstReqCount2 == vstReqCount, "vst req count has moved")
}
