package main

import (
	"fmt"
	"os"
	"strings"
	driver "github.com/arangodb/go-driver"
	"github.com/arangodb/go-driver/http"
)

const (
	ErrorNoEnvironment = 1
	ErrorNoConnection = 2
	ErrorNoClient = 3
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

func makeHttpClient(config TestConfig) driver.Client {
	conn, err := http.NewConnection(http.ConnectionConfig{
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

func assertNotNil(err error, msg string) {
	if err != nil {
		fmt.Printf("Expected no error but got: %v, %s\n", err, msg)
		os.Exit(100)
	}
}
