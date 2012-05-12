# coding: utf-8

require 'rspec'
require './arangodb.rb'

describe ArangoDB do
  prefix = "rest_edge"

  context "creating an edge:" do

################################################################################
## error handling
################################################################################

    context "error handling:" do
      it "returns an error if url is missing from" do
	cn = "UnitTestsCollectionEdge"
	cmd = "/edge?collection=#{cn}&createCollection=true"
	body = "{}"
        doc = ArangoDB.log_post("#{prefix}-missing-from-to", cmd, :body => body)

	doc.code.should eq(400)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(400)
	doc.parsed_response['code'].should eq(400)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	ArangoDB.drop_collection(cn)
      end

      it "returns an error if from/to are malformed" do
	cn = "UnitTestsCollectionEdge"
	cmd = "/edge?collection=#{cn}&createCollection=true&from=1&to=1"
	body = "{}"
        doc = ArangoDB.log_post("#{prefix}-bad-from-to", cmd, :body => body)

	doc.code.should eq(400)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(1205)
	doc.parsed_response['code'].should eq(400)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	ArangoDB.drop_collection(cn)
      end

      it "returns an error if vertex-handle is missing" do
	cn = "UnitTestsCollectionEdge"
	cmd = "/edges/#{@cid}?vertex="
        doc = ArangoDB.log_get("#{prefix}-missing-handle", cmd)

	doc.code.should eq(400)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(400)
	doc.parsed_response['code'].should eq(400)
	doc.headers['content-type'].should eq("application/json")

	ArangoDB.drop_collection(cn)
      end
    end

################################################################################
## known collection name
################################################################################

    context "known collection name:" do
      before do
	@cn = "UnitTestsCollectionEdge"
	@cid = ArangoDB.create_collection(@cn)
      end

      after do
	ArangoDB.drop_collection(@cn)
      end

      it "creating an edge" do
	cmd = "/document?collection=#{@cid}"

	# create first vertex
	body = "{ \"a\" : 1 }"
	doc = ArangoDB.log_post("#{prefix}-create-edge", cmd, :body => body)

	doc.code.should eq(201)
	doc.parsed_response['_id'].should be_kind_of(String)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	id1 = doc.parsed_response['_id']

	# create second vertex
	body = "{ \"a\" : 2 }"
	doc = ArangoDB.log_post("#{prefix}-create-edge", cmd, :body => body)

	doc.code.should eq(201)
	doc.parsed_response['_id'].should be_kind_of(String)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	id2 = doc.parsed_response['_id']

	# create edge
	cmd = "/edge?collection=#{@cid}&from=#{id1}&to=#{id2}"
	body = "{}"
        doc = ArangoDB.log_post("#{prefix}-create-edge", cmd, :body => body)

	doc.code.should eq(201)
	doc.parsed_response['_id'].should be_kind_of(String)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	id3 = doc.parsed_response['_id']

	# check edge
	cmd = "/edge/#{id3}"
        doc = ArangoDB.log_get("#{prefix}-read-edge", cmd)

	doc.code.should eq(200)
	doc.parsed_response['_id'].should eq(id3)
	doc.parsed_response['_from'].should eq(id1)
	doc.parsed_response['_to'].should eq(id2)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")
	
	# create another edge
	cmd = "/edge?collection=#{@cid}&from=#{id1}&to=#{id2}"
	body = "{ \"e\" : 1 }"
        doc = ArangoDB.log_post("#{prefix}-create-edge", cmd, :body => body)

	doc.code.should eq(201)
	doc.parsed_response['_id'].should be_kind_of(String)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	id4 = doc.parsed_response['_id']

	# check edge
	cmd = "/edge/#{id4}"
        doc = ArangoDB.log_get("#{prefix}-read-edge", cmd)

	doc.code.should eq(200)
	doc.parsed_response['_id'].should eq(id4)
	doc.parsed_response['_from'].should eq(id1)
	doc.parsed_response['_to'].should eq(id2)
	doc.parsed_response['e'].should eq(1)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	# create third edge
	cmd = "/edge?collection=#{@cid}&from=#{id2}&to=#{id1}"
	body = "{ \"e\" : 2 }"
        doc = ArangoDB.log_post("#{prefix}-create-edge", cmd, :body => body)

	doc.code.should eq(201)
	doc.parsed_response['_id'].should be_kind_of(String)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	id5 = doc.parsed_response['_id']

	# check edge
	cmd = "/edge/#{id5}"
        doc = ArangoDB.log_get("#{prefix}-read-edge", cmd)

	doc.code.should eq(200)
	doc.parsed_response['_id'].should eq(id5)
	doc.parsed_response['_from'].should eq(id2)
	doc.parsed_response['_to'].should eq(id1)
	doc.parsed_response['e'].should eq(2)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	# check ANY edges
	cmd = "/edges/#{@cid}?vertex=#{id1}"
	doc = ArangoDB.log_get("#{prefix}-read-edges-any", cmd);

	doc.code.should eq(200)
	doc.parsed_response['edges'].should be_kind_of(Array)
	doc.parsed_response['edges'].length.should be(3)

	# check IN edges
	cmd = "/edges/#{@cid}?vertex=#{id1}&direction=in"
	doc = ArangoDB.log_get("#{prefix}-read-edges-in", cmd);

	doc.code.should eq(200)
	doc.parsed_response['edges'].should be_kind_of(Array)
	doc.parsed_response['edges'].length.should be(1)

	# check OUT edges
	cmd = "/edges/#{@cid}?vertex=#{id1}&direction=out"
	doc = ArangoDB.log_get("#{prefix}-read-edges-out", cmd);

	doc.code.should eq(200)
	doc.parsed_response['edges'].should be_kind_of(Array)
	doc.parsed_response['edges'].length.should be(2)
      end
    end

  end
end


