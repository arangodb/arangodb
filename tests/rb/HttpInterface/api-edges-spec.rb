# coding: utf-8

require 'rspec'
require 'uri'
require 'arangodb.rb'

describe ArangoDB do
  prefix = "rest-edge"

  context "querying edges:" do

################################################################################
## error handling
################################################################################

    context "error handling:" do
      it "returns an error if vertex identifier is missing" do
        cn = "UnitTestsCollectionEdge"
        ArangoDB.drop_collection(cn)
        ArangoDB.create_collection(cn, true, 3) # type 3 = edge collection
        cmd = "/_api/edges/#{cn}"
        doc = ArangoDB.log_get("#{prefix}-missing-identifier", cmd)

        doc.code.should eq(400)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(1205)
        doc.parsed_response['code'].should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        ArangoDB.drop_collection(cn)
      end

      it "returns an error if vertex identifier is empty" do
        cn = "UnitTestsCollectionEdge"
        ArangoDB.drop_collection(cn)
        ArangoDB.create_collection(cn, true, 3) # type 3 = edge collection
        cmd = "/_api/edges/#{cn}?vertex="
        doc = ArangoDB.log_get("#{prefix}-empty-identifier", cmd)

        doc.code.should eq(400)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(1205)
        doc.parsed_response['code'].should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        ArangoDB.drop_collection(cn)
      end
      
      it "returns an error if collection does not exist" do
        cn = "UnitTestsCollectionEdge"
        ArangoDB.drop_collection(cn)
        cmd = "/_api/edges/#{cn}?vertex=test"
        doc = ArangoDB.log_get("#{prefix}-no-collection-query", cmd)

        doc.code.should eq(404)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(1203)
        doc.parsed_response['code'].should eq(404)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
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
    
        @reFull = Regexp.new('^[a-zA-Z0-9_\-]+/\d+$')
        @reEdge = Regexp.new('^' + @ce + '/')
        @reVertex = Regexp.new('^' + @cv + '/')
      end

      after do
        ArangoDB.drop_collection(@ce)
        ArangoDB.drop_collection(@cv)
      end

      it "creating an edge" do
        cmd = "/_api/document?collection=#{@cv}"

        # create first vertex
        body = "{ \"a\" : 1 }"
        doc = ArangoDB.log_post("#{prefix}-create-edge", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['_id'].should be_kind_of(String)
        doc.parsed_response['_id'].should match(@reFull)
        doc.parsed_response['_id'].should match(@reVertex)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        id1 = doc.parsed_response['_id']

        # create second vertex
        body = "{ \"a\" : 2 }"
        doc = ArangoDB.log_post("#{prefix}-create-edge", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['_id'].should be_kind_of(String)
        doc.parsed_response['_id'].should match(@reFull)
        doc.parsed_response['_id'].should match(@reVertex)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        id2 = doc.parsed_response['_id']

        # create edge
        cmd = "/_api/document?collection=#{@ce}"
        body = "{\"_from\":\"#{id1}\",\"_to\":\"#{id2}\"}"
        doc = ArangoDB.log_post("#{prefix}-create-edge", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['_id'].should be_kind_of(String)
        doc.parsed_response['_id'].should match(@reFull)
        doc.parsed_response['_id'].should match(@reEdge)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        id3 = doc.parsed_response['_id']

        # check edge
        cmd = "/_api/document/#{id3}"
        doc = ArangoDB.log_get("#{prefix}-read-edge", cmd)

        doc.code.should eq(200)
        doc.parsed_response['_id'].should eq(id3)
        doc.parsed_response['_from'].should eq(id1)
        doc.parsed_response['_to'].should eq(id2)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        
        # create another edge
        cmd = "/_api/document?collection=#{@ce}"
        body = "{ \"e\" : 1, \"_from\" : \"#{id1}\", \"_to\": \"#{id2}\" }"
        doc = ArangoDB.log_post("#{prefix}-create-edge", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['_id'].should be_kind_of(String)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        id4 = doc.parsed_response['_id']

        # check edge
        cmd = "/_api/document/#{id4}"
        doc = ArangoDB.log_get("#{prefix}-read-edge", cmd)

        doc.code.should eq(200)
        doc.parsed_response['_id'].should eq(id4)
        doc.parsed_response['_id'].should match(@reFull)
        doc.parsed_response['_id'].should match(@reEdge)
        doc.parsed_response['_from'].should eq(id1)
        doc.parsed_response['_from'].should match(@reFull)
        doc.parsed_response['_from'].should match(@reVertex)
        doc.parsed_response['_to'].should eq(id2)
        doc.parsed_response['_to'].should match(@reFull)
        doc.parsed_response['_to'].should match(@reVertex)
        doc.parsed_response['e'].should eq(1)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        # create third edge
        cmd = "/_api/document?collection=#{@ce}"
        body = "{ \"e\" : 2, \"_from\" : \"#{id2}\", \"_to\" : \"#{id1}\" }"
        doc = ArangoDB.log_post("#{prefix}-create-edge", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['_id'].should be_kind_of(String)
        doc.parsed_response['_id'].should match(@reFull)
        doc.parsed_response['_id'].should match(@reEdge)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        id5 = doc.parsed_response['_id']

        # check edge
        cmd = "/_api/document/#{id5}"
        doc = ArangoDB.log_get("#{prefix}-read-edge", cmd)

        doc.code.should eq(200)
        doc.parsed_response['_id'].should eq(id5)
        doc.parsed_response['_id'].should match(@reFull)
        doc.parsed_response['_id'].should match(@reEdge)
        doc.parsed_response['_from'].should eq(id2)
        doc.parsed_response['_from'].should match(@reFull)
        doc.parsed_response['_from'].should match(@reVertex)
        doc.parsed_response['_to'].should eq(id1)
        doc.parsed_response['_to'].should match(@reFull)
        doc.parsed_response['_to'].should match(@reVertex)
        doc.parsed_response['e'].should eq(2)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        # check ANY edges
        cmd = "/_api/edges/#{@ce}?vertex=#{id1}"
        doc = ArangoDB.log_get("#{prefix}-read-edges-any", cmd);

        doc.code.should eq(200)
        doc.parsed_response['edges'].should be_kind_of(Array)
        doc.parsed_response['edges'].length.should eq(3)

        # check IN edges
        cmd = "/_api/edges/#{@ce}?vertex=#{id1}&direction=in"
        doc = ArangoDB.log_get("#{prefix}-read-edges-in", cmd);

        doc.code.should eq(200)
        doc.parsed_response['edges'].should be_kind_of(Array)
        doc.parsed_response['edges'].length.should eq(1)

        # check OUT edges
        cmd = "/_api/edges/#{@ce}?vertex=#{id1}&direction=out"
        doc = ArangoDB.log_get("#{prefix}-read-edges-out", cmd);

        doc.code.should eq(200)
        doc.parsed_response['edges'].should be_kind_of(Array)
        doc.parsed_response['edges'].length.should eq(2)
      end

    end

  end
end

