# coding: utf-8

require 'rspec'
require 'json'
require 'arangodb.rb'

describe ArangoDB do
  api = "/_api/replication"
  prefix = "api-replication"

  context "dealing with the replication interface:" do

################################################################################
## general
################################################################################

    context "dealing with general functions" do
      
      it "fetches the server id" do
        # fetch id
        cmd = api + "/server-id"
        doc = ArangoDB.log_get("#{prefix}-server-id", cmd)

        doc.code.should eq(200)
        doc.parsed_response['serverId'].should match(/^\d+$/)
      end

    end

################################################################################
## logger
################################################################################

    context "dealing with the logger" do

      before do
      end

      after do
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
      
      it "fetches the empty follow log" do
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

            doc.headers["x-arango-replication-checkmore"].should eq("false")
            doc.headers["x-arango-replication-lastincluded"].should match(/^\d+$/)
            doc.headers["x-arango-replication-lastincluded"].should eq("0")
            doc.headers["content-type"].should eq("application/x-arango-dump; charset=utf-8")
         
            body = doc.response.body
            body.should eq(nil)
            break
          end
        end
      end
      
      it "fetches a create collection action from the follow log" do
        ArangoDB.drop_collection("UnitTestsReplication")
        
        sleep 1

        cmd = api + "/logger-state"
        doc = ArangoDB.log_get("#{prefix}-follow-create-collection", cmd, :body => "")
        doc.code.should eq(200)
        doc.parsed_response["state"]["running"].should eq(true)
        fromTick = doc.parsed_response["state"]["lastLogTick"]

        cid = ArangoDB.create_collection("UnitTestsReplication")

        sleep 1
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
            document.should have_key("collection") 

            document["tick"].should match(/^\d+$/)
            document["tick"].to_i.should >= fromTick.to_i
            document["type"].should eq(2000) 
            document["cid"].should eq(cid) 

            c = document["collection"]
            c.should have_key("version")
            c["type"].should eq(2)
            c["cid"].should eq(cid)
            c["deleted"].should eq(false)
            c["doCompact"].should eq(true)
            c.should have_key("maximalSize")
            c["maximalSize"].should be_kind_of(Integer)
            c["name"].should eq("UnitTestsReplication")
            c["isVolatile"].should eq(false)
            c["waitForSync"].should eq(true)
          end

          body = body.slice(position + 1, body.length)
        end
         
      end

      it "fetches some collection operations from the follow log" do
        ArangoDB.drop_collection("UnitTestsReplication")
        
        sleep 1

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
        
        # delete document 
        cmd = "/_api/document/UnitTestsReplication/test"
        doc = ArangoDB.log_delete("#{prefix}-follow-collection", cmd) 
        doc.code.should eq(200)

        # drop collection
        cmd = "/_api/collection/UnitTestsReplication"
        doc = ArangoDB.log_delete("#{prefix}-follow-collection", cmd) 
        doc.code.should eq(200)
        
        sleep 1

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
              document.should have_key("collection") 

              document["tick"].should match(/^\d+$/)
              document["tick"].to_i.should >= fromTick.to_i
              document["type"].should eq(2000) 
              document["cid"].should eq(cid) 

              c = document["collection"]
              c.should have_key("version")
              c["type"].should eq(2)
              c["cid"].should eq(cid)
              c["deleted"].should eq(false)
              c["doCompact"].should eq(true)
              c.should have_key("maximalSize")
              c["maximalSize"].should be_kind_of(Integer)
              c["name"].should eq("UnitTestsReplication")
              c["isVolatile"].should eq(false)
              c["waitForSync"].should eq(true)
          
              i = i + 1
            end

          elsif i == 1 and document["type"] == 2300 and document["cid"] == cid
            # create document
            document.should have_key("tick") 
            document.should have_key("type") 
            document.should have_key("cid") 
            document.should have_key("key") 
            document.should have_key("rev") 
            
            document["tick"].should match(/^\d+$/)
            document["tick"].to_i.should >= fromTick.to_i
            document["type"].should eq(2300) 
            document["cid"].should eq(cid) 
            document["key"].should eq("test") 
            rev = document["rev"]

            rev.should match(/^\d+$/)
            rev.should_not eq("0")
            
            document["data"]["_key"].should eq("test") 
            document["data"]["_rev"].should eq(rev)
            document["data"]["test"].should eq(false)
              
            i = i + 1
          elsif i == 2 and document["type"] == 2302 and document["cid"] == cid
            # delete document
            document.should have_key("tick") 
            document.should have_key("type") 
            document.should have_key("cid") 
            document.should have_key("key") 
            document.should have_key("rev") 

            document["tick"].should match(/^\d+$/)
            document["tick"].to_i.should >= fromTick.to_i
            document["type"].should eq(2302) 
            document["cid"].should eq(cid) 
            document["key"].should eq("test") 
            document["rev"].should match(/^\d+$/)
            document["rev"].should_not eq(rev)
              
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

      end

      after do
        ArangoDB.put(api + "/logger-stop", :body => "")
        ArangoDB.drop_collection("UnitTestsReplication")
        ArangoDB.drop_collection("UnitTestsReplication2")
      end

################################################################################
## inventory
################################################################################

      it "checks the initial inventory with the default value for includeSystem" do
        cmd = api + "/inventory"
        doc = ArangoDB.log_get("#{prefix}-inventory-system-defaults", cmd, :body => "")

        doc.code.should eq(200)
        all = doc.parsed_response
        all.should have_key('collections')
        all.should have_key('state')

        collections = all["collections"]
        collections.each { |collection|
          collection["parameters"]["name"].should_not match(/^_/)
        }
      end

      it "checks the initial inventory" do
        cmd = api + "/inventory?includeSystem=false"
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
      
      it "checks the initial inventory for non-system collections" do
        cmd = api + "/inventory?includeSystem=false"
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
        cmd = api + "/inventory?includeSystem=true"
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
     
      it "checks the inventory after creating collections" do
        cid = ArangoDB.create_collection("UnitTestsReplication", false)
        cid2 = ArangoDB.create_collection("UnitTestsReplication2", true, 3)
       
        cmd = api + "/inventory?includeSystem=false"
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
        parameters["deleted"].should eq(false)
        parameters["doCompact"].should eq(true)
        parameters.should have_key("maximalSize")
        parameters["maximalSize"].should be_kind_of(Integer)
        parameters["name"].should eq("UnitTestsReplication")
        parameters["isVolatile"].should eq(false)
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
        parameters["deleted"].should eq(false)
        parameters["doCompact"].should eq(true)
        parameters.should have_key("maximalSize")
        parameters["maximalSize"].should be_kind_of(Integer)
        parameters["name"].should eq("UnitTestsReplication2")
        parameters["isVolatile"].should eq(false)
        parameters["waitForSync"].should eq(true)

        c['indexes'].should eq([ ])
      end
      
      it "checks the inventory with indexes" do
        cid = ArangoDB.create_collection("UnitTestsReplication", false)
        cid2 = ArangoDB.create_collection("UnitTestsReplication2", false)

        # create indexes for first collection
        body = "{ \"type\" : \"cap\", \"size\" : 9991 }"
        doc = ArangoDB.log_post("#{prefix}-inventory2", "/_api/index?collection=UnitTestsReplication", :body => body)
        doc.code.should eq(201)
        
        body = "{ \"type\" : \"hash\", \"unique\" : false, \"fields\" : [ \"a\", \"b\" ] }"
        doc = ArangoDB.log_post("#{prefix}-inventory2", "/_api/index?collection=UnitTestsReplication", :body => body)
        doc.code.should eq(201)
        
        body = "{ \"type\" : \"skiplist\", \"unique\" : false, \"fields\" : [ \"c\" ] }"
        doc = ArangoDB.log_post("#{prefix}-inventory2", "/_api/index?collection=UnitTestsReplication", :body => body)
        doc.code.should eq(201)
        
        # create indexes for second collection
        body = "{ \"type\" : \"geo\", \"unique\" : false, \"fields\" : [ \"a\", \"b\" ] }"
        doc = ArangoDB.log_post("#{prefix}-inventory2", "/_api/index?collection=UnitTestsReplication2", :body => body)
        doc.code.should eq(201)
        
        body = "{ \"type\" : \"skiplist\", \"unique\" : true, \"fields\" : [ \"d\" ] }"
        doc = ArangoDB.log_post("#{prefix}-inventory2", "/_api/index?collection=UnitTestsReplication2", :body => body)
        doc.code.should eq(201)
        
        body = "{ \"type\" : \"fulltext\", \"minLength\" : 8, \"fields\" : [ \"ff\" ] }"
        doc = ArangoDB.log_post("#{prefix}-inventory2", "/_api/index?collection=UnitTestsReplication2", :body => body)
        doc.code.should eq(201)
        
        body = "{ \"type\" : \"cap\", \"byteSize\" : 1048576 }"
        doc = ArangoDB.log_post("#{prefix}-inventory2", "/_api/index?collection=UnitTestsReplication2", :body => body)
        doc.code.should eq(201)
        
        cmd = api + "/inventory"
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
        parameters["deleted"].should eq(false)
        parameters["doCompact"].should eq(true)
        parameters.should have_key("maximalSize")
        parameters["maximalSize"].should be_kind_of(Integer)
        parameters["name"].should eq("UnitTestsReplication")
        parameters["isVolatile"].should eq(false)
        parameters["waitForSync"].should eq(false)

        indexes = c['indexes']
        indexes.length.should eq(3)
        
        idx = indexes[0]
        idx["id"].should match(/^\d+$/)
        idx["type"].should eq("cap")
        idx["size"].should eq(9991)
        idx["byteSize"].should eq(0)
        
        idx = indexes[1]
        idx["id"].should match(/^\d+$/)
        idx["type"].should eq("hash")
        idx["unique"].should eq(false)
        idx["fields"].should eq([ "a", "b" ])
        
        idx = indexes[2]
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
        parameters["deleted"].should eq(false)
        parameters["doCompact"].should eq(true)
        parameters.should have_key("maximalSize")
        parameters["maximalSize"].should be_kind_of(Integer)
        parameters["name"].should eq("UnitTestsReplication2")
        parameters["isVolatile"].should eq(false)
        parameters["waitForSync"].should eq(false)
        
        indexes = c['indexes']
        indexes.length.should eq(4)
        
        idx = indexes[0]
        idx["id"].should match(/^\d+$/)
        idx["type"].should eq("geo2")
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

        idx = indexes[3]
        idx["id"].should match(/^\d+$/)
        idx["type"].should eq("cap")
        idx["size"].should eq(0)
        idx["byteSize"].should eq(1048576)
      end

################################################################################
## dump
################################################################################

      it "checks the dump for an empty collection" do
        cid = ArangoDB.create_collection("UnitTestsReplication", false)

        doc = ArangoDB.log_put("#{prefix}-deleted", "/_admin/wal/flush?waitForSync=true&waitForCollector=true", :body => "")
        doc.code.should eq(200)

        cmd = api + "/dump?collection=UnitTestsReplication"
        doc = ArangoDB.log_get("#{prefix}-dump-empty", cmd, :body => "")

        doc.code.should eq(204)

        doc.headers["x-arango-replication-checkmore"].should eq("false")
        doc.headers["x-arango-replication-lastincluded"].should eq("0")
        doc.headers["content-type"].should eq("application/x-arango-dump; charset=utf-8")
        doc.response.body.should eq(nil)
      end
      
      it "checks the dump for a non-empty collection" do
        cid = ArangoDB.create_collection("UnitTestsReplication", false)

        (0...100).each{|i|
          body = "{ \"_key\" : \"test" + i.to_s + "\", \"test\" : " + i.to_s + " }"
          doc = ArangoDB.post("/_api/document?collection=UnitTestsReplication", :body => body)
          doc.code.should eq(202)
        }

        cmd = api + "/dump?collection=UnitTestsReplication"
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
          doc['key'].should eq("test" + i.to_s)
          doc['rev'].should match(/^\d+$/)
          doc['data']['_key'].should eq("test" + i.to_s)
          doc['data']['_rev'].should match(/^\d+$/)
          doc['data']['test'].should eq(i)

          body = body.slice(position + 1, body.length)
          i = i + 1
        end

        i.should eq(100)
      end

      it "checks the dump for a non-empty collection, small chunkSize" do
        cid = ArangoDB.create_collection("UnitTestsReplication", false)

        (0...100).each{|i|
          body = "{ \"_key\" : \"test" + i.to_s + "\", \"test\" : " + i.to_s + " }"
          doc = ArangoDB.post("/_api/document?collection=UnitTestsReplication", :body => body)
          doc.code.should eq(202)
        }

        doc = ArangoDB.log_put("#{prefix}-deleted", "/_admin/wal/flush?waitForSync=true&waitForCollector=true", :body => "")
        doc.code.should eq(200)

        cmd = api + "/dump?collection=UnitTestsReplication&chunkSize=1024"
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
          doc['key'].should eq("test" + i.to_s)
          doc['rev'].should match(/^\d+$/)
          doc['data']['_key'].should eq("test" + i.to_s)
          doc['data']['_rev'].should match(/^\d+$/)
          doc['data']['test'].should eq(i)

          body = body.slice(position + 1, body.length)
          i = i + 1
        end

        i.should be < 100
      end

      
      it "checks the dump for an edge collection" do
        cid = ArangoDB.create_collection("UnitTestsReplication", false)
        cid2 = ArangoDB.create_collection("UnitTestsReplication2", false, 3)

        (0...100).each{|i|
          body = "{ \"_key\" : \"test" + i.to_s + "\", \"test1\" : " + i.to_s + ", \"test2\" : false, \"test3\" : [ ], \"test4\" : { } }"
          doc = ArangoDB.post("/_api/edge?collection=UnitTestsReplication2&from=UnitTestsReplication/foo&to=UnitTestsReplication/bar", :body => body)
          doc.code.should eq(202)
        }

        doc = ArangoDB.log_put("#{prefix}-deleted", "/_admin/wal/flush?waitForSync=true&waitForCollector=true", :body => "")
        doc.code.should eq(200)

        cmd = api + "/dump?collection=UnitTestsReplication2&chunkSize=65536"
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
          document['type'].should eq(2301)
          document['key'].should eq("test" + i.to_s)
          document['rev'].should match(/^\d+$/)
          document['data']['_key'].should eq("test" + i.to_s)
          document['data']['_rev'].should match(/^\d+$/)
          document['data']['_from'].should eq("UnitTestsReplication/foo")
          document['data']['_to'].should eq("UnitTestsReplication/bar")
          document['data']['test1'].should eq(i)
          document['data']['test2'].should eq(false)
          document['data']['test3'].should eq([ ])
          document['data']['test4'].should eq({ })

          body = body.slice(position + 1, body.length)
          i = i + 1
        end

        i.should eq(100)
      end

      it "checks the dump for an edge collection, small chunkSize" do
        cid = ArangoDB.create_collection("UnitTestsReplication", false)
        cid2 = ArangoDB.create_collection("UnitTestsReplication2", false, 3)

        (0...100).each{|i|
          body = "{ \"_key\" : \"test" + i.to_s + "\", \"test1\" : " + i.to_s + ", \"test2\" : false, \"test3\" : [ ], \"test4\" : { } }"
          doc = ArangoDB.post("/_api/edge?collection=UnitTestsReplication2&from=UnitTestsReplication/foo&to=UnitTestsReplication/bar", :body => body)
          doc.code.should eq(202)
        }

        doc = ArangoDB.log_put("#{prefix}-deleted", "/_admin/wal/flush?waitForSync=true&waitForCollector=true", :body => "")
        doc.code.should eq(200)

        cmd = api + "/dump?collection=UnitTestsReplication2&chunkSize=1024"
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
          document['type'].should eq(2301)
          document['key'].should eq("test" + i.to_s)
          document['rev'].should match(/^\d+$/)
          document['data']['_key'].should eq("test" + i.to_s)
          document['data']['_rev'].should match(/^\d+$/)
          document['data']['_from'].should eq("UnitTestsReplication/foo")
          document['data']['_to'].should eq("UnitTestsReplication/bar")
          document['data']['test1'].should eq(i)
          document['data']['test2'].should eq(false)
          document['data']['test3'].should eq([ ])
          document['data']['test4'].should eq({ })

          body = body.slice(position + 1, body.length)
          i = i + 1
        end

        i.should be < 100
      end
      
      it "checks the dump for a collection with deleted documents" do
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

        cmd = api + "/dump?collection=UnitTestsReplication"
        doc = ArangoDB.log_get("#{prefix}-deleted", cmd, :body => "", :format => :plain)
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

          document['type'].should eq(2302)
          document['key'].should eq("test" + i.floor.to_s)

          body = body.slice(position + 1, body.length)
          i = i + 1
        end

        i.should eq(10)
      end
      
      it "checks the dump for a truncated collection" do
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

        cmd = api + "/dump?collection=UnitTestsReplication"
        doc = ArangoDB.log_get("#{prefix}-truncated", cmd, :body => "", :format => :plain)
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

          document['type'].should eq(2302)
          # truncate order is undefined
          document['key'].should match(/^test\d+$/)
          document['rev'].should match(/^\d+$/)

          body = body.slice(position + 1, body.length)
          i = i + 1
        end

        i.should eq(10)
      end
      
      it "fetches incremental parts of a collection dump" do
        cid = ArangoDB.create_collection("UnitTestsReplication", false)

        (0...10).each{|i|
          body = "{ \"_key\" : \"test" + i.to_s + "\", \"test\" : " + i.to_s + " }"
          doc = ArangoDB.post("/_api/document?collection=UnitTestsReplication", :body => body)
          doc.code.should eq(202)
        }

        doc = ArangoDB.log_put("#{prefix}-deleted", "/_admin/wal/flush?waitForSync=true&waitForCollector=true", :body => "")
        doc.code.should eq(200)

        fromTick = "0"

        (0...10).each{|i|
          cmd = api + "/dump?collection=UnitTestsReplication&from=" + fromTick + "&chunkSize=1"
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
          document['key'].should eq("test" + i.to_s)
          document['rev'].should match(/^\d+$/)
          document['data']['_key'].should eq("test" + i.to_s)
          document['data']['_rev'].should match(/^\d+$/)
          document['data']['test'].should eq(i)
        }
      end

    end

  end

end
