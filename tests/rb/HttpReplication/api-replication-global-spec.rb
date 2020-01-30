# coding: utf-8

require 'rspec'
require 'json'
require 'arangodb.rb'

describe ArangoDB do

  context "dealing with the replication interface:" do

################################################################################
## applier
################################################################################

    context "dealing with the applier" do

      api = "/_api/replication"
      prefix = "api-replication"

      before do
        ArangoDB.put(api + "/applier-stop?global=true", :body => "")
        ArangoDB.delete(api + "/applier-state?global=true", :body => "")
      end

      after do
        ArangoDB.put(api + "/applier-stop?global=true", :body => "")
        ArangoDB.delete(api + "/applier-state?global=true", :body => "")
      end

################################################################################
## start
################################################################################

      it "starts the applier" do
        cmd = api + "/applier-start?global=true"
        doc = ArangoDB.log_put("#{prefix}-applier-start", cmd, :body => "")
        doc.code.should eq(400) # because configuration is invalid
      end

################################################################################
## stop
################################################################################

      it "stops the applier" do
        cmd = api + "/applier-stop?global=true"
        doc = ArangoDB.log_put("#{prefix}-applier-start", cmd, :body => "")
        doc.code.should eq(200)
      end

################################################################################
## properties
################################################################################

      it "fetches the applier config" do
        cmd = api + "/applier-config?global=true"
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

      it "sets and re-fetches the applier config" do
        cmd = api + "/applier-config?global=true"
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

      it "checks the state" do
        # fetch state
        cmd = api + "/applier-state?global=true"
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
## wal access
################################################################################

    context "dealing with wal access api" do
      api = "/_api/wal"
      prefix = "api-wal"
      
      before do
        ArangoDB.drop_collection("UnitTestsReplication")
      end

      after do
        ArangoDB.drop_collection("UnitTestsReplication")
      end

################################################################################
## state
################################################################################

      it "check the state" do
        # fetch state
        cmd = "/_api/replication/logger-state"
        doc = ArangoDB.log_get("api-replication-logger-state", cmd, :body => "")

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
## tickRanges
################################################################################

      it "fetches the available tick range" do
        # fetch state
        cmd = api + "/range"
        doc = ArangoDB.log_get("#{prefix}-range", cmd, :body => "")

        doc.code.should eq(200)

        result = doc.parsed_response
        result.size.should be > 0 
        
        result.should have_key('tickMin')
        result.should have_key('tickMax')
        result['tickMin'].should match(/^\d+$/)
        result['tickMax'].should match(/^\d+$/)
        result.should have_key('server')                
        result['server'].should have_key('serverId')        
        result['server']['serverId'].should match(/^\d+$/)
      end

################################################################################
## lastTick
################################################################################

      it "fetches the available tick range" do
        # fetch state
        cmd = api + "/lastTick"
        doc = ArangoDB.log_get("#{prefix}-lastTick", cmd, :body => "")

        doc.code.should eq(200)

        result = doc.parsed_response
        result.size.should be > 0 
        
        result.should have_key('tick')
        result['tick'].should match(/^\d+$/)
        result.should have_key('server')                
        result['server'].should have_key('serverId')        
        result['server']['serverId'].should match(/^\d+$/)
      end

################################################################################
## follow
################################################################################

      it "fetches the empty follow log" do
        while 1
          cmd = api + "/lastTick"
          doc = ArangoDB.log_get("#{prefix}-lastTick", cmd, :body => "")
          doc.code.should eq(200)
          fromTick = doc.parsed_response["tick"]

          cmd = api + "/tail?global=true&from=" + fromTick
          doc = ArangoDB.log_get("#{prefix}-tail", cmd, :body => "", :format => :plain)

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
      
      it "tails the WAL with a tick far in the future" do
        cmd = api + "/lastTick"
        doc = ArangoDB.log_get("#{prefix}-lastTick", cmd, :body => "")
        doc.code.should eq(200)
        fromTick = doc.parsed_response["tick"].to_i * 10000000

        cmd = api + "/tail?global=true&from=" + fromTick.to_s
        doc = ArangoDB.log_get("#{prefix}-tail", cmd, :body => "", :format => :plain)

        doc.code.should eq(204)
        doc.headers["x-arango-replication-lastincluded"].should eq("0")
      end

      it "fetches a create collection action from the follow log" do
        cid = 0
        cuid = 0
        doc = {}

        while 1
          ArangoDB.drop_collection("UnitTestsReplication")

          sleep 5

          cmd = api + "/lastTick"
          doc = ArangoDB.log_get("#{prefix}-lastTick", cmd, :body => "")
          doc.code.should eq(200)
          fromTick = doc.parsed_response["tick"]

          cid = ArangoDB.create_collection("UnitTestsReplication")
          cuid = ArangoDB.properties_collection(cid)["globallyUniqueId"]

          sleep 1

          cmd = api + "/tail?global=true&from=" + fromTick
          doc = ArangoDB.log_get("#{prefix}-follow-create-collection", cmd, :body => "", :format => :plain)
          [200, 204].should include(doc.code)
        
          break if doc.headers["x-arango-replication-frompresent"] == "true" and doc.headers["x-arango-replication-lastincluded"] != "0"
        end
        
        doc.headers["x-arango-replication-lastincluded"].should match(/^\d+$/)
        doc.headers["x-arango-replication-lastincluded"].should_not eq("0")
        doc.headers["content-type"].should eq("application/x-arango-dump; charset=utf-8")

        body = doc.response.body

        while 1
          position = body.index("\n")

          break if position == nil

          part = body.slice(0, position)
          document = JSON.parse(part)

          if document["type"] == 2000 and document["cuid"] == cuid
            document.should have_key("tick")
            document.should have_key("type")
            document.should have_key("cuid")
            document.should have_key("data")

            document["tick"].should match(/^\d+$/)
            document["tick"].to_i.should >= fromTick.to_i
            document["type"].should eq(2000)
            document["cuid"].should eq(cuid)

            c = document["data"]
            c.should have_key("version")
            c["type"].should eq(2)
            c["cid"].should eq(cid)
            c["cid"].should be_kind_of(String)
            c["globallyUniqueId"].should eq(cuid)            
            c["deleted"].should eq(false)
            c["name"].should eq("UnitTestsReplication")
            c["waitForSync"].should eq(true)
          end

          body = body.slice(position + 1, body.length)
        end

      end

      it "fetches some collection operations from the follow log" do
        while 1
          ArangoDB.drop_collection("UnitTestsReplication")

          sleep 5

          cmd = api + "/lastTick"
          doc = ArangoDB.log_get("#{prefix}-follow-collection", cmd, :body => "")
          doc.code.should eq(200)
          fromTick = doc.parsed_response["tick"]

          # create collection
          cid = ArangoDB.create_collection("UnitTestsReplication")
          cuid = ArangoDB.properties_collection(cid)["globallyUniqueId"]        

          # create document
          cmd = "/_api/document?collection=UnitTestsReplication"
          body = "{ \"_key\" : \"test\", \"test\" : false }"
          doc = ArangoDB.log_post("#{prefix}-follow-collection", cmd, :body => body)
          doc.code.should eq(201)
          rev = doc.parsed_response["_rev"]

          # update document
          cmd = "/_api/document/UnitTestsReplication/test"
          body = "{ \"updated\" : true }"
          doc = ArangoDB.log_patch("#{prefix}-follow-collection", cmd, :body => body)
          doc.code.should eq(201)
          rev2 = doc.parsed_response["_rev"]

          # delete document
          cmd = "/_api/document/UnitTestsReplication/test"
          doc = ArangoDB.log_delete("#{prefix}-follow-collection", cmd)
          doc.code.should eq(200)

          sleep 1

          cmd = api + "/tail?global=true&from=" + fromTick
          doc = ArangoDB.log_get("#{prefix}-follow-create-collection", cmd, :body => "", :format => :plain)
          [200, 204].should include(doc.code)
          body = doc.response.body
          
          break if doc.headers["x-arango-replication-frompresent"] == "true" and doc.headers["x-arango-replication-lastincluded"] != "0"
        end

        doc.headers["x-arango-replication-lastincluded"].should match(/^\d+$/)
        doc.headers["x-arango-replication-lastincluded"].should_not eq("0")
        doc.headers["content-type"].should eq("application/x-arango-dump; charset=utf-8")

        i = 0
        while 1
          position = body.index("\n")

          break if position == nil

          part = body.slice(0, position)
          document = JSON.parse(part)

          if i == 0
            if document["type"] == 2000 and document["cuid"] == cuid
              # create collection
              document.should have_key("tick")
              document.should have_key("type")
              document.should have_key("cuid")
              document.should have_key("data")

              document["tick"].should match(/^\d+$/)
              document["tick"].to_i.should >= fromTick.to_i
              document["type"].should eq(2000)
              document["cuid"].should eq(cuid)

              c = document["data"]
              c.should have_key("version")
              c["type"].should eq(2)
              c["cid"].should eq(cid)
              c["cid"].should be_kind_of(String)
              c["globallyUniqueId"].should eq(cuid)              
              c["deleted"].should eq(false)
              c["name"].should eq("UnitTestsReplication")
              c["waitForSync"].should eq(true)

              i = i + 1
            end
          elsif i == 1 and document["type"] == 2300 and document["cuid"] == cuid
            # create document
            document.should have_key("tick")
            document.should have_key("type")
            document.should have_key("cuid")

            document["tick"].should match(/^\d+$/)
            document["tick"].to_i.should >= fromTick.to_i
            document["type"].should eq(2300)
            document["cuid"].should eq(cuid)
            document["data"]["_key"].should eq("test")
            document["data"]["_rev"].should match(/^[a-zA-Z0-9_\-]+$/)
            document["data"]["_rev"].should eq(rev)
            document["data"]["test"].should eq(false)

            i = i + 1
          elsif i == 2 and document["cuid"] == cuid
            # update document, there must only be 2300 no 2302
            document.should have_key("tick")
            document.should have_key("type")
            document.should have_key("cuid")

            document["tick"].should match(/^\d+$/)
            document["tick"].to_i.should >= fromTick.to_i
            document["type"].should eq(2300)
            document["cuid"].should eq(cuid)
            document["data"]["_key"].should eq("test")
            document["data"]["_rev"].should match(/^[a-zA-Z0-9_\-]+$/)
            document["data"]["_rev"].should eq(rev2)
            document["data"]["test"].should eq(false)

            i = i + 1
          elsif i == 3 and document["type"] == 2302 and document["cuid"] == cuid
            # delete document
            document.should have_key("tick")
            document.should have_key("type")
            document.should have_key("cuid")

            document["tick"].should match(/^\d+$/)
            document["tick"].to_i.should >= fromTick.to_i
            document["type"].should eq(2302)
            document["cuid"].should eq(cuid)
            document["data"]["_key"].should eq("test")
            document["data"]["_rev"].should match(/^[a-zA-Z0-9_\-]+$/)
            document["data"]["_rev"].should_not eq(rev)

            i = i + 1
          end

          body = body.slice(position + 1, body.length)
        end

        i.should eq(4)

        # tail you later
        cmd = api + "/lastTick"
        doc = ArangoDB.log_get("#{prefix}-follow-collection", cmd, :body => "")
        doc.code.should eq(200)
        fromTick = doc.parsed_response["tick"]

        # drop collection
        cmd = "/_api/collection/UnitTestsReplication"
        doc = ArangoDB.log_delete("#{prefix}-follow-collection", cmd)
        doc.code.should eq(200)

        while 1
          sleep 1

          cmd = api + "/tail?global=true&from=" + fromTick
          doc = ArangoDB.log_get("#{prefix}-follow-create-collection", cmd, :body => "", :format => :plain)
          [200, 204].should include(doc.code)
          body = doc.response.body
          
          break if doc.headers["x-arango-replication-frompresent"] == "true" and doc.headers["x-arango-replication-lastincluded"] != "0"
        end

        while 1
          position = body.index("\n")

          break if position == nil

          part = body.slice(0, position)
          document = JSON.parse(part)

          if document["type"] == 2001 and document["cuid"] == cuid
            # drop collection
            document.should have_key("tick")
            document.should have_key("type")
            document.should have_key("cuid")

            document["tick"].should match(/^\d+$/)
            document["tick"].to_i.should >= fromTick.to_i
            document["type"].should eq(2001)
            document["cuid"].should eq(cuid)

            i = i + 1
          end

          body = body.slice(position + 1, body.length)
        end

        i.should eq(5)        

      end

      it "fetches some more single document operations from the log" do
        ArangoDB.drop_collection("UnitTestsReplication")

        sleep 5

        cmd = api + "/lastTick"
        doc = ArangoDB.log_get("#{prefix}-follow-collection", cmd, :body => "")
        doc.code.should eq(200)
        fromTick = doc.parsed_response["tick"]

        # create collection
        cid = ArangoDB.create_collection("UnitTestsReplication")
        cuid = ArangoDB.properties_collection(cid)["globallyUniqueId"]        

        # create document
        cmd = "/_api/document?collection=UnitTestsReplication"
        body = "{ \"_key\" : \"test\", \"test\" : false }"
        doc = ArangoDB.log_post("#{prefix}-follow-collection", cmd, :body => body)
        doc.code.should eq(201)
        rev = doc.parsed_response["_rev"]

        # update document
        cmd = "/_api/document/UnitTestsReplication/test"
        body = "{ \"updated\" : true }"
        doc = ArangoDB.log_patch("#{prefix}-follow-collection", cmd, :body => body)
        doc.code.should eq(201)
        rev2 = doc.parsed_response["_rev"]

        # delete document
        cmd = "/_api/document/UnitTestsReplication/test"
        doc = ArangoDB.log_delete("#{prefix}-follow-collection", cmd)
        doc.code.should eq(200)

        sleep 5

        cmd = api + "/tail?global=true&from=" + fromTick
        doc = ArangoDB.log_get("#{prefix}-follow-create-collection", cmd, :body => "", :format => :plain)
        doc.code.should eq(200)

        doc.headers["x-arango-replication-lastincluded"].should match(/^\d+$/)
        doc.headers["x-arango-replication-lastincluded"].should_not eq("0")
        doc.headers["content-type"].should eq("application/x-arango-dump; charset=utf-8")

        tickTypes = { 2000 => 0, 2001 => 0, 2300 => 0, 2302 => 0 }

        body = doc.response.body

        cmd = api + "/lastTick"
        doc = ArangoDB.log_get("#{prefix}-follow-collection", cmd, :body => "")
        doc.code.should eq(200)

        i = 0
        while 1
          position = body.index("\n")

          break if position == nil

          part = body.slice(0, position)
          marker = JSON.parse(part)

          if marker["type"] >= 2000 and marker["cuid"] == cuid
            # create collection
            marker.should have_key("tick")
            marker.should have_key("type")
            marker.should have_key("cuid")

            if marker["type"] == 2300
              marker.should have_key("data")
            end

            cc = tickTypes[marker["type"]]
            tickTypes[marker["type"]] = cc + 1
          end
          body = body.slice(position + 1, body.length)
        end

        tickTypes[2000].should eq(1) # collection create
        tickTypes[2001].should eq(0) # collection drop
        tickTypes[2300].should eq(2) # document insert / update
        tickTypes[2302].should eq(1) # document drop

        # drop collection
        cmd = "/_api/collection/UnitTestsReplication"
        doc = ArangoDB.log_delete("#{prefix}-follow-collection", cmd)
        doc.code.should eq(200)
      end
      
      it "validates chunkSize restrictions" do
        ArangoDB.drop_collection("UnitTestsReplication")

        sleep 1

        cmd = api + "/lastTick"
        doc = ArangoDB.log_get("#{prefix}-follow-chunksize", cmd, :body => "")
        doc.code.should eq(200)
        fromTick = doc.parsed_response["tick"]
        originalTick = fromTick
        lastScanned = fromTick

        # create collection
        cid = ArangoDB.create_collection("UnitTestsReplication")
        cuid = ArangoDB.properties_collection(cid)["globallyUniqueId"]        

        # create documents
        (1..250).each do |value| 
          cmd = "/_api/document?collection=UnitTestsReplication"
          body = "{ \"value\" : \"thisIsALongerStringBecauseWeWantToTestTheChunkSizeLimitsLaterOnAndItGetsEvenLongerWithTimeForRealNow\" }"
          doc = ArangoDB.log_post("#{prefix}-follow-chunksize", cmd, :body => body)
          doc.code.should eq(201)
        end

        # create one big transaction
        docsBody = "["
        (1..749).each do |value| 
          docsBody << "{ \"value\" : \"%d\" }," % [value]
        end
        docsBody << "{ \"value\" : \"500\" }]"
        cmd = "/_api/document?collection=UnitTestsReplication"
        doc = ArangoDB.log_post("#{prefix}-follow-chunksize", cmd, :body => docsBody)
        doc.code.should eq(201)

        # create more documents
        (1..500).each do |value| 
          cmd = "/_api/document?collection=UnitTestsReplication"
          body = "{ \"value\" : \"thisIsALongerStringBecauseWeWantToTestTheChunkSizeLimitsLaterOnAndItGetsEvenLongerWithTimeForRealNow\" }"
          doc = ArangoDB.log_post("#{prefix}-follow-chunksize", cmd, :body => body)
          doc.code.should eq(201)
        end

        sleep 1
        
        tickTypes = { 2000 => 0, 2001 => 0, 2200 => 0, 2201 => 0, 2300 => 0 }

        while 1
          cmd = api + "/tail?global=true&from=" + fromTick + "&lastScanned=" + lastScanned + "&chunkSize=16384"
          doc = ArangoDB.log_get("#{prefix}-follow-chunksize", cmd, :body => "", :format => :plain)
          [200, 204].should include(doc.code)
            
          break if doc.code == 204

          doc.headers["x-arango-replication-lastincluded"].should match(/^\d+$/)
          doc.headers["x-arango-replication-lastincluded"].should_not eq("0")
          doc.headers["x-arango-replication-lastscanned"].should match(/^\d+$/)
          doc.headers["x-arango-replication-lastscanned"].should_not eq("0")
          if fromTick == originalTick
            # first batch
            doc.headers["x-arango-replication-checkmore"].should eq("true")
          end
          doc.headers["content-type"].should eq("application/x-arango-dump; charset=utf-8")
          # we need to allow for some overhead here, as the chunkSize restriction is not honored precisely
          doc.headers["content-length"].to_i.should be < (16 + 9) * 1024

          # update lastScanned for next request
          lastScanned = doc.headers["x-arango-replication-lastscanned"]
          body = doc.response.body 

          i = 0
          while 1
            position = body.index("\n")

            break if position == nil

            part = body.slice(0, position)
            marker = JSON.parse(part)

            # update last tick value
            marker.should have_key("tick")
            fromTick = marker["tick"]

            if marker["type"] == 2200
              marker.should have_key("tid")
              marker.should have_key("db")
              tickTypes[2200] = tickTypes[2200] + 1

            elsif marker["type"] == 2201
              marker.should have_key("tid")
              tickTypes[2201] = tickTypes[2201] + 1

            elsif marker["type"] >= 2000 and marker["cuid"] == cuid
              # collection markings
              marker.should have_key("type")
              marker.should have_key("cuid")

              if marker["type"] == 2300
                marker.should have_key("data")
                marker.should have_key("tid")
              end

              cc = tickTypes[marker["type"]]
              tickTypes[marker["type"]] = cc + 1
           
              break if tickTypes[2300] == 1500
            end
            body = body.slice(position + 1, body.length)
          end
        end

        tickTypes[2000].should eq(1) # collection create
        tickTypes[2001].should eq(0) # collection drop
        tickTypes[2200].should be >= 1 # begin transaction
        tickTypes[2201].should be >= 1 # commit transaction
        tickTypes[2300].should eq(1500) # document inserts

        
        # now try again with a single chunk  
        tickTypes = { 2000 => 0, 2001 => 0, 2200 => 0, 2201 => 0, 2300 => 0 }

        cmd = api + "/tail?global=true&from=" + originalTick + "&lastScanned=" + originalTick + "&chunkSize=1048576"
        doc = ArangoDB.log_get("#{prefix}-follow-chunksize", cmd, :body => "", :format => :plain)
        doc.code.should eq(200)

        doc.headers["x-arango-replication-lastincluded"].should match(/^\d+$/)
        doc.headers["x-arango-replication-lastincluded"].should_not eq("0")
        doc.headers["content-type"].should eq("application/x-arango-dump; charset=utf-8")

        # we need to allow for some overhead here, as the chunkSize restriction is not honored precisely
        doc.headers["content-length"].to_i.should be > (16 + 8) * 1024

        body = doc.response.body

        i = 0
        while 1
          position = body.index("\n")

          break if position == nil

          part = body.slice(0, position)
          marker = JSON.parse(part)

          marker.should have_key("tick")

          if marker["type"] == 2200
            marker.should have_key("tid")
            marker.should have_key("db")
            tickTypes[2200] = tickTypes[2200] + 1

          elsif marker["type"] == 2201
            marker.should have_key("tid")
            tickTypes[2201] = tickTypes[2201] + 1

          elsif marker["type"] >= 2000 and marker["cuid"] == cuid
            # create collection
            marker.should have_key("type")
            marker.should have_key("cuid")

            if marker["type"] == 2300
              marker.should have_key("data")
              marker.should have_key("tid")
            end

            cc = tickTypes[marker["type"]]
            tickTypes[marker["type"]] = cc + 1
           
            break if tickTypes[2300] == 1500
          end
          body = body.slice(position + 1, body.length)
        end

        tickTypes[2000].should eq(1) # collection create
        tickTypes[2001].should eq(0) # collection drop
        tickTypes[2200].should be >= 1 # begin transaction
        tickTypes[2201].should be >= 1 # commit transaction
        tickTypes[2300].should eq(1500) # document inserts

        # drop collection
        cmd = "/_api/collection/UnitTestsReplication"
        doc = ArangoDB.log_delete("#{prefix}-follow-chunksize", cmd)
        doc.code.should eq(200)
      end
    end

################################################################################
## inventory / dump
################################################################################

      context "dealing with the initial dump" do

        api = "/_api/replication"
        prefix = "api-replication"

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

        it "checks the initial inventory" do
          cmd = api + "/inventory?includeSystem=true&global=true&batchId=#{@batchId}"
          doc = ArangoDB.log_get("#{prefix}-inventory", cmd, :body => "")

          doc.code.should eq(200)
          all = doc.parsed_response
          all.should have_key('databases')
          all.should have_key('state')
          state = all['state']
          state['running'].should eq(true)
          state['lastLogTick'].should match(/^\d+$/)
          state['time'].should match(/^\d+-\d+-\d+T\d+:\d+:\d+Z$/)
          
          databases = all['databases']        
          databases.each { |name, database|
            database.should have_key('collections')

            collections = database["collections"]
            filtered = [ ]
            collections.each { |collection|
              if [ "UnitTestsReplication", "UnitTestsReplication2" ].include? collection["parameters"]["name"]
                filtered.push collection
              end
              collection["parameters"].should have_key('globallyUniqueId')
            }
            filtered.should eq([ ])
          }
        end

        it "checks the inventory after creating collections" do
          cid = ArangoDB.create_collection("UnitTestsReplication", false)
          cuid = ArangoDB.properties_collection(cid)["globallyUniqueId"]                
          cid2 = ArangoDB.create_collection("UnitTestsReplication2", true, 3)
          cuid2 = ArangoDB.properties_collection(cid2)["globallyUniqueId"]                        

          cmd = api + "/inventory?includeSystem=true&global=true&batchId=#{@batchId}"
          doc = ArangoDB.log_get("#{prefix}-inventory-create", cmd, :body => "")
          doc.code.should eq(200)
          all = doc.parsed_response        

          all.should have_key('state')
          state = all['state']
          state['running'].should eq(true)
          state['lastLogTick'].should match(/^\d+$/)
          state['time'].should match(/^\d+-\d+-\d+T\d+:\d+:\d+Z$/)

          all.should have_key('databases')
          databases = all['databases']

          filtered = [ ]        
          databases.each { |name, database|
            database.should have_key('collections')

            collections = database["collections"]
            filtered = [ ]
            collections.each { |collection|
              if [ "UnitTestsReplication", "UnitTestsReplication2" ].include? collection["parameters"]["name"]
                filtered.push collection
              end
            collection["parameters"].should have_key('globallyUniqueId')
          }
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
        parameters.should have_key("globallyUniqueId")
        parameters["globallyUniqueId"].should eq(cuid)

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
        parameters.should have_key("globallyUniqueId")
        parameters["globallyUniqueId"].should eq(cuid2)

        c['indexes'].should eq([ ])
      end

      it "checks the inventory with indexes" do
        cid = ArangoDB.create_collection("UnitTestsReplication", false)
        cuid = ArangoDB.properties_collection(cid)["globallyUniqueId"]                        
        cid2 = ArangoDB.create_collection("UnitTestsReplication2", false)
        cuid2 = ArangoDB.properties_collection(cid2)["globallyUniqueId"]        

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

        cmd = api + "/inventory?batchId=#{@batchId}&global=true"
        doc = ArangoDB.log_get("#{prefix}-inventory2", cmd, :body => "")
        doc.code.should eq(200)

        all = doc.parsed_response

        all.should have_key('state')
        state = all['state']
        state['running'].should eq(true)
        state['lastLogTick'].should match(/^\d+$/)
        state['time'].should match(/^\d+-\d+-\d+T\d+:\d+:\d+Z$/)

        filtered = [ ]
        all.should have_key('databases')        
        databases = all['databases']
        databases.each { |name, database|
          database.should have_key('collections')
          collections = database['collections']
          collections.each { |collection|
            if [ "UnitTestsReplication", "UnitTestsReplication2" ].include? collection["parameters"]["name"]
              filtered.push collection
            end
          }
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
        parameters.should have_key("globallyUniqueId")
        parameters["globallyUniqueId"].should eq(cuid)

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
        parameters.should have_key("globallyUniqueId")
        parameters["globallyUniqueId"].should eq(cuid2)

        indexes = c['indexes']
        indexes.length.should eq(1)

        idx = indexes[0]
        idx["id"].should match(/^\d+$/)
        idx["type"].should eq("skiplist")
        idx["unique"].should eq(true)
        idx["fields"].should eq([ "d" ])

      end
    end
  end

  ## TODO: add CRUD tests for updates

end
