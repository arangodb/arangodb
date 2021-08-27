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

import (
	"fmt"
	"strconv"
	"sync"
)

type Obj struct {
	Key    string   `json: "_key"`
	Name   string   `json: "name"`
	Number int64    `json: "number"`
}

func main() {
	testGreeting("Test high parallelism")
	config := configFromEnv()

	client := makeHttp2Client(config.Endpoints)
	db := makeDatabase(client, "TestDB", nil)
	coll := makeCollection(db, "TestColl", nil)
	wg := sync.WaitGroup{}
	mutex := sync.Mutex{}
  for i := 1; i <= 100; i++ {
		wg.Add(1)
		fmt.Printf("Starting go-routine %d...\n", i)
		go func(nr int) {
			l := make([]Obj, 0, 1024)
			for j := 1; j <= 10000; j++ {
				l = append(l, Obj{ Key: "K_" + strconv.Itoa(i) + "_" + strconv.Itoa(j),
				                   Name: "This is a string " + strconv.Itoa(j),
													 Number: int64(j),
												 })
				if len(l) % 1000 == 0 {
					_, _, err := coll.CreateDocuments(nil, l)
				  if err != nil {
						mutex.Lock()
						fmt.Printf("go routine %d got an error: %v\n", nr, err)
						mutex.Unlock()
						break
					}
					l = l[0:0]
					mutex.Lock()
					fmt.Printf("go routine %d has inserted %d documents.\n", nr, j)
					mutex.Unlock()
				}
			}
			mutex.Lock()
			fmt.Printf("go routine %d done.\n", nr)
			mutex.Unlock()
			wg.Done()
		}(i)
	}
	wg.Wait()

	cleanupDatabasesAndCollections()
}
