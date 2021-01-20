# coding: utf-8

require 'rspec'
require 'json'
require 'arangodb.rb'

describe ArangoDB do

  context "dealing with the replication interface:" do

    api = "/_api/replication"
    prefix = "api-replication"

################################################################################
## general
################################################################################

    context "dealing with general functions" do

      it "fetches the server id 1" do
        # fetch id
        cmd = api + "/server-id"
        doc = ArangoDB.log_get("#{prefix}-server-id", cmd)

        doc.code.should eq(200)
        doc.parsed_response['serverId'].should match(/^\d+$/)
      end

    end

################################################################################
## applier
################################################################################

    context "dealing with the applier" do

      before do
        ArangoDB.put(api + "/applier-stop", :body => "")
        ArangoDB.delete(api + "/applier-state", :body => "")
      end

      after do
        ArangoDB.put(api + "/applier-stop", :body => "")
        ArangoDB.delete(api + "/applier-state", :body => "")
      end

################################################################################
## start
################################################################################

      it "starts the applier 1" do
        cmd = api + "/applier-start"
        doc = ArangoDB.log_put("#{prefix}-applier-start", cmd, :body => "")
        doc.code.should eq(400) # because configuration is invalid
      end

################################################################################
## stop
################################################################################

      it "stops the applier 1" do
        cmd = api + "/applier-stop"
        doc = ArangoDB.log_put("#{prefix}-applier-start", cmd, :body => "")
        doc.code.should eq(200)
      end

################################################################################
## properties
################################################################################

      it "fetches the applier config 1" do
        cmd = api + "/applier-config"
        doc = ArangoDB.log_get("#{prefix}-applier-config", cmd, :body => "")

        doc.code.should eq(200)
        all = doc.parsed_response
        all["requestTimeout"].should be_kind_of(Numeric)
        all["connectTimeout"].should be_kind_of(Numeric)
        all["ignoreErrors"].should be_kind_of(Integer)
        all["maxConnectRetries"].should be_kind_of(Integer)
        all["sslProtocol"].should be_kind_of(Integer)
        all["chunkSize"].should be_kind_of(Integer)
        all.should have_key("autoStart")
        all.should have_key("adaptivePolling")
        all.should have_key("autoResync")
        all.should have_key("includeSystem")
        all.should have_key("requireFromPresent")
        all.should have_key("verbose")
        all["restrictType"].should be_kind_of(String)
        all["connectionRetryWaitTime"].should be_kind_of(Numeric)
        all["initialSyncMaxWaitTime"].should be_kind_of(Numeric)
        all["idleMinWaitTime"].should be_kind_of(Numeric)
        all["idleMaxWaitTime"].should be_kind_of(Numeric)
      end

################################################################################
## set and fetch properties
################################################################################

      it "sets and re-fetches the applier config 1" do
        cmd = api + "/applier-config"
        body = '{ "endpoint" : "tcp://127.0.0.1:9999", "database" : "foo", "ignoreErrors" : 5, "requestTimeout" : 32.2, "connectTimeout" : 51.1, "maxConnectRetries" : 12345, "chunkSize" : 143423232, "autoStart" : true, "adaptivePolling" : false, "autoResync" : true, "includeSystem" : true, "requireFromPresent" : true, "verbose" : true, "connectionRetryWaitTime" : 22.12, "initialSyncMaxWaitTime" : 12.21, "idleMinWaitTime" : 1.4, "idleMaxWaitTime" : 7.3 }'
        doc = ArangoDB.log_put("#{prefix}-applier-config", cmd, :body => body)

        doc.code.should eq(200)
        all = doc.parsed_response
        all["endpoint"].should eq("tcp://127.0.0.1:9999")
        all["database"].should eq("foo")
        all["requestTimeout"].should eq(32.2)
        all["connectTimeout"].should eq(51.1)
        all["ignoreErrors"].should eq(5)
        all["maxConnectRetries"].should eq(12345)
        all["sslProtocol"].should eq(0)
        all["chunkSize"].should eq(143423232)
        all["autoStart"].should eq(true)
        all["adaptivePolling"].should eq(false)
        all["autoResync"].should eq(true)
        all["includeSystem"].should eq(true)
        all["requireFromPresent"].should eq(true)
        all["verbose"].should eq(true)
        all["connectionRetryWaitTime"].should eq(22.12)
        all["initialSyncMaxWaitTime"].should eq(12.21)
        all["idleMinWaitTime"].should eq(1.4)
        all["idleMaxWaitTime"].should eq(7.3)

        # refetch same data
        doc = ArangoDB.log_get("#{prefix}-applier-config", cmd, :body => "")

        doc.code.should eq(200)
        all = doc.parsed_response
        all["endpoint"].should eq("tcp://127.0.0.1:9999")
        all["database"].should eq("foo")
        all["requestTimeout"].should eq(32.2)
        all["connectTimeout"].should eq(51.1)
        all["ignoreErrors"].should eq(5)
        all["maxConnectRetries"].should eq(12345)
        all["sslProtocol"].should eq(0)
        all["chunkSize"].should eq(143423232)
        all["autoStart"].should eq(true)
        all["adaptivePolling"].should eq(false)
        all["autoResync"].should eq(true)
        all["includeSystem"].should eq(true)
        all["requireFromPresent"].should eq(true)
        all["verbose"].should eq(true)
        all["connectionRetryWaitTime"].should eq(22.12)
        all["initialSyncMaxWaitTime"].should eq(12.21)
        all["idleMinWaitTime"].should eq(1.4)
        all["idleMaxWaitTime"].should eq(7.3)


        body = '{ "endpoint" : "ssl://127.0.0.1:12345", "database" : "bar", "ignoreErrors" : 2, "requestTimeout" : 12.5, "connectTimeout" : 26.3, "maxConnectRetries" : 12, "chunkSize" : 1234567, "autoStart" : false, "adaptivePolling" : true, "autoResync" : false, "includeSystem" : false, "requireFromPresent" : false, "verbose" : false, "connectionRetryWaitTime" : 2.5, "initialSyncMaxWaitTime" : 4.3, "idleMinWaitTime" : 0.22, "idleMaxWaitTime" : 3.5 }'
        doc = ArangoDB.log_put("#{prefix}-applier-config", cmd, :body => body)

        doc.code.should eq(200)
        all = doc.parsed_response
        all["endpoint"].should eq("ssl://127.0.0.1:12345")
        all["database"].should eq("bar")
        all["requestTimeout"].should eq(12.5)
        all["connectTimeout"].should eq(26.3)
        all["ignoreErrors"].should eq(2)
        all["maxConnectRetries"].should eq(12)
        all["sslProtocol"].should eq(0)
        all["chunkSize"].should eq(1234567)
        all["autoStart"].should eq(false)
        all["adaptivePolling"].should eq(true)
        all["autoResync"].should eq(false)
        all["includeSystem"].should eq(false)
        all["requireFromPresent"].should eq(false)
        all["verbose"].should eq(false)
        all["connectionRetryWaitTime"].should eq(2.5)
        all["initialSyncMaxWaitTime"].should eq(4.3)
        all["idleMinWaitTime"].should eq(0.22)
        all["idleMaxWaitTime"].should eq(3.5)

        # refetch same data
        doc = ArangoDB.log_get("#{prefix}-applier-config", cmd, :body => "")

        doc.code.should eq(200)
        all = doc.parsed_response
        all["endpoint"].should eq("ssl://127.0.0.1:12345")
        all["database"].should eq("bar")
        all["requestTimeout"].should eq(12.5)
        all["connectTimeout"].should eq(26.3)
        all["ignoreErrors"].should eq(2)
        all["maxConnectRetries"].should eq(12)
        all["sslProtocol"].should eq(0)
        all["chunkSize"].should eq(1234567)
        all["autoStart"].should eq(false)
        all["adaptivePolling"].should eq(true)
        all["autoResync"].should eq(false)
        all["includeSystem"].should eq(false)
        all["requireFromPresent"].should eq(false)
        all["verbose"].should eq(false)
        all["connectionRetryWaitTime"].should eq(2.5)
        all["initialSyncMaxWaitTime"].should eq(4.3)
        all["idleMinWaitTime"].should eq(0.22)
        all["idleMaxWaitTime"].should eq(3.5)
      end

################################################################################
## state
################################################################################

      it "checks the applier state 1" do
        # fetch state
        cmd = api + "/applier-state"
        doc = ArangoDB.log_get("#{prefix}-applier-state", cmd, :body => "")

        doc.code.should eq(200)
        all = doc.parsed_response
        all.should have_key('state')
        all.should have_key('server')

        state = all['state']
        state['running'].should eq(false)
        state.should have_key("lastAppliedContinuousTick")
        state.should have_key("lastProcessedContinuousTick")
        state.should have_key("lastAvailableContinuousTick")
        state.should have_key("safeResumeTick")

        state.should have_key("progress")
        progress = state['progress']
        progress.should have_key("time")
        progress['time'].should match(/^(\d+-\d+-\d+T\d+:\d+:\d+Z)?$/)
        progress.should have_key("failedConnects")

        state.should have_key("totalRequests")
        state.should have_key("totalFailedConnects")
        state.should have_key("totalEvents")
        state.should have_key("totalOperationsExcluded")

        state.should have_key("lastError")
        lastError = state["lastError"]
        lastError.should have_key("errorNum")
        state.should have_key("time")
        state['time'].should match(/^\d+-\d+-\d+T\d+:\d+:\d+Z$/)
      end
    end

################################################################################
## logger
################################################################################

    context "dealing with the logger" do
      before do
        ArangoDB.drop_collection("UnitTestsReplication")
      end

      after do
        ArangoDB.drop_collection("UnitTestsReplication")
      end

################################################################################
## state
################################################################################

      it "checks the logger state" do
        # fetch state
        cmd = api + "/logger-state"
        doc = ArangoDB.log_get("#{prefix}-logger-state", cmd, :body => "")

        doc.code.should eq(200)

        all = doc.parsed_response
        all.should have_key('state')
        all.should have_key('server')
        all.should have_key('clients')

        state = all['state']
        state['running'].should eq(true)
        state['lastLogTick'].should match(/^\d+$/)
        state['time'].should match(/^\d+-\d+-\d+T\d+:\d+:\d+Z$/)

        server = all['server']
        server['engine'].should eq("rocksdb")
        server['serverId'].should match(/^\d+$/)
        server.should have_key('version')
      end

################################################################################
## firstTick
################################################################################

      it "fetches the first available tick" do
        # fetch state
        cmd = api + "/logger-first-tick"
        doc = ArangoDB.log_get("#{prefix}-logger-first-tick", cmd, :body => "")

        doc.code.should eq(200)

        result = doc.parsed_response
        result.should have_key('firstTick')

        result['firstTick'].should match(/^\d+$/)
      end

################################################################################
## tickRanges
################################################################################

      it "fetches the available tick ranges" do
        # fetch state
        cmd = api + "/logger-tick-ranges"
        doc = ArangoDB.log_get("#{prefix}-logger-tick-ranges", cmd, :body => "")

        doc.code.should eq(200)

        result = doc.parsed_response
        result.size.should be > 0 
        
        result.each { |datafile|
          datafile.should have_key('datafile')
          datafile.should have_key('status')
          datafile.should have_key('tickMin')
          datafile.should have_key('tickMax')
          datafile['tickMin'].should match(/^\d+$/)
          datafile['tickMax'].should match(/^\d+$/)
        }
      end

################################################################################
## follow
################################################################################

      it "fetches the empty follow log 1" do
        while 1
          cmd = api + "/logger-state"
          doc = ArangoDB.log_get("#{prefix}-follow-empty", cmd, :body => "")
          doc.code.should eq(200)
          doc.parsed_response["state"]["running"].should eq(true)
          fromTick = doc.parsed_response["state"]["lastLogTick"]

          cmd = api + "/logger-follow?from=" + fromTick
          doc = ArangoDB.log_get("#{prefix}-follow-empty", cmd, :body => "", :format => :plain)

          if doc.code != 204
            # someone else did something else
            doc.code.should eq(200)
            # sleep for a second and try again
            sleep 1
          else
            doc.code.should eq(204)

# doc.headers["x-arango-replication-checkmore"].should eq("false")
            doc.headers["x-arango-replication-lastincluded"].should match(/^\d+$/)
            doc.headers["x-arango-replication-lastincluded"].should eq("0")
            doc.headers["content-type"].should eq("application/x-arango-dump; charset=utf-8")

            body = doc.response.body
            body.should eq(nil)
            break
          end
        end
      end

      it "fetches a create collection action from the follow log 1" do
        ArangoDB.drop_collection("UnitTestsReplication")

        sleep 5

        cmd = api + "/logger-state"
        doc = ArangoDB.log_get("#{prefix}-follow-create-collection", cmd, :body => "")
        doc.code.should eq(200)
        doc.parsed_response["state"]["running"].should eq(true)
        fromTick = doc.parsed_response["state"]["lastLogTick"]

        cid = ArangoDB.create_collection("UnitTestsReplication")

        sleep 5

        cmd = api + "/logger-follow?from=" + fromTick
        doc = ArangoDB.log_get("#{prefix}-follow-create-collection", cmd, :body => "", :format => :plain)
        doc.code.should eq(200)

        doc.headers["x-arango-replication-lastincluded"].should match(/^\d+$/)
        doc.headers["x-arango-replication-lastincluded"].should_not eq("0")
        doc.headers["content-type"].should eq("application/x-arango-dump; charset=utf-8")

        body = doc.response.body

        while 1
          position = body.index("\n")

          break if position == nil

          part = body.slice(0, position)
          document = JSON.parse(part)

          if document["type"] == 2000 and document["cid"] == cid
            document.should have_key("tick")
            document.should have_key("type")
            document.should have_key("cid")
            document.should have_key("cname")
            document.should have_key("data")

            document["tick"].should match(/^\d+$/)
            document["tick"].to_i.should >= fromTick.to_i
            document["type"].should eq(2000)
            document["cid"].should eq(cid)
            document["cid"].should be_kind_of(String)
            document["cname"].should eq("UnitTestsReplication")

            c = document["data"]
            c.should have_key("version")
            c["type"].should eq(2)
            c["cid"].should eq(cid)
            c["cid"].should be_kind_of(String)
            c["deleted"].should eq(false)
            c["name"].should eq("UnitTestsReplication")
            c["waitForSync"].should eq(true)
          end

          body = body.slice(position + 1, body.length)
        end

      end

      it "fetches some collection operations from the follow log 1" do
        ArangoDB.drop_collection("UnitTestsReplication")

        sleep 5

        cmd = api + "/logger-state"
        doc = ArangoDB.log_get("#{prefix}-follow-collection", cmd, :body => "")
        doc.code.should eq(200)
        doc.parsed_response["state"]["running"].should eq(true)
        fromTick = doc.parsed_response["state"]["lastLogTick"]

        # create collection
        cid = ArangoDB.create_collection("UnitTestsReplication")

        # create document
        cmd = "/_api/document?collection=UnitTestsReplication"
        body = "{ \"_key\" : \"test\", \"test\" : false }"
        doc = ArangoDB.log_post("#{prefix}-follow-collection", cmd, :body => body)
        doc.code.should eq(201)
        rev = doc.parsed_response["_rev"]

        # delete document
        cmd = "/_api/document/UnitTestsReplication/test"
        doc = ArangoDB.log_delete("#{prefix}-follow-collection", cmd)
        doc.code.should eq(200)

        # drop collection
        cmd = "/_api/collection/UnitTestsReplication"
        doc = ArangoDB.log_delete("#{prefix}-follow-collection", cmd)
        doc.code.should eq(200)

        sleep 5

        cmd = api + "/logger-follow?from=" + fromTick
        doc = ArangoDB.log_get("#{prefix}-follow-collection", cmd, :body => "", :format => :plain)
        doc.code.should eq(200)

        doc.headers["x-arango-replication-lastincluded"].should match(/^\d+$/)
        doc.headers["x-arango-replication-lastincluded"].should_not eq("0")
        doc.headers["content-type"].should eq("application/x-arango-dump; charset=utf-8")


        body = doc.response.body
        i = 0
        while 1
          position = body.index("\n")

          break if position == nil

          part = body.slice(0, position)
          document = JSON.parse(part)

          if i == 0
            if document["type"] == 2000 and document["cid"] == cid
              # create collection
              document.should have_key("tick")
              document.should have_key("type")
              document.should have_key("cid")
              document.should have_key("cname")
              document.should have_key("data")

              document["tick"].should match(/^\d+$/)
              document["tick"].to_i.should >= fromTick.to_i
              document["type"].should eq(2000)
              document["cid"].should eq(cid)
              document["cid"].should be_kind_of(String)
              document["cname"].should eq("UnitTestsReplication")

              c = document["data"]
              c.should have_key("version")
              c["type"].should eq(2)
              c["cid"].should eq(cid)
              c["cid"].should be_kind_of(String)
              c["deleted"].should eq(false)
              c["name"].should eq("UnitTestsReplication")
              c["waitForSync"].should eq(true)

              i = i + 1
            end

          elsif i == 1 and document["type"] == 2300 and document["cid"] == cid
            # create document
            document.should have_key("tick")
            document.should have_key("type")
            document.should have_key("cid")

            document["tick"].should match(/^\d+$/)
            document["tick"].to_i.should >= fromTick.to_i
            document["type"].should eq(2300)
            document["cid"].should eq(cid)
            document["cid"].should be_kind_of(String)
            document["data"]["_key"].should eq("test")
            document["data"]["_rev"].should match(/^[a-zA-Z0-9_\-]+$/)
            document["data"]["_rev"].should_not eq("0")
            document["data"]["test"].should eq(false)

            i = i + 1
          elsif i == 2 and document["type"] == 2302 and document["cid"] == cid
            # delete document
            document.should have_key("tick")
            document.should have_key("type")
            document.should have_key("cid")

            document["tick"].should match(/^\d+$/)
            document["tick"].to_i.should >= fromTick.to_i
            document["type"].should eq(2302)
            document["cid"].should eq(cid)
            document["cid"].should be_kind_of(String)
            document["data"]["_key"].should eq("test")

            i = i + 1
          elsif i == 3 and document["type"] == 2001 and document["cid"] == cid
            # drop collection
            document.should have_key("tick")
            document.should have_key("type")
            document.should have_key("cid")

            document["tick"].should match(/^\d+$/)
            document["tick"].to_i.should >= fromTick.to_i
            document["type"].should eq(2001)
            document["cid"].should eq(cid)
            document["cid"].should be_kind_of(String)

            i = i + 1
          end

          body = body.slice(position + 1, body.length)
        end

      end

    end

################################################################################
## inventory / dump
################################################################################

    context "dealing with the initial dump" do

      before do
        ArangoDB.drop_collection("UnitTestsReplication")
        ArangoDB.drop_collection("UnitTestsReplication2")
        doc = ArangoDB.post(api + "/batch", :body => "{}")
        doc.code.should eq(200)
        @batchId = doc.parsed_response['id']
        @batchId.should  match(/^\d+$/)
      end

      after do
        ArangoDB.delete(api + "/batch/#{@batchId}", :body => "")
        ArangoDB.drop_collection("UnitTestsReplication")
        ArangoDB.drop_collection("UnitTestsReplication2")
      end

################################################################################
## inventory
################################################################################

      it "checks the initial inventory 1" do
        cmd = api + "/inventory?includeSystem=false&batchId=#{@batchId}"
        doc = ArangoDB.log_get("#{prefix}-inventory", cmd, :body => "")

        doc.code.should eq(200)
        all = doc.parsed_response
        all.should have_key('collections')
        all.should have_key('state')

        collections = all["collections"]
        filtered = [ ]
        collections.each { |collection|
          if [ "UnitTestsReplication", "UnitTestsReplication2" ].include? collection["parameters"]["name"]
            filtered.push collection
          end
        }
        filtered.should eq([ ])
        state = all['state']
        state['running'].should eq(true)
        state['lastLogTick'].should match(/^\d+$/)
        state['time'].should match(/^\d+-\d+-\d+T\d+:\d+:\d+Z$/)
      end

      it "checks the initial inventory for non-system collections 1" do
        cmd = api + "/inventory?includeSystem=false&batchId=#{@batchId}"
        doc = ArangoDB.log_get("#{prefix}-inventory-system", cmd, :body => "")

        doc.code.should eq(200)
        all = doc.parsed_response
        all.should have_key('collections')
        all.should have_key('state')

        collections = all["collections"]
        collections.each { |collection|
          collection["parameters"]["name"].should_not match(/^_/)
        }
      end

      it "checks the initial inventory for system collections" do
        cmd = api + "/inventory?includeSystem=true&batchId=#{@batchId}"
        doc = ArangoDB.log_get("#{prefix}-inventory-system", cmd, :body => "")

        doc.code.should eq(200)
        all = doc.parsed_response
        all.should have_key('collections')
        all.should have_key('state')

        collections = all["collections"]
        systemCollections = 0
        collections.each { |collection|
          if collection["parameters"]["name"].match(/^_/)
            systemCollections = systemCollections + 1
          end
        }

        systemCollections.should_not eq(0)
      end

      it "checks the inventory after creating collections 1" do
        cid = ArangoDB.create_collection("UnitTestsReplication", false)
        cid2 = ArangoDB.create_collection("UnitTestsReplication2", true, 3)

        cmd = api + "/inventory?includeSystem=false&batchId=#{@batchId}"
        doc = ArangoDB.log_get("#{prefix}-inventory-create", cmd, :body => "")
        doc.code.should eq(200)

        all = doc.parsed_response
        all.should have_key('collections')
        all.should have_key('state')

        state = all['state']
        state['running'].should eq(true)
        state['lastLogTick'].should match(/^\d+$/)
        state['time'].should match(/^\d+-\d+-\d+T\d+:\d+:\d+Z$/)

        collections = all['collections']
        filtered = [ ]
        collections.each { |collection|
          if [ "UnitTestsReplication", "UnitTestsReplication2" ].include? collection["parameters"]["name"]
            filtered.push collection
          end
        }
        filtered.length.should eq(2)

        # first collection
        c = filtered[0]
        c.should have_key("parameters")
        c.should have_key("indexes")

        parameters = c['parameters']
        parameters.should have_key("version")
        parameters["version"].should be_kind_of(Integer)
        parameters["type"].should be_kind_of(Integer)
        parameters["type"].should eq(2)
        parameters["cid"].should eq(cid)
        parameters["cid"].should be_kind_of(String)
        parameters["deleted"].should eq(false)
        parameters["name"].should eq("UnitTestsReplication")
        parameters["waitForSync"].should eq(false)

        c['indexes'].should eq([ ])

        # second collection
        c = filtered[1]
        c.should have_key("parameters")
        c.should have_key("indexes")

        parameters = c['parameters']
        parameters.should have_key("version")
        parameters["version"].should be_kind_of(Integer)
        parameters["type"].should be_kind_of(Integer)
        parameters["type"].should eq(3)
        parameters["cid"].should eq(cid2)
        parameters["cid"].should be_kind_of(String)
        parameters["deleted"].should eq(false)
        parameters["name"].should eq("UnitTestsReplication2")
        parameters["waitForSync"].should eq(true)

        c['indexes'].should eq([ ])
      end

      it "checks the inventory with indexes 1" do
        cid = ArangoDB.create_collection("UnitTestsReplication", false)
        cid2 = ArangoDB.create_collection("UnitTestsReplication2", false)

        body = "{ \"type\" : \"hash\", \"unique\" : false, \"fields\" : [ \"a\", \"b\" ] }"
        doc = ArangoDB.log_post("#{prefix}-inventory2", "/_api/index?collection=UnitTestsReplication", :body => body)
        doc.code.should eq(201)

        body = "{ \"type\" : \"skiplist\", \"unique\" : false, \"fields\" : [ \"c\" ] }"
        doc = ArangoDB.log_post("#{prefix}-inventory2", "/_api/index?collection=UnitTestsReplication", :body => body)
        doc.code.should eq(201)

        # create indexes for second collection
        body = "{ \"type\" : \"skiplist\", \"unique\" : true, \"fields\" : [ \"d\" ] }"
        doc = ArangoDB.log_post("#{prefix}-inventory2", "/_api/index?collection=UnitTestsReplication2", :body => body)
        doc.code.should eq(201)

        cmd = api + "/inventory?batchId=#{@batchId}"
        doc = ArangoDB.log_get("#{prefix}-inventory2", cmd, :body => "")
        doc.code.should eq(200)

        all = doc.parsed_response
        all.should have_key('collections')
        all.should have_key('state')

        state = all['state']
        state['running'].should eq(true)
        state['lastLogTick'].should match(/^\d+$/)
        state['time'].should match(/^\d+-\d+-\d+T\d+:\d+:\d+Z$/)

        collections = all['collections']
        filtered = [ ]
        collections.each { |collection|
          if [ "UnitTestsReplication", "UnitTestsReplication2" ].include? collection["parameters"]["name"]
            filtered.push collection
          end
        }
        filtered.length.should eq(2)

        # first collection
        c = filtered[0]
        c.should have_key("parameters")
        c.should have_key("indexes")

        parameters = c['parameters']
        parameters.should have_key("version")
        parameters["version"].should be_kind_of(Integer)
        parameters["type"].should be_kind_of(Integer)
        parameters["type"].should eq(2)
        parameters["cid"].should eq(cid)
        parameters["cid"].should be_kind_of(String)
        parameters["deleted"].should eq(false)
        parameters["name"].should eq("UnitTestsReplication")
        parameters["waitForSync"].should eq(false)

        indexes = c['indexes']
        indexes.length.should eq(2)

        idx = indexes[0]
        idx["id"].should match(/^\d+$/)
        idx["type"].should eq("hash")
        idx["unique"].should eq(false)
        idx["fields"].should eq([ "a", "b" ])

        idx = indexes[1]
        idx["id"].should match(/^\d+$/)
        idx["type"].should eq("skiplist")
        idx["unique"].should eq(false)
        idx["fields"].should eq([ "c" ])

        # second collection
        c = filtered[1]
        c.should have_key("parameters")
        c.should have_key("indexes")

        parameters = c['parameters']
        parameters.should have_key("version")
        parameters["version"].should be_kind_of(Integer)
        parameters["type"].should be_kind_of(Integer)
        parameters["type"].should eq(2)
        parameters["cid"].should eq(cid2)
        parameters["cid"].should be_kind_of(String)
        parameters["deleted"].should eq(false)
        parameters["name"].should eq("UnitTestsReplication2")
        parameters["waitForSync"].should eq(false)

        indexes = c['indexes']
        indexes.length.should eq(1)

        idx = indexes[0]
        idx["id"].should match(/^\d+$/)
        idx["type"].should eq("skiplist")
        idx["unique"].should eq(true)
        idx["fields"].should eq([ "d" ])

      end

################################################################################
## dump
################################################################################

      it "checks the dump for an empty collection 1" do
        cid = ArangoDB.create_collection("UnitTestsReplication", false)

        doc = ArangoDB.log_put("#{prefix}-deleted", "/_admin/wal/flush?waitForSync=true&waitForCollector=true", :body => "")
        doc.code.should eq(200)

        cmd = api + "/dump?collection=UnitTestsReplication&batchId=#{@batchId}"
        doc = ArangoDB.log_get("#{prefix}-dump-empty", cmd, :body => "")

        doc.code.should eq(204)

        doc.headers["x-arango-replication-checkmore"].should eq("false")
        doc.headers["x-arango-replication-lastincluded"].should eq("0")
        doc.headers["content-type"].should eq("application/x-arango-dump; charset=utf-8")
        doc.response.body.should eq(nil)
      end

      it "checks the dump for a non-empty collection 1" do
        cid = ArangoDB.create_collection("UnitTestsReplication", false)

        (0...100).each{|i|
          body = "{ \"_key\" : \"test" + i.to_s + "\", \"test\" : " + i.to_s + " }"
          doc = ArangoDB.post("/_api/document?collection=UnitTestsReplication", :body => body)
          doc.code.should eq(202)
        }

        ArangoDB.delete(api + "/batch/#{@batchId}", :body => "")
        doc0 = ArangoDB.post(api + "/batch", :body => "{}")
        @batchId = doc0.parsed_response["id"]
        @batchId.should  match(/^\d+$/)

        cmd = api + "/dump?collection=UnitTestsReplication&batchId=#{@batchId}"
        doc = ArangoDB.log_get("#{prefix}-dump-non-empty", cmd, :body => "", :format => :plain)

        doc.code.should eq(200)

        doc.headers["x-arango-replication-checkmore"].should eq("false")
        doc.headers["x-arango-replication-lastincluded"].should match(/^\d+$/)
        doc.headers["x-arango-replication-lastincluded"].should_not eq("0")
        doc.headers["content-type"].should eq("application/x-arango-dump; charset=utf-8")

        body = doc.response.body
        i = 0
        while 1
          position = body.index("\n")

          break if position == nil

          part = body.slice(0, position)

          doc = JSON.parse(part)
          doc.should have_key("type")
          doc.should have_key("data")
          doc.should_not have_key("_key")
          doc['type'].should eq(2300)
          doc['data']['_key'].should match(/^test[0-9]+$/)
          doc["data"]["_rev"].should match(/^[a-zA-Z0-9_\-]+$/)
          doc['data'].should have_key("test")

          body = body.slice(position + 1, body.length)
          i = i + 1
        end

        i.should eq(100)
      end
      
      it "checks the dump for a non-empty collection, no envelopes" do
        cid = ArangoDB.create_collection("UnitTestsReplication", false)

        (0...100).each{|i|
          body = "{ \"_key\" : \"test" + i.to_s + "\", \"test\" : " + i.to_s + " }"
          doc = ArangoDB.post("/_api/document?collection=UnitTestsReplication", :body => body)
          doc.code.should eq(202)
        }

        ArangoDB.delete(api + "/batch/#{@batchId}", :body => "")
        doc0 = ArangoDB.post(api + "/batch", :body => "{}")
        @batchId = doc0.parsed_response["id"]
        @batchId.should  match(/^\d+$/)

        cmd = api + "/dump?collection=UnitTestsReplication&batchId=#{@batchId}&useEnvelope=false"
        doc = ArangoDB.log_get("#{prefix}-dump-non-empty", cmd, :body => "", :format => :plain)

        doc.code.should eq(200)

        doc.headers["x-arango-replication-checkmore"].should eq("false")
        doc.headers["x-arango-replication-lastincluded"].should match(/^\d+$/)
        doc.headers["x-arango-replication-lastincluded"].should_not eq("0")
        doc.headers["content-type"].should eq("application/x-arango-dump; charset=utf-8")

        body = doc.response.body
        i = 0
        while 1
          position = body.index("\n")

          break if position == nil

          part = body.slice(0, position)

          doc = JSON.parse(part)
          doc.should_not have_key("type")
          doc.should_not have_key("data")
          doc.should have_key("_key")
          doc['_key'].should match(/^test[0-9]+$/)
          doc["_rev"].should match(/^[a-zA-Z0-9_\-]+$/)
          doc.should have_key("test")

          body = body.slice(position + 1, body.length)
          i = i + 1
        end

        i.should eq(100)
      end

      it "checks the dump for a non-empty collection, small chunkSize 1" do
        cid = ArangoDB.create_collection("UnitTestsReplication", false)

        (0...100).each{|i|
          body = "{ \"_key\" : \"test" + i.to_s + "\", \"test\" : " + i.to_s + " }"
          doc = ArangoDB.post("/_api/document?collection=UnitTestsReplication", :body => body)
          doc.code.should eq(202)
        }

        doc = ArangoDB.log_put("#{prefix}-deleted", "/_admin/wal/flush?waitForSync=true&waitForCollector=true", :body => "")
        doc.code.should eq(200)

        ArangoDB.delete(api + "/batch/#{@batchId}", :body => "")
        doc0 = ArangoDB.post(api + "/batch", :body => "{}")
        @batchId = doc0.parsed_response["id"]
        @batchId.should  match(/^\d+$/)

        cmd = api + "/dump?collection=UnitTestsReplication&chunkSize=1024&batchId=#{@batchId}"
        doc = ArangoDB.log_get("#{prefix}-dump-non-empty", cmd, :body => "", :format => :plain)

        doc.code.should eq(200)

        doc.headers["x-arango-replication-checkmore"].should eq("true")
        doc.headers["x-arango-replication-lastincluded"].should match(/^\d+$/)
        doc.headers["x-arango-replication-lastincluded"].should_not eq("0")
        doc.headers["content-type"].should eq("application/x-arango-dump; charset=utf-8")

        body = doc.response.body
        i = 0
        while 1
          position = body.index("\n")

          break if position == nil

          part = body.slice(0, position)

          doc = JSON.parse(part)
          doc['type'].should eq(2300)
          doc['data']['_key'].should match(/^test[0-9]+$/)
          doc["data"]["_rev"].should match(/^[a-zA-Z0-9_\-]+$/)
          doc['data'].should have_key('test')

          body = body.slice(position + 1, body.length)
          i = i + 1
        end

        i.should be < 100
      end


      it "checks the dump for an edge collection 1" do
        cid = ArangoDB.create_collection("UnitTestsReplication", false)
        cid2 = ArangoDB.create_collection("UnitTestsReplication2", false, 3)

        (0...100).each{|i|
          body = "{ \"_key\" : \"test" + i.to_s + "\", \"_from\" : \"UnitTestsReplication/foo\", \"_to\" : \"UnitTestsReplication/bar\", \"test1\" : " + i.to_s + ", \"test2\" : false, \"test3\" : [ ], \"test4\" : { } }"
          doc = ArangoDB.post("/_api/document?collection=UnitTestsReplication2", :body => body)
          doc.code.should eq(202)
        }

        doc = ArangoDB.log_put("#{prefix}-deleted", "/_admin/wal/flush?waitForSync=true&waitForCollector=true", :body => "")
        doc.code.should eq(200)

        ArangoDB.delete(api + "/batch/#{@batchId}", :body => "")
        doc0 = ArangoDB.post(api + "/batch", :body => "{}")
        @batchId = doc0.parsed_response["id"]
        @batchId.should  match(/^\d+$/)

        cmd = api + "/dump?collection=UnitTestsReplication2&chunkSize=65536&batchId=#{@batchId}"
        doc = ArangoDB.log_get("#{prefix}-dump-edge", cmd, :body => "", :format => :plain)

        doc.code.should eq(200)

        doc.headers["x-arango-replication-checkmore"].should eq("false")
        doc.headers["x-arango-replication-lastincluded"].should match(/^\d+$/)
        doc.headers["x-arango-replication-lastincluded"].should_not eq("0")
        doc.headers["content-type"].should eq("application/x-arango-dump; charset=utf-8")

        body = doc.response.body
        i = 0
        while 1
          position = body.index("\n")

          break if position == nil

          part = body.slice(0, position)

          document = JSON.parse(part)
          document['type'].should eq(2300)
          document['data']['_key'].should match(/^test[0-9]+$/)
          document["data"]["_rev"].should match(/^[a-zA-Z0-9_\-]+$/)
          document['data']['_from'].should eq("UnitTestsReplication/foo")
          document['data']['_to'].should eq("UnitTestsReplication/bar")
          document['data'].should have_key('test1')
          document['data']['test2'].should eq(false)
          document['data']['test3'].should eq([ ])
          document['data']['test4'].should eq({ })

          body = body.slice(position + 1, body.length)
          i = i + 1
        end

        i.should eq(100)
      end

      it "checks the dump for an edge collection, small chunkSize 1" do
        cid = ArangoDB.create_collection("UnitTestsReplication", false)
        cid2 = ArangoDB.create_collection("UnitTestsReplication2", false, 3)

        (0...100).each{|i|
          body = "{ \"_key\" : \"test" + i.to_s + "\", \"_from\" : \"UnitTestsReplication/foo\", \"_to\" : \"UnitTestsReplication/bar\", \"test1\" : " + i.to_s + ", \"test2\" : false, \"test3\" : [ ], \"test4\" : { } }"
          doc = ArangoDB.post("/_api/document?collection=UnitTestsReplication2", :body => body)
          doc.code.should eq(202)
        }

        doc = ArangoDB.log_put("#{prefix}-deleted", "/_admin/wal/flush?waitForSync=true&waitForCollector=true", :body => "")
        doc.code.should eq(200)

        ArangoDB.delete(api + "/batch/#{@batchId}", :body => "")
        doc0 = ArangoDB.post(api + "/batch", :body => "{}")
        @batchId = doc0.parsed_response["id"]
        @batchId.should  match(/^\d+$/)

        cmd = api + "/dump?collection=UnitTestsReplication2&chunkSize=1024&batchId=#{@batchId}"
        doc = ArangoDB.log_get("#{prefix}-dump-edge", cmd, :body => "", :format => :plain)

        doc.code.should eq(200)

        doc.headers["x-arango-replication-checkmore"].should eq("true")
        doc.headers["x-arango-replication-lastincluded"].should match(/^\d+$/)
        doc.headers["x-arango-replication-lastincluded"].should_not eq("0")
        doc.headers["content-type"].should eq("application/x-arango-dump; charset=utf-8")

        body = doc.response.body
        i = 0
        while 1
          position = body.index("\n")

          break if position == nil

          part = body.slice(0, position)

          document = JSON.parse(part)
          document['type'].should eq(2300)
          document['data']['_key'].should match(/^test[0-9]+$/)
          document['data']['_rev'].should match(/^[a-zA-Z0-9_\-]+$/)
          document['data']['_from'].should eq("UnitTestsReplication/foo")
          document['data']['_to'].should eq("UnitTestsReplication/bar")
          document['data'].should have_key('test1')
          document['data']['test2'].should eq(false)
          document['data']['test3'].should eq([ ])
          document['data']['test4'].should eq({ })

          body = body.slice(position + 1, body.length)
          i = i + 1
        end

        i.should be < 100
      end

      it "checks the dump for a collection with deleted documents 1" do
        cid = ArangoDB.create_collection("UnitTestsReplication", false)

        doc = ArangoDB.log_put("#{prefix}-deleted", "/_admin/wal/flush?waitForSync=true&waitForCollector=true", :body => "")
        doc.code.should eq(200)

        (0...10).each{|i|
          body = "{ \"_key\" : \"test" + i.to_s + "\", \"test\" : " + i.to_s + " }"
          doc = ArangoDB.post("/_api/document?collection=UnitTestsReplication", :body => body)
          doc.code.should eq(202)

          doc = ArangoDB.delete("/_api/document/UnitTestsReplication/test" + i.to_s, :body => body)
          doc.code.should eq(202)
        }

        doc = ArangoDB.log_put("#{prefix}-deleted", "/_admin/wal/flush?waitForSync=true&waitForCollector=true", :body => "")
        doc.code.should eq(200)

        ArangoDB.delete(api + "/batch/#{@batchId}", :body => "")
        doc0 = ArangoDB.post(api + "/batch", :body => "{}")
        @batchId = doc0.parsed_response["id"]
        @batchId.should  match(/^\d+$/)

        cmd = api + "/dump?collection=UnitTestsReplication&batchId=#{@batchId}"
        doc = ArangoDB.log_get("#{prefix}-deleted", cmd, :body => "", :format => :plain)
        doc.code.should eq(204)

        doc.headers["x-arango-replication-checkmore"].should eq("false")
        doc.headers["x-arango-replication-lastincluded"].should eq("0")
        doc.headers["content-type"].should eq("application/x-arango-dump; charset=utf-8")
      end

      it "checks the dump for a truncated collection 1" do
        cid = ArangoDB.create_collection("UnitTestsReplication", false)

        (0...10).each{|i|
          body = "{ \"_key\" : \"test" + i.to_s + "\", \"test\" : " + i.to_s + " }"
          doc = ArangoDB.post("/_api/document?collection=UnitTestsReplication", :body => body)
          doc.code.should eq(202)
        }

        # truncate
        cmd = "/_api/collection/UnitTestsReplication/truncate"
        doc = ArangoDB.log_put("#{prefix}-truncated", cmd, :body => "", :format => :plain)
        doc.code.should eq(200)

        doc = ArangoDB.log_put("#{prefix}-deleted", "/_admin/wal/flush?waitForSync=true&waitForCollector=true", :body => "")
        doc.code.should eq(200)

        ArangoDB.delete(api + "/batch/#{@batchId}", :body => "")
        doc0 = ArangoDB.post(api + "/batch", :body => "{}")
        @batchId = doc0.parsed_response["id"]
        @batchId.should  match(/^\d+$/)

        cmd = api + "/dump?collection=UnitTestsReplication&batchId=#{@batchId}"
        doc = ArangoDB.log_get("#{prefix}-truncated", cmd, :body => "", :format => :plain)
        doc.code.should eq(204)

        doc.headers["x-arango-replication-checkmore"].should eq("false")
        doc.headers["x-arango-replication-lastincluded"].should eq("0")
        doc.headers["content-type"].should eq("application/x-arango-dump; charset=utf-8")
      end

      it "checks the dump for a non-empty collection, 3.0 mode 1" do
        cid = ArangoDB.create_collection("UnitTestsReplication", false)

        (0...100).each{|i|
          body = "{ \"_key\" : \"test" + i.to_s + "\", \"test\" : " + i.to_s + " }"
          doc = ArangoDB.post("/_api/document?collection=UnitTestsReplication", :body => body)
          doc.code.should eq(202)
        }

        ArangoDB.delete(api + "/batch/#{@batchId}", :body => "")
        doc0 = ArangoDB.post(api + "/batch", :body => "{}")
        @batchId = doc0.parsed_response["id"]
        @batchId.should  match(/^\d+$/)

        cmd = api + "/dump?collection=UnitTestsReplication&batchId=#{@batchId}"
        doc = ArangoDB.log_get("#{prefix}-dump-non-empty", cmd, :body => "", :format => :plain)

        doc.code.should eq(200)

        body = doc.response.body
        i = 0
        while 1
          position = body.index("\n")

          break if position == nil

          part = body.slice(0, position)

          doc = JSON.parse(part)
          doc['type'].should eq(2300)
          doc.should_not have_key("key")
          doc.should_not have_key("rev")
          doc['data']['_key'].should match(/^test[0-9]+$/)
          doc['data']['_rev'].should match(/^[a-zA-Z0-9_\-]+$/)
          doc['data'].should have_key('test')

          body = body.slice(position + 1, body.length)
          i = i + 1
        end

        i.should eq(100)
      end

      it "fetches incremental parts of a collection dump 1" do
        cid = ArangoDB.create_collection("UnitTestsReplication", false)

        (0...10).each{|i|
          body = "{ \"_key\" : \"test" + i.to_s + "\", \"test\" : " + i.to_s + " }"
          doc = ArangoDB.post("/_api/document?collection=UnitTestsReplication", :body => body)
          doc.code.should eq(202)
        }

        doc = ArangoDB.log_put("#{prefix}-deleted", "/_admin/wal/flush?waitForSync=true&waitForCollector=true", :body => "")
        doc.code.should eq(200)

        fromTick = "0"

        ArangoDB.delete(api + "/batch/#{@batchId}", :body => "")
        doc0 = ArangoDB.post(api + "/batch", :body => "{}")
        @batchId = doc0.parsed_response["id"]
        @batchId.should  match(/^\d+$/)

        (0...10).each{|i|
          cmd = api + "/dump?collection=UnitTestsReplication&from=" + fromTick + "&chunkSize=1&batchId=#{@batchId}"
          doc = ArangoDB.log_get("#{prefix}-incremental", cmd, :body => "", :format => :plain)
          doc.code.should eq(200)

          if i == 9
            doc.headers["x-arango-replication-checkmore"].should eq("false")
          else
            doc.headers["x-arango-replication-checkmore"].should eq("true")
          end

          doc.headers["x-arango-replication-lastincluded"].should match(/^\d+$/)
          doc.headers["x-arango-replication-lastincluded"].should_not eq("0")
          doc.headers["x-arango-replication-lastincluded"].to_i.should >= fromTick.to_i
          doc.headers["content-type"].should eq("application/x-arango-dump; charset=utf-8")

          fromTick = doc.headers["x-arango-replication-lastincluded"]

          body = doc.response.body
          document = JSON.parse(body)
          document['type'].should eq(2300)
          document['data']['_key'].should match(/^test[0-9]+$/)
          document['data']['_rev'].should match(/^[a-zA-Z0-9_\-]+$/)
          document['data'].should have_key('test')
        }
      end

    end

  end

  context "dealing with the replication interface on UnitTestDB:" do

    api = "/_db/UnitTestDB/_api/replication"
    prefix = "api-replication-testdb"

    before do
      res = ArangoDB.create_database("UnitTestDB");
      res.should eq(true)
    end

    after do
      res = ArangoDB.drop_database("UnitTestDB");
      res.should eq(true)
    end

################################################################################
## general
################################################################################

    context "dealing with general functions" do

      it "fetches the server id 2" do
        # fetch id
        cmd = api + "/server-id"
        doc = ArangoDB.log_get("#{prefix}-server-id", cmd)

        doc.code.should eq(200)
        doc.parsed_response['serverId'].should match(/^\d+$/)
      end

    end

################################################################################
## applier
################################################################################

    context "dealing with the applier" do

      before do
        ArangoDB.put(api + "/applier-stop", :body => "")
        ArangoDB.delete(api + "/applier-state", :body => "")
      end

      after do
        ArangoDB.put(api + "/applier-stop", :body => "")
        ArangoDB.delete(api + "/applier-state", :body => "")
      end

################################################################################
## start
################################################################################

      it "starts the applier 2" do
        cmd = api + "/applier-start"
        doc = ArangoDB.log_put("#{prefix}-applier-start", cmd, :body => "")
        doc.code.should eq(400) # because configuration is invalid
      end

################################################################################
## stop
################################################################################

      it "stops the applier 2" do
        cmd = api + "/applier-stop"
        doc = ArangoDB.log_put("#{prefix}-applier-start", cmd, :body => "")
        doc.code.should eq(200)
      end

################################################################################
## properties
################################################################################

      it "fetches the applier config 2" do
        cmd = api + "/applier-config"
        doc = ArangoDB.log_get("#{prefix}-applier-config", cmd, :body => "")

        doc.code.should eq(200)
        all = doc.parsed_response
        all["requestTimeout"].should be_kind_of(Numeric)
        all["connectTimeout"].should be_kind_of(Numeric)
        all["ignoreErrors"].should be_kind_of(Integer)
        all["maxConnectRetries"].should be_kind_of(Integer)
        all["sslProtocol"].should be_kind_of(Integer)
        all["chunkSize"].should be_kind_of(Integer)
        all.should have_key("autoStart")
        all.should have_key("adaptivePolling")
        all.should have_key("autoResync")
        all.should have_key("includeSystem")
        all.should have_key("requireFromPresent")
        all.should have_key("verbose")
        all["restrictType"].should be_kind_of(String)
        all["connectionRetryWaitTime"].should be_kind_of(Numeric)
        all["initialSyncMaxWaitTime"].should be_kind_of(Numeric)
        all["idleMinWaitTime"].should be_kind_of(Numeric)
        all["idleMaxWaitTime"].should be_kind_of(Numeric)
      end

################################################################################
## set and fetch properties
################################################################################

      it "sets and re-fetches the applier config 2" do
        cmd = api + "/applier-config"
        body = '{ "endpoint" : "tcp://127.0.0.1:9999", "database" : "foo", "ignoreErrors" : 5, "requestTimeout" : 32.2, "connectTimeout" : 51.1, "maxConnectRetries" : 12345, "chunkSize" : 143423232, "autoStart" : true, "adaptivePolling" : false, "autoResync" : true, "includeSystem" : true, "requireFromPresent" : true, "verbose" : true, "connectionRetryWaitTime" : 22.12, "initialSyncMaxWaitTime" : 12.21, "idleMinWaitTime" : 1.4, "idleMaxWaitTime" : 7.3 }'
        doc = ArangoDB.log_put("#{prefix}-applier-config", cmd, :body => body)

        doc.code.should eq(200)
        all = doc.parsed_response
        all["endpoint"].should eq("tcp://127.0.0.1:9999")
        all["database"].should eq("foo")
        all["requestTimeout"].should eq(32.2)
        all["connectTimeout"].should eq(51.1)
        all["ignoreErrors"].should eq(5)
        all["maxConnectRetries"].should eq(12345)
        all["sslProtocol"].should eq(0)
        all["chunkSize"].should eq(143423232)
        all["autoStart"].should eq(true)
        all["adaptivePolling"].should eq(false)
        all["autoResync"].should eq(true)
        all["includeSystem"].should eq(true)
        all["requireFromPresent"].should eq(true)
        all["verbose"].should eq(true)
        all["connectionRetryWaitTime"].should eq(22.12)
        all["initialSyncMaxWaitTime"].should eq(12.21)
        all["idleMinWaitTime"].should eq(1.4)
        all["idleMaxWaitTime"].should eq(7.3)

        # refetch same data
        doc = ArangoDB.log_get("#{prefix}-applier-config", cmd, :body => "")

        doc.code.should eq(200)
        all = doc.parsed_response
        all["endpoint"].should eq("tcp://127.0.0.1:9999")
        all["database"].should eq("foo")
        all["requestTimeout"].should eq(32.2)
        all["connectTimeout"].should eq(51.1)
        all["ignoreErrors"].should eq(5)
        all["maxConnectRetries"].should eq(12345)
        all["sslProtocol"].should eq(0)
        all["chunkSize"].should eq(143423232)
        all["autoStart"].should eq(true)
        all["adaptivePolling"].should eq(false)
        all["autoResync"].should eq(true)
        all["includeSystem"].should eq(true)
        all["requireFromPresent"].should eq(true)
        all["verbose"].should eq(true)
        all["connectionRetryWaitTime"].should eq(22.12)
        all["initialSyncMaxWaitTime"].should eq(12.21)
        all["idleMinWaitTime"].should eq(1.4)
        all["idleMaxWaitTime"].should eq(7.3)


        body = '{ "endpoint" : "ssl://127.0.0.1:12345", "database" : "bar", "ignoreErrors" : 2, "requestTimeout" : 12.5, "connectTimeout" : 26.3, "maxConnectRetries" : 12, "chunkSize" : 1234567, "autoStart" : false, "adaptivePolling" : true, "autoResync" : false, "includeSystem" : false, "requireFromPresent" : false, "verbose" : false, "connectionRetryWaitTime" : 2.5, "initialSyncMaxWaitTime" : 4.3, "idleMinWaitTime" : 0.22, "idleMaxWaitTime" : 3.5 }'
        doc = ArangoDB.log_put("#{prefix}-applier-config", cmd, :body => body)

        doc.code.should eq(200)
        all = doc.parsed_response
        all["endpoint"].should eq("ssl://127.0.0.1:12345")
        all["database"].should eq("bar")
        all["requestTimeout"].should eq(12.5)
        all["connectTimeout"].should eq(26.3)
        all["ignoreErrors"].should eq(2)
        all["maxConnectRetries"].should eq(12)
        all["sslProtocol"].should eq(0)
        all["chunkSize"].should eq(1234567)
        all["autoStart"].should eq(false)
        all["adaptivePolling"].should eq(true)
        all["autoResync"].should eq(false)
        all["includeSystem"].should eq(false)
        all["requireFromPresent"].should eq(false)
        all["verbose"].should eq(false)
        all["connectionRetryWaitTime"].should eq(2.5)
        all["initialSyncMaxWaitTime"].should eq(4.3)
        all["idleMinWaitTime"].should eq(0.22)
        all["idleMaxWaitTime"].should eq(3.5)

        # refetch same data
        doc = ArangoDB.log_get("#{prefix}-applier-config", cmd, :body => "")

        doc.code.should eq(200)
        all = doc.parsed_response
        all["endpoint"].should eq("ssl://127.0.0.1:12345")
        all["database"].should eq("bar")
        all["requestTimeout"].should eq(12.5)
        all["connectTimeout"].should eq(26.3)
        all["ignoreErrors"].should eq(2)
        all["maxConnectRetries"].should eq(12)
        all["sslProtocol"].should eq(0)
        all["chunkSize"].should eq(1234567)
        all["autoStart"].should eq(false)
        all["adaptivePolling"].should eq(true)
        all["autoResync"].should eq(false)
        all["includeSystem"].should eq(false)
        all["requireFromPresent"].should eq(false)
        all["verbose"].should eq(false)
        all["connectionRetryWaitTime"].should eq(2.5)
        all["initialSyncMaxWaitTime"].should eq(4.3)
        all["idleMinWaitTime"].should eq(0.22)
        all["idleMaxWaitTime"].should eq(3.5)
      end

################################################################################
## state
################################################################################

      it "checks the applier state again 2" do
        # fetch state
        cmd = api + "/applier-state"
        doc = ArangoDB.log_get("#{prefix}-applier-state", cmd, :body => "")

        doc.code.should eq(200)
        all = doc.parsed_response
        all.should have_key('state')
        all.should have_key('server')

        state = all['state']
        state['running'].should eq(false)
        state.should have_key("lastAppliedContinuousTick")
        state.should have_key("lastProcessedContinuousTick")
        state.should have_key("lastAvailableContinuousTick")
        state.should have_key("safeResumeTick")

        state.should have_key("progress")
        progress = state['progress']
        progress.should have_key("time")
        progress['time'].should match(/^(\d+-\d+-\d+T\d+:\d+:\d+Z)?$/)
        progress.should have_key("failedConnects")

        state.should have_key("totalRequests")
        state.should have_key("totalFailedConnects")
        state.should have_key("totalEvents")
        state.should have_key("totalOperationsExcluded")

        state.should have_key("lastError")
        lastError = state["lastError"]
        lastError.should have_key("errorNum")
        state.should have_key("time")
        state['time'].should match(/^\d+-\d+-\d+T\d+:\d+:\d+Z$/)
      end
    end

################################################################################
## logger
################################################################################

    context "dealing with the logger 2" do
      before do
        ArangoDB.drop_collection("UnitTestsReplication")
      end

      after do
        ArangoDB.drop_collection("UnitTestsReplication")
      end

################################################################################
## state
################################################################################

      it "checks the state" do
        # fetch state
        cmd = api + "/logger-state"
        doc = ArangoDB.log_get("#{prefix}-logger-state", cmd, :body => "")

        doc.code.should eq(200)

        all = doc.parsed_response
        all.should have_key('state')
        all.should have_key('server')
        all.should have_key('clients')

        state = all['state']
        state['running'].should eq(true)
        state['lastLogTick'].should match(/^\d+$/)
        state['time'].should match(/^\d+-\d+-\d+T\d+:\d+:\d+Z$/)

        server = all['server']
        server['serverId'].should match(/^\d+$/)
        server.should have_key('version')
      end

################################################################################
## follow
################################################################################

      it "fetches the empty follow log 2" do
        while 1
          cmd = api + "/logger-state"
          doc = ArangoDB.log_get("#{prefix}-follow-empty", cmd, :body => "")
          doc.code.should eq(200)
          doc.parsed_response["state"]["running"].should eq(true)
          fromTick = doc.parsed_response["state"]["lastLogTick"]

          cmd = api + "/logger-follow?from=" + fromTick
          doc = ArangoDB.log_get("#{prefix}-follow-empty", cmd, :body => "", :format => :plain)

          if doc.code != 204
            # someone else did something else
            doc.code.should eq(200)
            # sleep for a second and try again
            sleep 1
          else
            doc.code.should eq(204)

# doc.headers["x-arango-replication-checkmore"].should eq("false")
            doc.headers["x-arango-replication-lastincluded"].should match(/^\d+$/)
            doc.headers["x-arango-replication-lastincluded"].should eq("0")
            doc.headers["content-type"].should eq("application/x-arango-dump; charset=utf-8")

            body = doc.response.body
            body.should eq(nil)
            break
          end
        end
      end

      it "fetches a create collection action from the follow log 2" do
        ArangoDB.drop_collection("UnitTestsReplication", "UnitTestDB")

        sleep 5

        cmd = api + "/logger-state"
        doc = ArangoDB.log_get("#{prefix}-follow-create-collection", cmd, :body => "")
        doc.code.should eq(200)
        doc.parsed_response["state"]["running"].should eq(true)
        fromTick = doc.parsed_response["state"]["lastLogTick"]

        cid = ArangoDB.create_collection("UnitTestsReplication", true, 2, "UnitTestDB")

        sleep 5

        cmd = api + "/logger-follow?from=" + fromTick
        doc = ArangoDB.log_get("#{prefix}-follow-create-collection", cmd, :body => "", :format => :plain)
        doc.code.should eq(200)

        doc.headers["x-arango-replication-lastincluded"].should match(/^\d+$/)
        doc.headers["x-arango-replication-lastincluded"].should_not eq("0")
        doc.headers["content-type"].should eq("application/x-arango-dump; charset=utf-8")

        body = doc.response.body

        while 1
          position = body.index("\n")

          break if position == nil

          part = body.slice(0, position)
          document = JSON.parse(part)

          if document["type"] == 2000 and document["cid"] == cid
            document.should have_key("tick")
            document.should have_key("type")
            document.should have_key("cid")
            document.should have_key("cname")
            document.should have_key("data")

            document["tick"].should match(/^\d+$/)
            document["tick"].to_i.should >= fromTick.to_i
            document["type"].should eq(2000)
            document["cid"].should eq(cid)
            document["cid"].should be_kind_of(String)
            document["cname"].should eq("UnitTestsReplication")

            c = document["data"]
            c.should have_key("version")
            c["type"].should eq(2)
            c["cid"].should eq(cid)
            c["cid"].should be_kind_of(String)
            c["deleted"].should eq(false)
            c["name"].should eq("UnitTestsReplication")
            c["waitForSync"].should eq(true)
          end

          body = body.slice(position + 1, body.length)
        end

      end

      it "fetches some collection operations from the follow log 2" do
        ArangoDB.drop_collection("UnitTestsReplication", "UnitTestDB")

        sleep 5

        cmd = api + "/logger-state"
        doc = ArangoDB.log_get("#{prefix}-follow-collection", cmd, :body => "")
        doc.code.should eq(200)
        doc.parsed_response["state"]["running"].should eq(true)
        fromTick = doc.parsed_response["state"]["lastLogTick"]

        # create collection
        cid = ArangoDB.create_collection("UnitTestsReplication", true, 2, "UnitTestDB")

        # create document
        cmd = "/_db/UnitTestDB/_api/document?collection=UnitTestsReplication"
        body = "{ \"_key\" : \"test\", \"test\" : false }"
        doc = ArangoDB.log_post("#{prefix}-follow-collection", cmd, :body => body)
        doc.code.should eq(201)
        rev = doc.parsed_response["_rev"]

        # delete document
        cmd = "/_db/UnitTestDB/_api/document/UnitTestsReplication/test"
        doc = ArangoDB.log_delete("#{prefix}-follow-collection", cmd)
        doc.code.should eq(200)

        # drop collection
        cmd = "/_db/UnitTestDB/_api/collection/UnitTestsReplication"
        doc = ArangoDB.log_delete("#{prefix}-follow-collection", cmd)
        doc.code.should eq(200)

        sleep 5

        cmd = api + "/logger-follow?from=" + fromTick
        doc = ArangoDB.log_get("#{prefix}-follow-create-collection", cmd, :body => "", :format => :plain)
        doc.code.should eq(200)

        doc.headers["x-arango-replication-lastincluded"].should match(/^\d+$/)
        doc.headers["x-arango-replication-lastincluded"].should_not eq("0")
        doc.headers["content-type"].should eq("application/x-arango-dump; charset=utf-8")


        body = doc.response.body
        i = 0
        while 1
          position = body.index("\n")

          break if position == nil

          part = body.slice(0, position)
          document = JSON.parse(part)

          if i == 0
            if document["type"] == 2000 and document["cid"] == cid
              # create collection
              document.should have_key("tick")
              document.should have_key("type")
              document.should have_key("cid")
              document.should have_key("cname")
              document.should have_key("data")

              document["tick"].should match(/^\d+$/)
              document["tick"].to_i.should >= fromTick.to_i
              document["type"].should eq(2000)
              document["cid"].should eq(cid)
              document["cid"].should be_kind_of(String)
              document["cname"].should eq("UnitTestsReplication")

              c = document["data"]
              c.should have_key("version")
              c["type"].should eq(2)
              c["cid"].should eq(cid)
              c["cid"].should be_kind_of(String)
              c["deleted"].should eq(false)
              c["name"].should eq("UnitTestsReplication")
              c["waitForSync"].should eq(true)

              i = i + 1
            end

          elsif i == 1 and document["type"] == 2300 and document["cid"] == cid
            # create document
            document.should have_key("tick")
            document.should have_key("type")
            document.should have_key("cid")

            document["tick"].should match(/^\d+$/)
            document["tick"].to_i.should >= fromTick.to_i
            document["type"].should eq(2300)
            document["cid"].should eq(cid)
            document["cid"].should be_kind_of(String)
            document["data"]["_key"].should eq("test")
            document["data"]["_rev"].should match(/^[a-zA-Z0-9_\-]+$/)
            document["data"]["_rev"].should_not eq("0")
            document["data"]["test"].should eq(false)

            i = i + 1
          elsif i == 2 and document["type"] == 2302 and document["cid"] == cid
            # delete document
            document.should have_key("tick")
            document.should have_key("type")
            document.should have_key("cid")

            document["tick"].should match(/^\d+$/)
            document["tick"].to_i.should >= fromTick.to_i
            document["type"].should eq(2302)
            document["cid"].should eq(cid)
            document["cid"].should be_kind_of(String)
            document["data"]["_key"].should eq("test")

            i = i + 1
          elsif i == 3 and document["type"] == 2001 and document["cid"] == cid
            # drop collection
            document.should have_key("tick")
            document.should have_key("type")
            document.should have_key("cid")

            document["tick"].should match(/^\d+$/)
            document["tick"].to_i.should >= fromTick.to_i
            document["type"].should eq(2001)
            document["cid"].should eq(cid)
            document["cid"].should be_kind_of(String)

            i = i + 1
          end

          body = body.slice(position + 1, body.length)
        end

      end

    end

################################################################################
## inventory / dump
################################################################################

    context "dealing with the initial dump" do

      before do
        ArangoDB.drop_collection("UnitTestsReplication", "UnitTestDB")
        ArangoDB.drop_collection("UnitTestsReplication2", "UnitTestDB")
        doc = ArangoDB.post(api + "/batch", :body => "{}")
        doc.code.should eq(200)
        @batchId = doc.parsed_response['id']
        @batchId.should  match(/^\d+$/)
      end

      after do
        ArangoDB.delete(api + "/batch/#{@batchId}", :body => "")
        ArangoDB.drop_collection("UnitTestsReplication", "UnitTestDB")
        ArangoDB.drop_collection("UnitTestsReplication2", "UnitTestDB")
      end

################################################################################
## inventory
################################################################################

      it "checks the initial inventory 2" do
        cmd = api + "/inventory?includeSystem=false&batchId=#{@batchId}"
        doc = ArangoDB.log_get("#{prefix}-inventory", cmd, :body => "")

        doc.code.should eq(200)
        all = doc.parsed_response
        all.should have_key('collections')
        all.should have_key('state')

        collections = all["collections"]
        filtered = [ ]
        collections.each { |collection|
          if [ "UnitTestsReplication", "UnitTestsReplication2" ].include? collection["parameters"]["name"]
            filtered.push collection
          end
        }
        filtered.should eq([ ])
        state = all['state']
        state['running'].should eq(true)
        state['lastLogTick'].should match(/^\d+$/)
        state['time'].should match(/^\d+-\d+-\d+T\d+:\d+:\d+Z$/)
      end

      it "checks the initial inventory for non-system collections 2" do
        cmd = api + "/inventory?includeSystem=false&batchId=#{@batchId}"
        doc = ArangoDB.log_get("#{prefix}-inventory-system", cmd, :body => "")

        doc.code.should eq(200)
        all = doc.parsed_response
        all.should have_key('collections')
        all.should have_key('state')

        collections = all["collections"]
        collections.each { |collection|
          collection["parameters"]["name"].should_not match(/^_/)
        }
      end

      it "checks the initial inventory for system collections 2" do
        cmd = api + "/inventory?includeSystem=true&batchId=#{@batchId}"
        doc = ArangoDB.log_get("#{prefix}-inventory-system", cmd, :body => "")

        doc.code.should eq(200)
        all = doc.parsed_response
        all.should have_key('collections')
        all.should have_key('state')

        collections = all["collections"]
        systemCollections = 0
        collections.each { |collection|
          if collection["parameters"]["name"].match(/^_/)
            systemCollections = systemCollections + 1
          end
        }

        systemCollections.should_not eq(0)
      end

      it "checks the inventory after creating collections 2" do
        cid = ArangoDB.create_collection("UnitTestsReplication", false, 2, "UnitTestDB")
        cid2 = ArangoDB.create_collection("UnitTestsReplication2", true, 3, "UnitTestDB")

        cmd = api + "/inventory?includeSystem=false&batchId=#{@batchId}"
        doc = ArangoDB.log_get("#{prefix}-inventory-create", cmd, :body => "")
        doc.code.should eq(200)

        all = doc.parsed_response
        all.should have_key('collections')
        all.should have_key('state')

        state = all['state']
        state['running'].should eq(true)
        state['lastLogTick'].should match(/^\d+$/)
        state['time'].should match(/^\d+-\d+-\d+T\d+:\d+:\d+Z$/)

        collections = all['collections']
        filtered = [ ]
        collections.each { |collection|
          if [ "UnitTestsReplication", "UnitTestsReplication2" ].include? collection["parameters"]["name"]
            filtered.push collection
          end
        }
        filtered.length.should eq(2)

        # first collection
        c = filtered[0]
        c.should have_key("parameters")
        c.should have_key("indexes")

        parameters = c['parameters']
        parameters.should have_key("version")
        parameters["version"].should be_kind_of(Integer)
        parameters["type"].should be_kind_of(Integer)
        parameters["type"].should eq(2)
        parameters["cid"].should eq(cid)
        parameters["cid"].should be_kind_of(String)
        parameters["deleted"].should eq(false)
        parameters["name"].should eq("UnitTestsReplication")
        parameters["waitForSync"].should eq(false)

        c['indexes'].should eq([ ])

        # second collection
        c = filtered[1]
        c.should have_key("parameters")
        c.should have_key("indexes")

        parameters = c['parameters']
        parameters.should have_key("version")
        parameters["version"].should be_kind_of(Integer)
        parameters["type"].should be_kind_of(Integer)
        parameters["type"].should eq(3)
        parameters["cid"].should eq(cid2)
        parameters["cid"].should be_kind_of(String)
        parameters["deleted"].should eq(false)
        parameters["name"].should eq("UnitTestsReplication2")
        parameters["waitForSync"].should eq(true)

        c['indexes'].should eq([ ])
      end

      it "checks the inventory with indexes 2" do
        cid = ArangoDB.create_collection("UnitTestsReplication", false, 2, "UnitTestDB")
        cid2 = ArangoDB.create_collection("UnitTestsReplication2", false, 2, "UnitTestDB")

        idxUrl = "/_db/UnitTestDB/_api/index"

        body = "{ \"type\" : \"hash\", \"unique\" : false, \"fields\" : [ \"a\", \"b\" ] }"
        doc = ArangoDB.log_post("#{prefix}-inventory2", "#{idxUrl}?collection=UnitTestsReplication", :body => body)
        doc.code.should eq(201)

        body = "{ \"type\" : \"skiplist\", \"unique\" : false, \"fields\" : [ \"c\" ] }"
        doc = ArangoDB.log_post("#{prefix}-inventory2", "#{idxUrl}?collection=UnitTestsReplication", :body => body)
        doc.code.should eq(201)

        # create indexes for second collection
        body = "{ \"type\" : \"geo\", \"unique\" : false, \"fields\" : [ \"a\", \"b\" ] }"
        doc = ArangoDB.log_post("#{prefix}-inventory2", "#{idxUrl}?collection=UnitTestsReplication2", :body => body)
        doc.code.should eq(201)

        body = "{ \"type\" : \"skiplist\", \"unique\" : true, \"fields\" : [ \"d\" ] }"
        doc = ArangoDB.log_post("#{prefix}-inventory2", "#{idxUrl}?collection=UnitTestsReplication2", :body => body)
        doc.code.should eq(201)

        body = "{ \"type\" : \"fulltext\", \"minLength\" : 8, \"fields\" : [ \"ff\" ] }"
        doc = ArangoDB.log_post("#{prefix}-inventory2", "#{idxUrl}?collection=UnitTestsReplication2", :body => body)
        doc.code.should eq(201)

        cmd = api + "/inventory?batchId=#{@batchId}"
        doc = ArangoDB.log_get("#{prefix}-inventory2", cmd, :body => "")
        doc.code.should eq(200)

        all = doc.parsed_response
        all.should have_key('collections')
        all.should have_key('state')

        state = all['state']
        state['running'].should eq(true)
        state['lastLogTick'].should match(/^\d+$/)
        state['time'].should match(/^\d+-\d+-\d+T\d+:\d+:\d+Z$/)

        collections = all['collections']
        filtered = [ ]
        collections.each { |collection|
          if [ "UnitTestsReplication", "UnitTestsReplication2" ].include? collection["parameters"]["name"]
            filtered.push collection
          end
        }
        filtered.length.should eq(2)

        # first collection
        c = filtered[0]
        c.should have_key("parameters")
        c.should have_key("indexes")

        parameters = c['parameters']
        parameters.should have_key("version")
        parameters["version"].should be_kind_of(Integer)
        parameters["type"].should be_kind_of(Integer)
        parameters["type"].should eq(2)
        parameters["cid"].should eq(cid)
        parameters["cid"].should be_kind_of(String)
        parameters["deleted"].should eq(false)
        parameters["name"].should eq("UnitTestsReplication")
        parameters["waitForSync"].should eq(false)

        indexes = c['indexes']
        indexes.length.should eq(2)

        idx = indexes[0]
        idx["id"].should match(/^\d+$/)
        idx["type"].should eq("hash")
        idx["unique"].should eq(false)
        idx["fields"].should eq([ "a", "b" ])

        idx = indexes[1]
        idx["id"].should match(/^\d+$/)
        idx["type"].should eq("skiplist")
        idx["unique"].should eq(false)
        idx["fields"].should eq([ "c" ])

        # second collection
        c = filtered[1]
        c.should have_key("parameters")
        c.should have_key("indexes")

        parameters = c['parameters']
        parameters.should have_key("version")
        parameters["version"].should be_kind_of(Integer)
        parameters["type"].should be_kind_of(Integer)
        parameters["type"].should eq(2)
        parameters["cid"].should eq(cid2)
        parameters["cid"].should be_kind_of(String)
        parameters["deleted"].should eq(false)
        parameters["name"].should eq("UnitTestsReplication2")
        parameters["waitForSync"].should eq(false)

        indexes = c['indexes']
        indexes.length.should eq(3)

        idx = indexes[0]
        idx["id"].should match(/^\d+$/)
        idx["type"].should eq("geo")
        idx["unique"].should eq(false)
        idx["fields"].should eq([ "a", "b" ])

        idx = indexes[1]
        idx["id"].should match(/^\d+$/)
        idx["type"].should eq("skiplist")
        idx["unique"].should eq(true)
        idx["fields"].should eq([ "d" ])
        
        idx = indexes[2]
        idx["id"].should match(/^\d+$/)
        idx["type"].should eq("fulltext")
        idx["unique"].should eq(false)
        idx["minLength"].should eq(8)
        idx["fields"].should eq([ "ff" ])
      end

################################################################################
## dump
################################################################################

      it "checks the dump for an empty collection 2" do
        cid = ArangoDB.create_collection("UnitTestsReplication", false, 2, "UnitTestDB")

        doc = ArangoDB.log_put("#{prefix}-deleted", "/_admin/wal/flush?waitForSync=true&waitForCollector=true", :body => "")
        doc.code.should eq(200)

        cmd = api + "/dump?collection=UnitTestsReplication&batchId=#{@batchId}"
        doc = ArangoDB.log_get("#{prefix}-dump-empty", cmd, :body => "")

        doc.code.should eq(204)

        doc.headers["x-arango-replication-checkmore"].should eq("false")
        doc.headers["x-arango-replication-lastincluded"].should eq("0")
        doc.headers["content-type"].should eq("application/x-arango-dump; charset=utf-8")
        doc.response.body.should eq(nil)
      end

      it "checks the dump for a non-empty collection 2" do
        cid = ArangoDB.create_collection("UnitTestsReplication", false, 2 , "UnitTestDB")

        (0...100).each{|i|
          body = "{ \"_key\" : \"test" + i.to_s + "\", \"test\" : " + i.to_s + " }"
          doc = ArangoDB.post("/_db/UnitTestDB/_api/document?collection=UnitTestsReplication", :body => body)
          doc.code.should eq(202)
        }

        ArangoDB.delete(api + "/batch/#{@batchId}", :body => "")
        doc0 = ArangoDB.post(api + "/batch", :body => "{}")
        @batchId = doc0.parsed_response["id"]
        @batchId.should  match(/^\d+$/)

        cmd = api + "/dump?collection=UnitTestsReplication&batchId=#{@batchId}"
        doc = ArangoDB.log_get("#{prefix}-dump-non-empty", cmd, :body => "", :format => :plain)

        doc.code.should eq(200)

        doc.headers["x-arango-replication-checkmore"].should eq("false")
        doc.headers["x-arango-replication-lastincluded"].should match(/^\d+$/)
        doc.headers["x-arango-replication-lastincluded"].should_not eq("0")
        doc.headers["content-type"].should eq("application/x-arango-dump; charset=utf-8")

        body = doc.response.body
        i = 0
        while 1
          position = body.index("\n")

          break if position == nil

          part = body.slice(0, position)

          doc = JSON.parse(part)
          doc['type'].should eq(2300)
          doc['data']['_key'].should match(/^test[0-9]+$/)
          doc["data"]["_rev"].should match(/^[a-zA-Z0-9_\-]+$/)
          doc['data'].should have_key("test")

          body = body.slice(position + 1, body.length)
          i = i + 1
        end

        i.should eq(100)
      end

      it "checks the dump for a non-empty collection, small chunkSize 2" do
        cid = ArangoDB.create_collection("UnitTestsReplication", false, 2, "UnitTestDB")

        (0...100).each{|i|
          body = "{ \"_key\" : \"test" + i.to_s + "\", \"test\" : " + i.to_s + " }"
          doc = ArangoDB.post("/_db/UnitTestDB/_api/document?collection=UnitTestsReplication", :body => body)
          doc.code.should eq(202)
        }

        doc = ArangoDB.log_put("#{prefix}-deleted", "/_admin/wal/flush?waitForSync=true&waitForCollector=true", :body => "")
        doc.code.should eq(200)

        ArangoDB.delete(api + "/batch/#{@batchId}", :body => "")
        doc0 = ArangoDB.post(api + "/batch", :body => "{}")
        @batchId = doc0.parsed_response["id"]
        @batchId.should  match(/^\d+$/)

        cmd = api + "/dump?collection=UnitTestsReplication&chunkSize=1024&batchId=#{@batchId}"
        doc = ArangoDB.log_get("#{prefix}-dump-non-empty", cmd, :body => "", :format => :plain)

        doc.code.should eq(200)

        doc.headers["x-arango-replication-checkmore"].should eq("true")
        doc.headers["x-arango-replication-lastincluded"].should match(/^\d+$/)
        doc.headers["x-arango-replication-lastincluded"].should_not eq("0")
        doc.headers["content-type"].should eq("application/x-arango-dump; charset=utf-8")

        body = doc.response.body
        i = 0
        while 1
          position = body.index("\n")

          break if position == nil

          part = body.slice(0, position)

          doc = JSON.parse(part)
          doc['type'].should eq(2300)
          doc['data']['_key'].should match(/^test[0-9]+$/)
          doc["data"]["_rev"].should match(/^[a-zA-Z0-9_\-]+$/)
          doc['data'].should have_key('test')

          body = body.slice(position + 1, body.length)
          i = i + 1
        end

        i.should be < 100
      end


      it "checks the dump for an edge collection 2" do
        cid = ArangoDB.create_collection("UnitTestsReplication", false, 2, "UnitTestDB")
        cid2 = ArangoDB.create_collection("UnitTestsReplication2", false, 3, "UnitTestDB")

        (0...100).each{|i|
          body = "{ \"_key\" : \"test" + i.to_s + "\", \"_from\" : \"UnitTestsReplication/foo\", \"_to\" : \"UnitTestsReplication/bar\", \"test1\" : " + i.to_s + ", \"test2\" : false, \"test3\" : [ ], \"test4\" : { } }"
          doc = ArangoDB.post("/_db/UnitTestDB/_api/document?collection=UnitTestsReplication2", :body => body)
          doc.code.should eq(202)
        }

        doc = ArangoDB.log_put("#{prefix}-deleted", "/_admin/wal/flush?waitForSync=true&waitForCollector=true", :body => "")
        doc.code.should eq(200)

        ArangoDB.delete(api + "/batch/#{@batchId}", :body => "")
        doc0 = ArangoDB.post(api + "/batch", :body => "{}")
        @batchId = doc0.parsed_response["id"]
        @batchId.should  match(/^\d+$/)

        cmd = api + "/dump?collection=UnitTestsReplication2&chunkSize=65536&batchId=#{@batchId}"
        doc = ArangoDB.log_get("#{prefix}-dump-edge", cmd, :body => "", :format => :plain)

        doc.code.should eq(200)

        doc.headers["x-arango-replication-checkmore"].should eq("false")
        doc.headers["x-arango-replication-lastincluded"].should match(/^\d+$/)
        doc.headers["x-arango-replication-lastincluded"].should_not eq("0")
        doc.headers["content-type"].should eq("application/x-arango-dump; charset=utf-8")

        body = doc.response.body
        i = 0
        while 1
          position = body.index("\n")

          break if position == nil

          part = body.slice(0, position)

          document = JSON.parse(part)
          document['type'].should eq(2300)
          document['data']['_key'].should match(/^test[0-9]+$/)
          document["data"]["_rev"].should match(/^[a-zA-Z0-9_\-]+$/)
          document['data']['_from'].should eq("UnitTestsReplication/foo")
          document['data']['_to'].should eq("UnitTestsReplication/bar")
          document['data'].should have_key('test1')
          document['data']['test2'].should eq(false)
          document['data']['test3'].should eq([ ])
          document['data']['test4'].should eq({ })

          body = body.slice(position + 1, body.length)
          i = i + 1
        end

        i.should eq(100)
      end

      it "checks the dump for an edge collection, small chunkSize 2" do
        cid = ArangoDB.create_collection("UnitTestsReplication", false, 2, "UnitTestDB")
        cid2 = ArangoDB.create_collection("UnitTestsReplication2", false, 3, "UnitTestDB")

        (0...100).each{|i|
          body = "{ \"_key\" : \"test" + i.to_s + "\", \"_from\" : \"UnitTestsReplication/foo\", \"_to\" : \"UnitTestsReplication/bar\", \"test1\" : " + i.to_s + ", \"test2\" : false, \"test3\" : [ ], \"test4\" : { } }"
          doc = ArangoDB.post("/_db/UnitTestDB/_api/document?collection=UnitTestsReplication2", :body => body)
          doc.code.should eq(202)
        }

        doc = ArangoDB.log_put("#{prefix}-deleted", "/_admin/wal/flush?waitForSync=true&waitForCollector=true", :body => "")
        doc.code.should eq(200)

        ArangoDB.delete(api + "/batch/#{@batchId}", :body => "")
        doc0 = ArangoDB.post(api + "/batch", :body => "{}")
        @batchId = doc0.parsed_response["id"]
        @batchId.should  match(/^\d+$/)

        cmd = api + "/dump?collection=UnitTestsReplication2&chunkSize=1024&batchId=#{@batchId}"
        doc = ArangoDB.log_get("#{prefix}-dump-edge", cmd, :body => "", :format => :plain)

        doc.code.should eq(200)

        doc.headers["x-arango-replication-checkmore"].should eq("true")
        doc.headers["x-arango-replication-lastincluded"].should match(/^\d+$/)
        doc.headers["x-arango-replication-lastincluded"].should_not eq("0")
        doc.headers["content-type"].should eq("application/x-arango-dump; charset=utf-8")

        body = doc.response.body
        i = 0
        while 1
          position = body.index("\n")

          break if position == nil

          part = body.slice(0, position)

          document = JSON.parse(part)
          document['type'].should eq(2300)
          document['data']['_key'].should match(/^test[0-9]+$/)
          document['data']['_rev'].should match(/^[a-zA-Z0-9_\-]+$/)
          document['data']['_from'].should eq("UnitTestsReplication/foo")
          document['data']['_to'].should eq("UnitTestsReplication/bar")
          document['data'].should have_key('test1')
          document['data']['test2'].should eq(false)
          document['data']['test3'].should eq([ ])
          document['data']['test4'].should eq({ })

          body = body.slice(position + 1, body.length)
          i = i + 1
        end

        i.should be < 100
      end

      it "checks the dump for a collection with deleted documents 2" do
        cid = ArangoDB.create_collection("UnitTestsReplication", false, 2, "UnitTestDB")

        doc = ArangoDB.log_put("#{prefix}-deleted", "/_admin/wal/flush?waitForSync=true&waitForCollector=true", :body => "")
        doc.code.should eq(200)

        (0...10).each{|i|
          body = "{ \"_key\" : \"test" + i.to_s + "\", \"test\" : " + i.to_s + " }"
          doc = ArangoDB.post("/_db/UnitTestDB/_api/document?collection=UnitTestsReplication", :body => body)
          doc.code.should eq(202)

          doc = ArangoDB.delete("/_db/UnitTestDB/_api/document/UnitTestsReplication/test" + i.to_s, :body => body)
          doc.code.should eq(202)
        }

        doc = ArangoDB.log_put("#{prefix}-deleted", "/_admin/wal/flush?waitForSync=true&waitForCollector=true", :body => "")
        doc.code.should eq(200)

        ArangoDB.delete(api + "/batch/#{@batchId}", :body => "")
        doc0 = ArangoDB.post(api + "/batch", :body => "{}")
        @batchId = doc0.parsed_response["id"]
        @batchId.should  match(/^\d+$/)

        cmd = api + "/dump?collection=UnitTestsReplication&batchId=#{@batchId}"
        doc = ArangoDB.log_get("#{prefix}-deleted", cmd, :body => "", :format => :plain)
        doc.code.should eq(204)

        doc.headers["x-arango-replication-checkmore"].should eq("false")
        doc.headers["x-arango-replication-lastincluded"].should eq("0")
        doc.headers["content-type"].should eq("application/x-arango-dump; charset=utf-8")
      end

      it "checks the dump for a truncated collection 2" do
        cid = ArangoDB.create_collection("UnitTestsReplication", false, 2, "UnitTestDB")

        (0...10).each{|i|
          body = "{ \"_key\" : \"test" + i.to_s + "\", \"test\" : " + i.to_s + " }"
          doc = ArangoDB.post("/_db/UnitTestDB/_api/document?collection=UnitTestsReplication", :body => body)
          doc.code.should eq(202)
        }

        # truncate
        cmd = "/_db/UnitTestDB/_api/collection/UnitTestsReplication/truncate"
        doc = ArangoDB.log_put("#{prefix}-truncated", cmd, :body => "", :format => :plain)
        doc.code.should eq(200)

        doc = ArangoDB.log_put("#{prefix}-deleted", "/_admin/wal/flush?waitForSync=true&waitForCollector=true", :body => "")
        doc.code.should eq(200)

        ArangoDB.delete(api + "/batch/#{@batchId}", :body => "")
        doc0 = ArangoDB.post(api + "/batch", :body => "{}")
        @batchId = doc0.parsed_response["id"]
        @batchId.should  match(/^\d+$/)

        cmd = api + "/dump?collection=UnitTestsReplication&batchId=#{@batchId}"
        doc = ArangoDB.log_get("#{prefix}-truncated", cmd, :body => "", :format => :plain)
        doc.code.should eq(204)

        doc.headers["x-arango-replication-checkmore"].should eq("false")
        doc.headers["x-arango-replication-lastincluded"].should eq("0")
        doc.headers["content-type"].should eq("application/x-arango-dump; charset=utf-8")
      end

      it "checks the dump for a non-empty collection, 3.0 mode 2" do
        cid = ArangoDB.create_collection("UnitTestsReplication", false, 2, "UnitTestDB")

        (0...100).each{|i|
          body = "{ \"_key\" : \"test" + i.to_s + "\", \"test\" : " + i.to_s + " }"
          doc = ArangoDB.post("/_db/UnitTestDB/_api/document?collection=UnitTestsReplication", :body => body)
          doc.code.should eq(202)
        }

        ArangoDB.delete(api + "/batch/#{@batchId}", :body => "")
        doc0 = ArangoDB.post(api + "/batch", :body => "{}")
        @batchId = doc0.parsed_response["id"]
        @batchId.should  match(/^\d+$/)

        cmd = api + "/dump?collection=UnitTestsReplication&batchId=#{@batchId}"
        doc = ArangoDB.log_get("#{prefix}-dump-non-empty", cmd, :body => "", :format => :plain)

        doc.code.should eq(200)

        body = doc.response.body
        i = 0
        while 1
          position = body.index("\n")

          break if position == nil

          part = body.slice(0, position)

          doc = JSON.parse(part)
          doc['type'].should eq(2300)
          doc.should_not have_key("key")
          doc.should_not have_key("rev")
          doc['data']['_key'].should match(/^test[0-9]+$/)
          doc['data']['_rev'].should match(/^[a-zA-Z0-9_\-]+$/)
          doc['data'].should have_key('test')

          body = body.slice(position + 1, body.length)
          i = i + 1
        end

        i.should eq(100)
      end

      it "fetches incremental parts of a collection dump 2" do
        cid = ArangoDB.create_collection("UnitTestsReplication", false, 2, "UnitTestDB")

        (0...10).each{|i|
          body = "{ \"_key\" : \"test" + i.to_s + "\", \"test\" : " + i.to_s + " }"
          doc = ArangoDB.post("/_db/UnitTestDB/_api/document?collection=UnitTestsReplication", :body => body)
          doc.code.should eq(202)
        }

        doc = ArangoDB.log_put("#{prefix}-deleted", "/_admin/wal/flush?waitForSync=true&waitForCollector=true", :body => "")
        doc.code.should eq(200)

        fromTick = "0"

        ArangoDB.delete(api + "/batch/#{@batchId}", :body => "")
        doc0 = ArangoDB.post(api + "/batch", :body => "{}")
        @batchId = doc0.parsed_response["id"]
        @batchId.should  match(/^\d+$/)

        (0...10).each{|i|
          cmd = api + "/dump?collection=UnitTestsReplication&from=" + fromTick + "&chunkSize=1&batchId=#{@batchId}"
          doc = ArangoDB.log_get("#{prefix}-incremental", cmd, :body => "", :format => :plain)
          doc.code.should eq(200)

          if i == 9
            doc.headers["x-arango-replication-checkmore"].should eq("false")
          else
            doc.headers["x-arango-replication-checkmore"].should eq("true")
          end

          doc.headers["x-arango-replication-lastincluded"].should match(/^\d+$/)
          doc.headers["x-arango-replication-lastincluded"].should_not eq("0")
          doc.headers["x-arango-replication-lastincluded"].to_i.should >= fromTick.to_i
          doc.headers["content-type"].should eq("application/x-arango-dump; charset=utf-8")

          fromTick = doc.headers["x-arango-replication-lastincluded"]

          body = doc.response.body
          document = JSON.parse(body)
          document['type'].should eq(2300)
          document['data']['_key'].should match(/^test[0-9]+$/)
          document['data']['_rev'].should match(/^[a-zA-Z0-9_\-]+$/)
          document['data'].should have_key('test')
        }
      end

    end

  end

end
