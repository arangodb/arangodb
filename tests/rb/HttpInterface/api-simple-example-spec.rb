# coding: utf-8

require 'rspec'
require 'arangodb.rb'

describe ArangoDB do
  api = "/_api/simple"
  prefix = "api-simple"

  context "simple queries:" do

################################################################################
## by-example query
################################################################################

    context "by-example query:" do
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

      it "finds the examples" do
        body = "{ \"i\" : 1 }"
        doc = ArangoDB.post("/_api/document?collection=#{@cn}", :body => body)
        doc.code.should eq(202)
        d1 = doc.parsed_response['_id']

        body = "{ \"i\" : 1, \"a\" : { \"j\" : 1 } }"
        doc = ArangoDB.post("/_api/document?collection=#{@cn}", :body => body)
        doc.code.should eq(202)
        d2 = doc.parsed_response['_id']

        body = "{ \"i\" : 1, \"a\" : { \"j\" : 1, \"k\" : 1 } }"
        doc = ArangoDB.post("/_api/document?collection=#{@cn}", :body => body)
        doc.code.should eq(202)
        d3 = doc.parsed_response['_id']

        body = "{ \"i\" : 1, \"a\" : { \"j\" : 2, \"k\" : 2 } }"
        doc = ArangoDB.post("/_api/document?collection=#{@cn}", :body => body)
        doc.code.should eq(202)
        d4 = doc.parsed_response['_id']

        body = "{ \"i\" : 2 }"
        doc = ArangoDB.post("/_api/document?collection=#{@cn}", :body => body)
        doc.code.should eq(202)
        d5 = doc.parsed_response['_id']

        body = "{ \"i\" : 2, \"a\" : 2 }"
        doc = ArangoDB.post("/_api/document?collection=#{@cn}", :body => body)
        doc.code.should eq(202)
        d6 = doc.parsed_response['_id']

        body = "{ \"i\" : 2, \"a\" : { \"j\" : 2, \"k\" : 2 } }"
        doc = ArangoDB.post("/_api/document?collection=#{@cn}", :body => body)
        doc.code.should eq(202)
        d7 = doc.parsed_response['_id']

        cmd = api + "/by-example"
        body = "{ \"collection\" : \"#{@cn}\", \"example\" : { \"i\" : 1 } }"
        doc = ArangoDB.log_put("#{prefix}-by-example1", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(4)
        doc.parsed_response['count'].should eq(4)
        doc.parsed_response['result'].map{|i| i['_id']}.should =~ [d1,d2,d3,d4]

        cmd = api + "/by-example"
        body = "{ \"collection\" : \"#{@cn}\", \"example\" : { \"a\" : { \"j\" : 1 } } }"
        doc = ArangoDB.log_put("#{prefix}-by-example2", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(1)
        doc.parsed_response['count'].should eq(1)
        doc.parsed_response['result'].map{|i| i['_id']}.should =~ [d2]

        cmd = api + "/by-example"
        body = "{ \"collection\" : \"#{@cn}\", \"example\" : { \"a.j\" : 1 } }"
        doc = ArangoDB.log_put("#{prefix}-by-example3", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(2)
        doc.parsed_response['count'].should eq(2)
        doc.parsed_response['result'].map{|i| i['_id']}.should =~ [d2,d3]

        cmd = api + "/first-example"
        body = "{ \"collection\" : \"#{@cn}\", \"example\" : { \"a.j\" : 1, \"a.k\" : 1 } }"
        doc = ArangoDB.log_put("#{prefix}-first-example", cmd, :body => body)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['document']['_id'].should eq(d3)

        cmd = api + "/first-example"
        body = "{ \"collection\" : \"#{@cn}\", \"example\" : { \"a.j\" : 1, \"a.k\" : 2 } }"
        doc = ArangoDB.log_put("#{prefix}-first-example-not-found", cmd, :body => body)

        doc.code.should eq(404)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(404)
      end

      it "finds the first example, invalid collection" do
        cmd = api + "/first-example"
        body = "{ \"collection\" : \"NonExistingCollection\", \"example\" : { \"a\" : 1} }"
        doc = ArangoDB.log_put("#{prefix}-first-first-example-not-found", cmd, :body => body)

        doc.code.should eq(404)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(404)
        doc.parsed_response['errorNum'].should eq(1203)
      end
    end

################################################################################
## by-example query with skip / limit
################################################################################

    context "by-example query with skip:" do
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

      it "finds the examples with skip" do
        body = "{ \"someAttribute\" : \"someValue\", \"someOtherAttribute\" : \"someOtherValue\" }"
        doc = ArangoDB.post("/_api/document?collection=#{@cn}", :body => body)
        doc.code.should eq(202)

        body = "{ \"someAttribute\" : \"someValue\", \"someOtherAttribute2\" : \"someOtherValue2\" }"
        doc = ArangoDB.post("/_api/document?collection=#{@cn}", :body => body)
        doc.code.should eq(202)

        cmd = api + "/by-example"
        body = "{ \"collection\" : \"#{@cn}\", \"example\" : { \"someAttribute\" : \"someValue\" } }"
        doc = ArangoDB.log_put("#{prefix}-by-example-skip", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(2)
        doc.parsed_response['count'].should eq(2)

        body = "{ \"collection\" : \"#{@cn}\", \"example\" : { \"someAttribute\" : \"someValue\" }, \"skip\" : 1 }"
        doc = ArangoDB.log_put("#{prefix}-by-example-skip", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(1)
        doc.parsed_response['count'].should eq(1)
        
        body = "{ \"collection\" : \"#{@cn}\", \"example\" : { \"someAttribute\" : \"someValue\" }, \"skip\" : 1, \"limit\" : 1 }"
        doc = ArangoDB.log_put("#{prefix}-by-example-skip", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(1)
        doc.parsed_response['count'].should eq(1)
        
        body = "{ \"collection\" : \"#{@cn}\", \"example\" : { \"someAttribute\" : \"someValue\" }, \"skip\" : 2 }"
        doc = ArangoDB.log_put("#{prefix}-by-example-skip", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(0)
        doc.parsed_response['count'].should eq(0)
      end
    end

  end
end
