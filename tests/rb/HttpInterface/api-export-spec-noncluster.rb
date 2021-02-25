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
      
      it "returns an error if restrict has an invalid type" do
        cmd = api + "?collection=whatever"
        doc = ArangoDB.log_post("#{prefix}-missing-restrict-type", cmd, :body => "{ \"restrict\" : \"foo\" }")
        
        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(17)
      end

      it "returns an error if restrict type is missing" do
        cmd = api + "?collection=whatever"
        doc = ArangoDB.log_post("#{prefix}-missing-restrict-type", cmd, :body => "{ \"restrict\" : { \"fields\" : [ ] } }")
        
        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(10)
      end

      it "returns an error if restrict type is invalid" do
        cmd = api + "?collection=whatever"
        doc = ArangoDB.log_post("#{prefix}-invalid-restrict-type", cmd, :body => "{ \"restrict\" : { \"type\" : \"foo\", \"fields\" : [ ] } }")
        
        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(10)
      end

      it "returns an error if restrict fields are missing" do
        cmd = api + "?collection=whatever"
        doc = ArangoDB.log_post("#{prefix}-missing-restrict-fields", cmd, :body => "{ \"restrict\" : { \"type\" : \"include\" } }")
        
        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(10)
      end

      it "returns an error if restrict fields are invalid" do
        cmd = api + "?collection=whatever"
        doc = ArangoDB.log_post("#{prefix}-invalid-restrict-fields", cmd, :body => "{ \"restrict\" : { \"type\" : \"include\", \"fields\" : \"foo\" } }")
        
        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(10)
      end

    end

################################################################################
## create and using exports
################################################################################

    context "handling exports:" do
      before(:all) do
        @cn = "users"
        ArangoDB.drop_collection(@cn)
        @cid = ArangoDB.create_collection(@cn, false)

        (0...10).each{|i|
          ArangoDB.post("/_api/document?collection=#{@cid}", :body => "{ \"n\" : #{i} }")
        }
      end

      after(:all) do
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

      it "creates a cursor, single run, no body" do
        doc = ArangoDB.log_put("#{prefix}-return-single", "/_admin/wal/flush", :body => "{\"waitForSync\":true,\"waitForCollector\":true}")

        cmd = api + "?collection=#{@cid}"
        body = "{ \"flush\" : true }"
        doc = ArangoDB.log_post("#{prefix}-return-single", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should be_nil
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['count'].should be_nil
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

      it "creates a cursor, single run, bigger" do
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
      
      it "creates a cursor, multiple runs, bigger" do
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

      it "using limit, bigger" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"count\" : true, \"batchSize\" : 2000, \"flush\" : true, \"limit\" : 5 }"
        doc = ArangoDB.log_post("#{prefix}-limit", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should be_nil
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['count'].should eq(5)
        doc.parsed_response['result'].length.should eq(5)
      end
      
      it "using limit == collection size, bigger" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"count\" : true, \"batchSize\" : 2000, \"flush\" : true, \"limit\" : 2000 }"
        doc = ArangoDB.log_post("#{prefix}-limit-eq", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should be_nil
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['count'].should eq(2000)
        doc.parsed_response['result'].length.should eq(2000)
      end
      
      it "using limit > collection size, bigger" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"count\" : true, \"batchSize\" : 2000, \"flush\" : true, \"limit\" : 200000 }"
        doc = ArangoDB.log_post("#{prefix}-limit", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should be_nil
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['count'].should eq(2000)
        doc.parsed_response['result'].length.should eq(2000)
      end
    end

################################################################################
## using restrictions
################################################################################

    context "handling restrictions:" do
      before do
        @cn = "users"
        ArangoDB.drop_collection(@cn)
        @cid = ArangoDB.create_collection(@cn, false)

        ArangoDB.post("/_admin/execute", :body => "var db = require('internal').db, c = db.#{@cn}; for (var i = 0; i < 2000; ++i) { c.save({ a: i, b: i, c: i }); }")
      end

      after do
        ArangoDB.drop_collection(@cn)
      end

      it "includes a single attribute" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"count\" : true, \"batchSize\" : 2000, \"restrict\" : { \"type\" : \"include\", \"fields\" : [ \"b\" ] }, \"flush\" : true }"
        doc = ArangoDB.log_post("#{prefix}-include-single", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should be_nil
        doc.parsed_response['count'].should eq(2000)
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(2000)

        doc.parsed_response['result'].each{|oneDoc|
          oneDoc.size.should eq(1)
          oneDoc.should have_key('b')
        }
      end

      it "includes a few attributes" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"batchSize\" : 2000, \"restrict\" : { \"type\" : \"include\", \"fields\" : [ \"b\", \"_id\", \"_rev\", \"a\" ] }, \"flush\" : true }"
        doc = ArangoDB.log_post("#{prefix}-include-few", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should be_nil
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(2000)

        doc.parsed_response['result'].each{|oneDoc|
          oneDoc.size.should eq(4)
          oneDoc.should have_key('a')
          oneDoc.should have_key('b')
          oneDoc.should_not have_key('c')
          oneDoc.should_not have_key('_key')
          oneDoc.should have_key('_id')
          oneDoc.should have_key('_rev')
        }
      end

      it "includes non-existing attributes" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"batchSize\" : 2000, \"restrict\" : { \"type\" : \"include\", \"fields\" : [ \"c\", \"xxxx\", \"A\" ] }, \"flush\" : true }"
        doc = ArangoDB.log_post("#{prefix}-include-non-existing", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should be_nil
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(2000)

        doc.parsed_response['result'].each{|oneDoc|
          oneDoc.size.should eq(1)
          oneDoc.should have_key('c')
        }
      end

      it "includes no attributes" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"batchSize\" : 2000, \"restrict\" : { \"type\" : \"include\", \"fields\" : [ ] }, \"flush\" : true }"
        doc = ArangoDB.log_post("#{prefix}-include-none", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should be_nil
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(2000)

        doc.parsed_response['result'].each{|oneDoc|
          oneDoc.size.should eq(0)
        }
      end

      it "excludes a single attribute" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"batchSize\" : 2000, \"restrict\" : { \"type\" : \"exclude\", \"fields\" : [ \"b\" ] }, \"flush\" : true }"
        doc = ArangoDB.log_post("#{prefix}-exclude-single", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should be_nil
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(2000)

        doc.parsed_response['result'].each{|oneDoc|
          oneDoc.size.should eq(5)
          oneDoc.should_not have_key('b')
          oneDoc.should have_key('a')
          oneDoc.should have_key('c')
          oneDoc.should have_key('_id')
          oneDoc.should have_key('_key')
          oneDoc.should have_key('_rev')
        }
      end

      it "excludes a few attributes" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"batchSize\" : 2000, \"restrict\" : { \"type\" : \"exclude\", \"fields\" : [ \"b\", \"_id\", \"_rev\", \"a\" ] }, \"flush\" : true }"
        doc = ArangoDB.log_post("#{prefix}-exclude-few", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should be_nil
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(2000)

        doc.parsed_response['result'].each{|oneDoc|
          oneDoc.size.should eq(2)
          oneDoc.should have_key('c')
          oneDoc.should have_key('_key')
        }
      end

      it "excludes non-existing attributes" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"batchSize\" : 2000, \"restrict\" : { \"type\" : \"exclude\", \"fields\" : [ \"c\", \"xxxx\", \"A\" ] }, \"flush\" : true }"
        doc = ArangoDB.log_post("#{prefix}-exclude-non-existing", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should be_nil
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(2000)

        doc.parsed_response['result'].each{|oneDoc|
          oneDoc.size.should eq(5)
          oneDoc.should_not have_key('c')
        }
      end

      it "excludes no attributes" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"batchSize\" : 2000, \"restrict\" : { \"type\" : \"exclude\", \"fields\" : [ ] }, \"flush\" : true }"
        doc = ArangoDB.log_post("#{prefix}-exclude-none", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should be_nil
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(2000)

        doc.parsed_response['result'].each{|oneDoc|
          oneDoc.size.should eq(6)
        }
      end

    end

  end
end
