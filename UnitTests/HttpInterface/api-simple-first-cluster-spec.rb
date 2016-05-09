# coding: utf-8

require 'rspec'
require 'arangodb.rb'

describe ArangoDB do
  api = "/_api/simple"
  prefix = "api-simple"

  context "simple queries:" do

################################################################################
## first / last
################################################################################

    context "first and last queries:" do
      before do
        @cn = "UnitTestsCollectionByExample"
        ArangoDB.drop_collection(@cn)
        
        body = "{ \"name\" : \"#{@cn}\", \"numberOfShards\" : 8 }"
        doc = ArangoDB.post("/_api/collection", :body => body)
        doc.code.should eq(200)
        @cid = doc.parsed_response['id']
      end

      after do
        ArangoDB.drop_collection(@cn)
      end
      
      it "finds the examples, sharded collection" do
        cmd = api + "/first"
        body = "{ \"collection\" : \"#{@cn}\" }"
        doc = ArangoDB.log_put("#{prefix}-first1", cmd, :body => body)

        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(17)
        
        cmd = api + "/first"
        body = "{ \"collection\" : \"#{@cn}\", \"count\" : 2 }"
        doc = ArangoDB.log_put("#{prefix}-first2", cmd, :body => body)

        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(17)

        cmd = api + "/last"
        body = "{ \"collection\" : \"#{@cn}\" }"
        doc = ArangoDB.log_put("#{prefix}-last1", cmd, :body => body)

        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(17)
        
        cmd = api + "/last"
        body = "{ \"collection\" : \"#{@cn}\", \"count\" : 2 }"
        doc = ArangoDB.log_put("#{prefix}-last2", cmd, :body => body)

        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(17)
      end
    end

  end
end
