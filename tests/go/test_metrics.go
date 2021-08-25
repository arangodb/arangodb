package main

import (
  "fmt"
	"os"
	"strings"
	driver "github.com/arangodb/go-driver"
	"github.com/arangodb/go-driver/http"
)


func main() {
  fmt.Printf("%s world!\n", Helper())
	endpointstring, ok := os.LookupEnv("TEST_ENDPOINTS")
	if !ok {
		fmt.Printf("Did not find TEST_ENDPOINTS!\n")
		os.Exit(1)
  }
	endpoints := strings.Split(endpointstring, ",")
	conn, err := http.NewConnection(http.ConnectionConfig{
			Endpoints: endpoints,
	})
	if err != nil {
		fmt.Printf("Could not create client connection: %v\n", err)
	}
	client, err := driver.NewClient(driver.ClientConfig{
			Connection: conn,
	})
	if err != nil {
		fmt.Printf("Could not connect: %v\n", err)
		os.Exit(2)
	}
	versionInfo, err := client.Version(nil)
	if err != nil {
		fmt.Printf("Could not get version: %v\n", err)
		os.Exit(3)
	}
	fmt.Printf("Version found: %s\n", versionInfo.Version)
	os.Exit(0)
}

