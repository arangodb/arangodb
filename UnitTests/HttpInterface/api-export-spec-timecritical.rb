# coding: utf-8

require 'rspec'
require 'arangodb.rb'

describe ArangoDB do
  api = "/_api/export"
  prefix = "api-export"

  context "dealing with exports:" do
    before do
      @reId = Regexp.new('^\d+$')
    end

################################################################################
## error handling
################################################################################

    context "error handling:" do
      it "returns an error if collection is unknown" do
        cmd = api + "?collection=unknowncollection";
        body = "{ \"count\" : true, \"batchSize\" : 2 }"
        doc = ArangoDB.log_post("#{prefix}-unknown-collection", cmd, :body => body)
       
        doc.code.should eq(404)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(404)
        doc.parsed_response['errorNum'].should eq(1203)
      end

      it "returns an error if cursor identifier is missing" do
        cmd = api
        doc = ArangoDB.log_put("#{prefix}-missing-cursor-identifier", cmd)
        
        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(400)
      end

      it "returns an error if cursor identifier is invalid" do
        cmd = api + "/123456"
        doc = ArangoDB.log_put("#{prefix}-invalid-cursor-identifier", cmd)
        
        doc.code.should eq(404)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(404)
        doc.parsed_response['errorNum'].should eq(1600)
      end

    end

################################################################################
## create and using exports
################################################################################

    context "handling exports:" do
      before do
        @cn = "users"
        ArangoDB.drop_collection(@cn)
        @cid = ArangoDB.create_collection(@cn, false)

        (0...10).each{|i|
          ArangoDB.post("/_api/document?collection=#{@cid}", :body => "{ \"n\" : #{i} }")
        }
      end

      after do
        ArangoDB.drop_collection(@cn)
      end

      it "creates a cursor, single run" do
        cmd = api + "?collection=#{@cid}"
        body = "{ \"count\" : true, \"flush\" : true }"
        doc = ArangoDB.log_post("#{prefix}-return-single", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should be_nil
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['count'].should eq(10)
        doc.parsed_response['result'].length.should eq(10)
      end
      
      it "creates a cursor single run, without count" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"count\" : false, \"flush\" : true }"
        doc = ArangoDB.log_post("#{prefix}-return-single", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should be_nil
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['count'].should eq(nil)
        doc.parsed_response['result'].length.should eq(10)
      end

      it "creates a cursor single run, larger batch size" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"count\" : true, \"batchSize\" : 50, \"flush\" : true }"
        doc = ArangoDB.log_post("#{prefix}-return-single-larger", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should be_nil
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['count'].should eq(10)
        doc.parsed_response['result'].length.should eq(10)
      end

      it "creates a cursor" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"count\" : true, \"batchSize\" : 4, \"flush\" : true }"
        doc = ArangoDB.log_post("#{prefix}-limit-return", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should be_kind_of(String)
        doc.parsed_response['id'].should match(@reId)
        doc.parsed_response['hasMore'].should eq(true)
        doc.parsed_response['count'].should eq(10)
        doc.parsed_response['result'].length.should eq(4)

        id = doc.parsed_response['id']

        cmd = api + "/#{id}"
        doc = ArangoDB.log_put("#{prefix}-return-cont", cmd)
        
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should be_kind_of(String)
        doc.parsed_response['id'].should match(@reId)
        doc.parsed_response['id'].should eq(id)
        doc.parsed_response['hasMore'].should eq(true)
        doc.parsed_response['count'].should eq(10)
        doc.parsed_response['result'].length.should eq(4)

        cmd = api + "/#{id}"
        doc = ArangoDB.log_put("#{prefix}-return-cont2", cmd)
        
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should be_nil
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['count'].should eq(10)
        doc.parsed_response['result'].length.should eq(2)

        cmd = api + "/#{id}"
        doc = ArangoDB.log_put("#{prefix}-return-cont3", cmd)

        doc.code.should eq(404)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(1600)
        doc.parsed_response['code'].should eq(404)
      end

      it "creates a cursor and deletes it in the middle" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"count\" : true, \"batchSize\" : 4, \"flush\" : true }"
        doc = ArangoDB.log_post("#{prefix}-return", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should be_kind_of(String)
        doc.parsed_response['id'].should match(@reId)
        doc.parsed_response['hasMore'].should eq(true)
        doc.parsed_response['count'].should eq(10)
        doc.parsed_response['result'].length.should eq(4)

        id = doc.parsed_response['id']

        cmd = api + "/#{id}"
        doc = ArangoDB.log_put("#{prefix}-return-cont", cmd)
        
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should be_kind_of(String)
        doc.parsed_response['id'].should match(@reId)
        doc.parsed_response['id'].should eq(id)
        doc.parsed_response['hasMore'].should eq(true)
        doc.parsed_response['count'].should eq(10)
        doc.parsed_response['result'].length.should eq(4)

        cmd = api + "/#{id}"
        doc = ArangoDB.log_delete("#{prefix}-delete", cmd)

        doc.code.should eq(202)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(202)
        doc.parsed_response['id'].should be_kind_of(String)
        doc.parsed_response['id'].should match(@reId)
      end

      it "deleting a cursor" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"count\" : true, \"batchSize\" : 2, \"flush\" : true }"
        doc = ArangoDB.post(cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should be_kind_of(String)
        doc.parsed_response['id'].should match(@reId)
        doc.parsed_response['hasMore'].should eq(true)
        doc.parsed_response['count'].should eq(10)
        doc.parsed_response['result'].length.should eq(2)

        id = doc.parsed_response['id']

        cmd = api + "/#{id}"
        doc = ArangoDB.log_delete("#{prefix}-delete", cmd)

        doc.code.should eq(202)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(202)
        doc.parsed_response['id'].should be_kind_of(String)
        doc.parsed_response['id'].should match(@reId)
      end

      it "deleting a deleted cursor" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"count\" : true, \"batchSize\" : 2, \"flush\" : true }"
        doc = ArangoDB.post(cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should be_kind_of(String)
        doc.parsed_response['id'].should match(@reId)
        doc.parsed_response['hasMore'].should eq(true)
        doc.parsed_response['count'].should eq(10)
        doc.parsed_response['result'].length.should eq(2)

        id = doc.parsed_response['id']

        cmd = api + "/#{id}"
        doc = ArangoDB.log_delete("#{prefix}-delete", cmd)

        doc.code.should eq(202)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(202)
        doc.parsed_response['id'].should be_kind_of(String)
        doc.parsed_response['id'].should match(@reId)
        
        doc = ArangoDB.log_delete("#{prefix}-delete", cmd)

        doc.code.should eq(404)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(1600);
        doc.parsed_response['code'].should eq(404)
        doc.parsed_response['id'].should be_nil
      end

      it "deleting an invalid cursor" do
        cmd = api + "/999999" # we assume this cursor id is invalid
        doc = ArangoDB.log_delete("#{prefix}-delete", cmd)

        doc.code.should eq(404)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true);
        doc.parsed_response['errorNum'].should eq(1600);
        doc.parsed_response['code'].should eq(404)
        doc.parsed_response['id'].should be_nil
      end

      it "creates a cursor that will expire" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"count\" : true, \"batchSize\" : 1, \"ttl\" : 4, \"flush\" : true }"
        doc = ArangoDB.log_post("#{prefix}-create-ttl", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should be_kind_of(String)
        doc.parsed_response['id'].should match(@reId)
        doc.parsed_response['hasMore'].should eq(true)
        doc.parsed_response['count'].should eq(10)
        doc.parsed_response['result'].length.should eq(1)

        sleep 1
        id = doc.parsed_response['id']

        cmd = api + "/#{id}"
        doc = ArangoDB.log_put("#{prefix}-create-ttl", cmd)
        
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should be_kind_of(String)
        doc.parsed_response['id'].should match(@reId)
        doc.parsed_response['id'].should eq(id)
        doc.parsed_response['hasMore'].should eq(true)
        doc.parsed_response['count'].should eq(10)
        doc.parsed_response['result'].length.should eq(1)

        sleep 1

        doc = ArangoDB.log_put("#{prefix}-create-ttl", cmd)
        
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should eq(id)
        doc.parsed_response['hasMore'].should eq(true)
        doc.parsed_response['count'].should eq(10)
        doc.parsed_response['result'].length.should eq(1)

        # after this, the cursor might expire eventually
        # the problem is that we cannot exactly determine the point in time
        # when it really vanishes, as this depends on thread scheduling, state     
        # of the cleanup thread etc.

        # sleep 10 # this should delete the cursor on the server
        # doc = ArangoDB.log_put("#{prefix}-create-ttl", cmd)
        # doc.code.should eq(404)
        # doc.headers['content-type'].should eq("application/json; charset=utf-8")
        # doc.parsed_response['error'].should eq(true)
        # doc.parsed_response['errorNum'].should eq(1600)
        # doc.parsed_response['code'].should eq(404)
      end
      
      it "creates a cursor that will not expire" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"count\" : true, \"batchSize\" : 1, \"ttl\" : 60, \"flush\" : true }"
        doc = ArangoDB.log_post("#{prefix}-create-ttl", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should be_kind_of(String)
        doc.parsed_response['id'].should match(@reId)
        doc.parsed_response['hasMore'].should eq(true)
        doc.parsed_response['count'].should eq(10)
        doc.parsed_response['result'].length.should eq(1)

        sleep 1
        id = doc.parsed_response['id']

        cmd = api + "/#{id}"
        doc = ArangoDB.log_put("#{prefix}-create-ttl", cmd)
        
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should be_kind_of(String)
        doc.parsed_response['id'].should match(@reId)
        doc.parsed_response['id'].should eq(id)
        doc.parsed_response['hasMore'].should eq(true)
        doc.parsed_response['count'].should eq(10)
        doc.parsed_response['result'].length.should eq(1)

        sleep 1

        doc = ArangoDB.log_put("#{prefix}-create-ttl", cmd)
        
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should eq(id)
        doc.parsed_response['hasMore'].should eq(true)
        doc.parsed_response['count'].should eq(10)
        doc.parsed_response['result'].length.should eq(1)

        sleep 5 # this should not delete the cursor on the server

        doc = ArangoDB.log_put("#{prefix}-create-ttl", cmd)
        
        doc.code.should eq(200)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should eq(id)
        doc.parsed_response['hasMore'].should eq(true)
        doc.parsed_response['count'].should eq(10)
        doc.parsed_response['result'].length.should eq(1)
        
        # must clean up. otherwise we'll have to wait 60 seconds!
        doc = ArangoDB.log_delete("#{prefix}-create-ttl", cmd)
      end
    end

################################################################################
## create and using exports
################################################################################

    context "handling bigger exports:" do
      before do
        @cn = "users"
        ArangoDB.drop_collection(@cn)
        @cid = ArangoDB.create_collection(@cn, false)

        ArangoDB.post("/_admin/execute", :body => "var db = require('internal').db, c = db.#{@cn}; for (var i = 0; i < 2000; ++i) { c.save({ n: i }); }")
      end

      after do
        ArangoDB.drop_collection(@cn)
      end

      it "creates a cursor, single run" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"count\" : true, \"batchSize\" : 2000, \"flush\" : true }"
        doc = ArangoDB.log_post("#{prefix}-return-single", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should be_nil
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['count'].should eq(2000)
        doc.parsed_response['result'].length.should eq(2000)
      end
      
      it "creates a cursor, multiple runs" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"count\" : true, \"batchSize\" : 700, \"flush\" : true }"
        doc = ArangoDB.log_post("#{prefix}-limit-return", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should be_kind_of(String)
        doc.parsed_response['id'].should match(@reId)
        doc.parsed_response['hasMore'].should eq(true)
        doc.parsed_response['count'].should eq(2000)
        doc.parsed_response['result'].length.should eq(700)

        id = doc.parsed_response['id']

        cmd = api + "/#{id}"
        doc = ArangoDB.log_put("#{prefix}-return-cont", cmd)
        
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should be_kind_of(String)
        doc.parsed_response['id'].should match(@reId)
        doc.parsed_response['id'].should eq(id)
        doc.parsed_response['hasMore'].should eq(true)
        doc.parsed_response['count'].should eq(2000)
        doc.parsed_response['result'].length.should eq(700)

        cmd = api + "/#{id}"
        doc = ArangoDB.log_put("#{prefix}-return-cont2", cmd)
        
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should be_nil
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['count'].should eq(2000)
        doc.parsed_response['result'].length.should eq(600)

        cmd = api + "/#{id}"
        doc = ArangoDB.log_put("#{prefix}-return-cont3", cmd)
        
        doc.code.should eq(404)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(1600)
        doc.parsed_response['code'].should eq(404)
      end
      
      it "consumes a cursor, while compaction is running" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"count\" : true, \"batchSize\" : 700, \"flush\" : true }"
        doc = ArangoDB.log_post("#{prefix}-limit-return", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should be_kind_of(String)
        doc.parsed_response['id'].should match(@reId)
        doc.parsed_response['hasMore'].should eq(true)
        doc.parsed_response['count'].should eq(2000)
        doc.parsed_response['result'].length.should eq(700)

        id = doc.parsed_response['id']

        # truncating the collection should not affect the results in the cursor
        cmd = "/_api/collection/#{@cn}/truncate"
        doc = ArangoDB.log_put("#{prefix}-return-cont", cmd)
        
        # flush wal
        cmd = "/_admin/wal/flush?waitForSync=true&waitForCollector=true"
        doc = ArangoDB.log_put("#{prefix}-return-cont", cmd)
        
        # rotate the journal
        cmd = "/_api/collection/#{@cn}/rotate"
        doc = ArangoDB.log_put("#{prefix}-return-cont", cmd)

        sleep 4

        # now continue with the cursor
        cmd = api + "/#{id}"
        doc = ArangoDB.log_put("#{prefix}-return-cont", cmd)
        
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should be_kind_of(String)
        doc.parsed_response['id'].should match(@reId)
        doc.parsed_response['id'].should eq(id)
        doc.parsed_response['hasMore'].should eq(true)
        doc.parsed_response['count'].should eq(2000)
        doc.parsed_response['result'].length.should eq(700)

        cmd = api + "/#{id}"
        doc = ArangoDB.log_put("#{prefix}-return-cont2", cmd)
        
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should be_nil
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['count'].should eq(2000)
        doc.parsed_response['result'].length.should eq(600)

        cmd = api + "/#{id}"
        doc = ArangoDB.log_put("#{prefix}-return-cont3", cmd)
        
        doc.code.should eq(404)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(1600)
        doc.parsed_response['code'].should eq(404)
      end

    end

  end
end
