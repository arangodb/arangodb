# coding: utf-8

require 'rspec'
require './arangodb.rb'

describe ArangoDB do
  api = "/_api/simple"
  prefix = "api-simple"

  context "simple queries:" do

################################################################################
## any query
################################################################################

    context "any query:" do
      before do
        @cn = "UnitTestsCollectionSimple"
        ArangoDB.drop_collection(@cn)
        @cid = ArangoDB.create_collection(@cn, false)

        (0...10).each{|i|
          ArangoDB.post("/_api/document?collection=#{@cid}", :body => "{ \"n\" : #{i} }")
        }
      end

      after do
        ArangoDB.drop_collection(@cn)
      end

      it "get any documents" do
        cmd = api + "/any"
        body = "{ \"collection\" : \"#{@cid}\" }"
        doc = ArangoDB.log_put("#{prefix}-any", cmd, :body => body)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['document']['n'].should be_kind_of(Integer)
      end
    end

################################################################################
## first / last
################################################################################

    context "first and last queries:" do
      before do
        @cn = "UnitTestsCollectionByExample"
        ArangoDB.drop_collection(@cn)
        @cid = ArangoDB.create_collection(@cn, false)
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

        
        cmd = api + "/first"
        body = "{ \"collection\" : \"#{@cn}\" }"
        doc = ArangoDB.log_put("#{prefix}-first1", cmd, :body => body)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['result']['_id'].should eq(d1)
        
        cmd = api + "/first"
        body = "{ \"collection\" : \"#{@cn}\", \"count\" : 2 }"
        doc = ArangoDB.log_put("#{prefix}-first2", cmd, :body => body)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['result'].length.should eq(2)
        doc.parsed_response['result'].map{|i| i['_id']}.should =~ [d1,d2]

        cmd = api + "/last"
        body = "{ \"collection\" : \"#{@cn}\" }"
        doc = ArangoDB.log_put("#{prefix}-last1", cmd, :body => body)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['result']['_id'].should eq(d7)
        
        cmd = api + "/last"
        body = "{ \"collection\" : \"#{@cn}\", \"count\" : 2 }"
        doc = ArangoDB.log_put("#{prefix}-last2", cmd, :body => body)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['result'].length.should eq(2)
        doc.parsed_response['result'].map{|i| i['_id']}.should =~ [d7,d6]
      end
    end

  end
end
