# coding: utf-8

require 'rspec'
require 'arangodb.rb'

describe ArangoDB do
  api = "/_api/simple"
  prefix = "api-simple"

  context "simple queries:" do

################################################################################
## all query
################################################################################

    context "all query (will take a while when using ssl):" do
      before do
        @cn = "UnitTestsCollectionSimple"
        ArangoDB.drop_collection(@cn)
        
        body = "{ \"name\" : \"#{@cn}\", \"numberOfShards\" : 8 }"
        doc = ArangoDB.post("/_api/collection", :body => body)
        doc.code.should eq(200)
        @cid = doc.parsed_response['id']

        ArangoDB.post("/_admin/execute", :body => "var db = require('internal').db, c = db.#{@cn}; for (var i = 0; i < 1500; ++i) { c.save({ n: i }); }")
      end

      after do
        ArangoDB.drop_collection(@cn)
      end

      it "get all documents" do
        cmd = api + "/all"
        body = "{ \"collection\" : \"#{@cn}\" }"
        doc = ArangoDB.log_put("#{prefix}-all", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(true)
        doc.parsed_response['result'].length.should eq(1000)
        doc.parsed_response['count'].should eq(1500)
      end

      it "get all documents with limit" do
        cmd = api + "/all"
        body = "{ \"collection\" : \"#{@cn}\", \"limit\" : 100 }"
        doc = ArangoDB.log_put("#{prefix}-all-limit", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(100)
        doc.parsed_response['count'].should eq(100)
      end

      it "get all documents with negative skip" do
        cmd = api + "/all"
        body = "{ \"collection\" : \"#{@cn}\", \"skip\" : -100 }"
        doc = ArangoDB.log_put("#{prefix}-all-negative-limit", cmd, :body => body)

        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(1504)
      end

      it "get all documents with skip" do
        cmd = api + "/all"
        body = "{ \"collection\" : \"#{@cn}\", \"skip\" : 1400 }"
        doc = ArangoDB.log_put("#{prefix}-all-skip", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(100)
        doc.parsed_response['count'].should eq(100)
      end

      it "get all documents with skip and limit" do
        cmd = api + "/all"
        body = "{ \"collection\" : \"#{@cn}\", \"skip\" : 1400, \"limit\" : 2 }"
        doc = ArangoDB.log_put("#{prefix}-all-skip-limit", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(2)
        doc.parsed_response['count'].should eq(2)
      end
    end

  end
end
