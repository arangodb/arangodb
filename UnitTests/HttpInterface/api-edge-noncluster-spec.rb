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
      
      it "returns an error if from/to are incomplete" do
        cn = "UnitTestsCollectionEdge"
        cmd = "/_api/edge?collection=#{cn}&createCollection=true&from=test&to=test"
        body = "{}"
        doc = ArangoDB.log_post("#{prefix}-incomplete-from-to", cmd, :body => body)

        doc.code.should eq(400)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(1205)
        doc.parsed_response['code'].should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        ArangoDB.drop_collection(cn)
      end
    end

  end
end

