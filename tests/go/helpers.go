package main

import (
	"context"
	"fmt"
	"os"
	"strconv"
	"strings"

	driver "github.com/arangodb/go-driver"
	driverhttp "github.com/arangodb/go-driver/http"

	"golang.org/x/net/http2"
)

const (
	ErrorNoEnvironment = 1
	ErrorNoConnection  = 2
	ErrorNoClient      = 3
)

func testGreeting(msg string) {
	fmt.Printf("==========================================================\n")
	fmt.Printf("Test starting: %s\n", msg)
	fmt.Printf("==========================================================\n\n")
}

type TestConfig struct {
	Endpoints []string
}

func configFromEnv() TestConfig {
	endpointstring, ok := os.LookupEnv("TEST_ENDPOINTS")
	if !ok {
		fmt.Printf("Did not find TEST_ENDPOINTS!\n")
		os.Exit(ErrorNoEnvironment)
	}
	return TestConfig{
		Endpoints: strings.Split(endpointstring, ","),
	}
}

func makeHttp1Client(config TestConfig) driver.Client {
	conn, err := driverhttp.NewConnection(driverhttp.ConnectionConfig{
		Endpoints: config.Endpoints,
	})
	if err != nil {
		fmt.Printf("Could not create client connection: %v\n", err)
		os.Exit(ErrorNoConnection)
	}
	client, err := driver.NewClient(driver.ClientConfig{
		Connection: conn,
	})
	if err != nil {
		fmt.Printf("Could not create client: %v\n", err)
		os.Exit(ErrorNoClient)
	}
	return client
}

func makeHttp2Client(config TestConfig) driver.Client {
	conn, err := driverhttp.NewConnection(driverhttp.ConnectionConfig{
		Endpoints: config.Endpoints,
		Transport: &http2.Transport{},
	})
	if err != nil {
		fmt.Printf("Could not create client connection: %v\n", err)
		os.Exit(ErrorNoConnection)
	}
	client, err := driver.NewClient(driver.ClientConfig{
		Connection: conn,
	})
	if err != nil {
		fmt.Printf("Could not create client: %v\n", err)
		os.Exit(ErrorNoClient)
	}
	return client
}

func assert(b bool, msg string) {
	if b {
		fmt.Printf("Assertion failure: %s\n", msg)
		os.Exit(100)
	}
}

func assertNil(err error, msg string) {
	if err != nil {
		fmt.Printf("Expected no error but got: %v, %s\n", err, msg)
		os.Exit(101)
	}
}

type Metrics struct {
	lines []string
}

func getMetrics(client driver.Client) (Metrics, error) {
	req, err := client.Connection().NewRequest("GET", "/_admin/metrics/v2")
	if err != nil {
		return Metrics{lines:[]string{}}, err
  }
	var rawResponse []byte
	ctx := driver.WithRawResponse(context.Background(), &rawResponse)
	_, _ = client.Connection().Do(ctx, req)
	// Will always return error, since the content type is not expected!
	lines := make([]string, 1200)
	prev := 0
	for i := 0; i < len(rawResponse); i++ {
		if rawResponse[i] == '\n' {
			lines = append(lines, string(rawResponse[prev:i]))
			prev = i+1
	  }
	}
	return Metrics{
		lines: lines,
  }, nil
}

func (m *Metrics) readIntMetric(metricName string) int64 {
	for i := 0; i < len(m.lines); i++ {
		if len(m.lines[i]) >= len(metricName) + 1 && 
		   m.lines[i][0:len(metricName)] == metricName {
			j, err := strconv.ParseInt(m.lines[i][len(metricName)+1:], 10, 64)
			if err != nil {
				return 0
			}
			return j
		}
	}
	return 0
}


