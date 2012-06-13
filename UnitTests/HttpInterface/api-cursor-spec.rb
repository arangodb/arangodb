# coding: utf-8

require 'rspec'
require './arangodb.rb'

describe ArangoDB do
  api = "/_api/cursor"
  prefix = "api-cursor"

  context "dealing with cursors:" do

################################################################################
## error handling
################################################################################

    context "error handling:" do
      it "returns an error if body is missing" do
	cmd = api
	doc = ArangoDB.log_post("#{prefix}-missing-body", cmd)
	
	doc.code.should eq(400)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['code'].should eq(400)
	doc.parsed_response['errorNum'].should eq(1502)
      end

      it "returns an error if collection is unknown" do
	cmd = api
	body = "{ \"query\" : \"FOR u IN unknowncollection LIMIT 2 RETURN u.n\", \"count\" : true, \"bindVars\" : {}, \"batchSize\" : 2 }"
	doc = ArangoDB.log_post("#{prefix}-unknown-collection", cmd, :body => body)
	
	doc.code.should eq(400)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['code'].should eq(400)
	doc.parsed_response['errorNum'].should eq(1520)
      end

      it "returns an error if cursor identifier is missing" do
	cmd = api
	doc = ArangoDB.log_put("#{prefix}-missing-cursor-identifier", cmd)
	
	doc.code.should eq(400)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['code'].should eq(400)
	doc.parsed_response['errorNum'].should eq(400)
      end

      it "returns an error if cursor identifier is invalid" do
	cmd = api + "/123456"
	doc = ArangoDB.log_put("#{prefix}-invalid-cursor-identifier", cmd)
	
	doc.code.should eq(400)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['code'].should eq(400)
	doc.parsed_response['errorNum'].should eq(1600)
      end

    end

################################################################################
## create and using cursors
################################################################################

    context "handling a cursor:" do
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

      it "creates a cursor single run" do
	cmd = api
	body = "{ \"query\" : \"FOR u IN #{@cn} LIMIT 2 RETURN u.n\", \"count\" : true, \"bindVars\" : {}, \"batchSize\" : 2 }"
	doc = ArangoDB.log_post("#{prefix}-create-for-limit-return-single", cmd, :body => body)
	
	doc.code.should eq(201)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(201)
	doc.parsed_response['hasMore'].should eq(false)
	doc.parsed_response['count'].should eq(2)
	doc.parsed_response['result'].length.should eq(2)
      end
      
      it "creates a cursor single run, without count" do
	cmd = api
	body = "{ \"query\" : \"FOR u IN #{@cn} LIMIT 2 RETURN u.n\", \"count\" : false, \"bindVars\" : {} }"
	doc = ArangoDB.log_post("#{prefix}-create-for-limit-return-single", cmd, :body => body)
	
	doc.code.should eq(201)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(201)
	doc.parsed_response['hasMore'].should eq(false)
	doc.parsed_response['count'].should eq(nil)
	doc.parsed_response['result'].length.should eq(2)
      end

      it "creates a cursor single run, large batch size" do
	cmd = api
	body = "{ \"query\" : \"FOR u IN #{@cn} LIMIT 2 RETURN u.n\", \"count\" : true, \"batchSize\" : 5 }"
	doc = ArangoDB.log_post("#{prefix}-create-for-limit-return-single-larger", cmd, :body => body)
	
	doc.code.should eq(201)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(201)
	doc.parsed_response['hasMore'].should eq(false)
	doc.parsed_response['count'].should eq(2)
	doc.parsed_response['result'].length.should eq(2)
      end

      it "creates a cursor" do
	cmd = api
	body = "{ \"query\" : \"FOR u IN #{@cn} LIMIT 5 RETURN u.n\", \"count\" : true, \"batchSize\" : 2 }"
	doc = ArangoDB.log_post("#{prefix}-create-for-limit-return", cmd, :body => body)
	
	doc.code.should eq(201)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(201)
	doc.parsed_response['id'].should be_kind_of(Integer)
	doc.parsed_response['hasMore'].should eq(true)
	doc.parsed_response['count'].should eq(5)
	doc.parsed_response['result'].length.should eq(2)

	id = doc.parsed_response['id']

	cmd = api + "/#{id}"
	doc = ArangoDB.log_put("#{prefix}-create-for-limit-return-cont", cmd)
	
	doc.code.should eq(200)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.parsed_response['hasMore'].should eq(true)
	doc.parsed_response['count'].should eq(5)
	doc.parsed_response['result'].length.should eq(2)

	cmd = api + "/#{id}"
	doc = ArangoDB.log_put("#{prefix}-create-for-limit-return-cont2", cmd)
	
	doc.code.should eq(200)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.parsed_response['hasMore'].should eq(false)
	doc.parsed_response['count'].should eq(5)
	doc.parsed_response['result'].length.should eq(1)

	cmd = api + "/#{id}"
	doc = ArangoDB.log_put("#{prefix}-create-for-limit-return-cont3", cmd)
	
	doc.code.should eq(400)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(1600)
	doc.parsed_response['code'].should eq(400)
      end

      it "deleting a cursor" do
	cmd = api
	body = "{ \"query\" : \"FOR u IN #{@cn} LIMIT 5 RETURN u.n\", \"count\" : true, \"batchSize\" : 2 }"
	doc = ArangoDB.post(cmd, :body => body)
	
	doc.code.should eq(201)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(201)
	doc.parsed_response['id'].should be_kind_of(Integer)
	doc.parsed_response['hasMore'].should eq(true)
	doc.parsed_response['count'].should eq(5)
	doc.parsed_response['result'].length.should eq(2)

	id = doc.parsed_response['id']

	cmd = api + "/#{id}"
	doc = ArangoDB.log_delete("#{prefix}-delete", cmd)

	doc.code.should eq(202)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(202)
      end
    end

################################################################################
## checking a query
################################################################################

    context "checking a query:" do
      before do
	@cn = "users"
	ArangoDB.drop_collection(@cn)
	@cid = ArangoDB.create_collection(@cn, false)
      end

      after do
	ArangoDB.drop_collection(@cn)
      end

      it "valid query" do
	cmd = "/_api/query"
	body = "{ \"query\" : \"FOR u IN #{@cn} FILTER u.name == @name LIMIT 2 RETURN u.n\" }"
	doc = ArangoDB.log_post("api-query-valid", cmd, :body => body)
	
	doc.code.should eq(200)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.parsed_response['bindVars'].should eq(["name"])
      end

      it "invalid query" do
	cmd = "/_api/query"
	body = "{ \"query\" : \"FOR u IN #{@cn} FILTER u.name = @name LIMIT 2 RETURN u.n\" }"
	doc = ArangoDB.log_post("api-query-invalid", cmd, :body => body)
	
	doc.code.should eq(400)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['code'].should eq(400)
	doc.parsed_response['errorNum'].should eq(1501)
      end
    end

  end
end
