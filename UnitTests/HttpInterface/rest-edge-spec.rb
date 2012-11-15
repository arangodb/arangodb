# coding: utf-8

require 'rspec'
require 'uri'
require './arangodb.rb'

describe ArangoDB do
  prefix = "rest-edge"

  context "creating an edge:" do

################################################################################
## error handling
################################################################################

    context "error handling:" do
      it "returns an error if url is missing from" do
	cn = "UnitTestsCollectionEdge"
	cmd = "/_api/edge?collection=#{cn}&createCollection=true"
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
	cmd = "/_api/edge?collection=#{cn}&createCollection=true&from=1&to=1"
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
	cmd = "/_api/edges/#{@cid}?vertex="
        doc = ArangoDB.log_get("#{prefix}-missing-handle", cmd)

	doc.code.should eq(400)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(400)
	doc.parsed_response['code'].should eq(400)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	ArangoDB.drop_collection(cn)
      end
    end

################################################################################
## known collection name
################################################################################

    context "known collection name:" do
      before do
	@ce = "UnitTestsCollectionEdge"
	@eid = ArangoDB.create_collection(@ce, true, 3) # type 3 = edge collection
	@cv = "UnitTestsCollectionVertex"
	@vid = ArangoDB.create_collection(@cv, true, 2) # type 2 = document collection
      end

      after do
	ArangoDB.drop_collection(@ce)
	ArangoDB.drop_collection(@cv)
      end

      it "creating an edge" do
	cmd = "/_api/document?collection=#{@vid}"

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
	cmd = "/_api/edge?collection=#{@eid}&from=#{id1}&to=#{id2}"
	body = "{}"
        doc = ArangoDB.log_post("#{prefix}-create-edge", cmd, :body => body)

	doc.code.should eq(201)
	doc.parsed_response['_id'].should be_kind_of(String)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	id3 = doc.parsed_response['_id']

	# check edge
	cmd = "/_api/edge/#{id3}"
        doc = ArangoDB.log_get("#{prefix}-read-edge", cmd)

	doc.code.should eq(200)
	doc.parsed_response['_id'].should eq(id3)
	doc.parsed_response['_from'].should eq(id1)
	doc.parsed_response['_to'].should eq(id2)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")
	
	# create another edge
	cmd = "/_api/edge?collection=#{@eid}&from=#{id1}&to=#{id2}"
	body = "{ \"e\" : 1 }"
        doc = ArangoDB.log_post("#{prefix}-create-edge", cmd, :body => body)

	doc.code.should eq(201)
	doc.parsed_response['_id'].should be_kind_of(String)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	id4 = doc.parsed_response['_id']

	# check edge
	cmd = "/_api/edge/#{id4}"
        doc = ArangoDB.log_get("#{prefix}-read-edge", cmd)

	doc.code.should eq(200)
	doc.parsed_response['_id'].should eq(id4)
	doc.parsed_response['_from'].should eq(id1)
	doc.parsed_response['_to'].should eq(id2)
	doc.parsed_response['e'].should eq(1)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	# create third edge
	cmd = "/_api/edge?collection=#{@eid}&from=#{id2}&to=#{id1}"
	body = "{ \"e\" : 2 }"
        doc = ArangoDB.log_post("#{prefix}-create-edge", cmd, :body => body)

	doc.code.should eq(201)
	doc.parsed_response['_id'].should be_kind_of(String)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	id5 = doc.parsed_response['_id']

	# check edge
	cmd = "/_api/edge/#{id5}"
        doc = ArangoDB.log_get("#{prefix}-read-edge", cmd)

	doc.code.should eq(200)
	doc.parsed_response['_id'].should eq(id5)
	doc.parsed_response['_from'].should eq(id2)
	doc.parsed_response['_to'].should eq(id1)
	doc.parsed_response['e'].should eq(2)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	# check ANY edges
	cmd = "/_api/edges/#{@eid}?vertex=#{id1}"
	doc = ArangoDB.log_get("#{prefix}-read-edges-any", cmd);

	doc.code.should eq(200)
	doc.parsed_response['edges'].should be_kind_of(Array)
	doc.parsed_response['edges'].length.should be(3)

	# check IN edges
	cmd = "/_api/edges/#{@eid}?vertex=#{id1}&direction=in"
	doc = ArangoDB.log_get("#{prefix}-read-edges-in", cmd);

	doc.code.should eq(200)
	doc.parsed_response['edges'].should be_kind_of(Array)
	doc.parsed_response['edges'].length.should be(1)

	# check OUT edges
	cmd = "/_api/edges/#{@eid}?vertex=#{id1}&direction=out"
	doc = ArangoDB.log_get("#{prefix}-read-edges-out", cmd);

	doc.code.should eq(200)
	doc.parsed_response['edges'].should be_kind_of(Array)
	doc.parsed_response['edges'].length.should be(2)
      end

      it "using collection names in from and to" do
	cmd = "/_api/document?collection=#{@vid}"

	# create a vertex
	body = "{ \"a\" : 1 }"
	doc = ArangoDB.log_post("#{prefix}-create-edge-named", cmd, :body => body)

	doc.code.should eq(201)
	doc.parsed_response['_id'].should be_kind_of(String)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	id = doc.parsed_response['_id']
        # replace collection id with collection name
        translated = URI.escape(id.gsub /^\d+\//, 'UnitTestsCollectionVertex/')
	
        # create edge, using collection id
	cmd = "/_api/edge?collection=#{@eid}&from=#{id}&to=#{id}"
	body = "{}"
        doc = ArangoDB.log_post("#{prefix}-create-edge-named", cmd, :body => body)

	doc.code.should eq(201)
	doc.parsed_response['_id'].should be_kind_of(String)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

        # create edge, using collection names
	cmd = "/_api/edge?collection=#{@eid}&from=#{translated}&to=#{translated}"
	body = "{}"
        doc = ArangoDB.log_post("#{prefix}-create-edge-named", cmd, :body => body)

	doc.code.should eq(201)
	doc.parsed_response['_id'].should be_kind_of(String)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")
        
        # create edge, using mix of collection names and ids
	cmd = "/_api/edge?collection=#{@eid}&from=#{translated}&to=#{id}"
	body = "{}"
        doc = ArangoDB.log_post("#{prefix}-create-edge-named", cmd, :body => body)

	doc.code.should eq(201)
	doc.parsed_response['_id'].should be_kind_of(String)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")
	
        # turn parameters around
        cmd = "/_api/edge?collection=#{@eid}&from=#{id}&to=#{translated}"
	body = "{}"
        doc = ArangoDB.log_post("#{prefix}-create-edge-named", cmd, :body => body)

	doc.code.should eq(201)
	doc.parsed_response['_id'].should be_kind_of(String)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")
      end
      
      it "using invalid collection names" do
	cmd = "/_api/document?collection=#{@vid}"

	# create a vertex
	body = "{ \"a\" : 1 }"
	doc = ArangoDB.log_post("#{prefix}-create-edge-invalid-name", cmd, :body => body)

	doc.code.should eq(201)
	doc.parsed_response['_id'].should be_kind_of(String)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	from = URI.escape("thefox/12345")
        to   = URI.escape("thefox/13443466")
	
        # create edge, using invalid collection names
	cmd = "/_api/edge?collection=#{@eid}&from=#{from}&to=#{to}"
	body = "{}"
        doc = ArangoDB.log_post("#{prefix}-create-edge-invalid-name", cmd, :body => body)

	doc.code.should eq(404)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(1203)
	doc.parsed_response['code'].should eq(404)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")
      end

      it "using invalid collection ids" do
	cmd = "/_api/document?collection=#{@vid}"

	# create a vertex
	body = "{ \"a\" : 1 }"
	doc = ArangoDB.log_post("#{prefix}-create-edge-invalid-cid", cmd, :body => body)

	doc.code.should eq(201)
	doc.parsed_response['_id'].should be_kind_of(String)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	from = "2/12345"
        to   = "3/13443466"
	
        # create edge, using invalid collection names
	cmd = "/_api/edge?collection=#{@eid}&from=#{from}&to=#{to}"
	body = "{}"
        doc = ArangoDB.log_post("#{prefix}-create-edge-invalid-cid", cmd, :body => body)

	doc.code.should eq(404)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(1203)
	doc.parsed_response['code'].should eq(404)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")
      end
    end

################################################################################
## known collection name, waitForSync URL param
################################################################################

    context "known collection name:" do
      before do
	@ce = "UnitTestsCollectionEdge"
	@eid = ArangoDB.create_collection(@ce, false, 3) # type 3 = edge collection
	@cv = "UnitTestsCollectionVertex"
	@vid = ArangoDB.create_collection(@cv, true, 2) # type 2 = document collection
      end

      after do
	ArangoDB.drop_collection(@ce)
	ArangoDB.drop_collection(@cv)
      end
      
      it "creating an edge, waitForSync URL param=false" do
	# create first vertex
	cmd = "/_api/document?collection=#{@vid}"
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
	cmd = "/_api/edge?collection=#{@eid}&from=#{id1}&to=#{id2}&waitForSync=false"
	body = "{}"
        doc = ArangoDB.log_post("#{prefix}-create-edge", cmd, :body => body)

	doc.code.should eq(202)
	doc.parsed_response['_id'].should be_kind_of(String)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	id3 = doc.parsed_response['_id']

	# check edge
	cmd = "/_api/edge/#{id3}"
        doc = ArangoDB.log_get("#{prefix}-read-edge", cmd)

	doc.code.should eq(200)
	doc.parsed_response['_id'].should eq(id3)
	doc.parsed_response['_from'].should eq(id1)
	doc.parsed_response['_to'].should eq(id2)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")
      end
      
      it "creating an edge, waitForSync URL param=true" do
	# create first vertex
	cmd = "/_api/document?collection=#{@vid}"
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
	cmd = "/_api/edge?collection=#{@eid}&from=#{id1}&to=#{id2}&waitForSync=true"
	body = "{}"
        doc = ArangoDB.log_post("#{prefix}-create-edge", cmd, :body => body)

	doc.code.should eq(201)
	doc.parsed_response['_id'].should be_kind_of(String)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	id3 = doc.parsed_response['_id']

	# check edge
	cmd = "/_api/edge/#{id3}"
        doc = ArangoDB.log_get("#{prefix}-read-edge", cmd)

	doc.code.should eq(200)
	doc.parsed_response['_id'].should eq(id3)
	doc.parsed_response['_from'].should eq(id1)
	doc.parsed_response['_to'].should eq(id2)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")
      end
    end

  end
end

