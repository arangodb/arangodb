# coding: utf-8

require 'rspec'
require 'arangodb.rb'

describe ArangoDB do
  prefix = "rest-create-document"
  didRegex = /^([0-9a-zA-Z]+)\/([0-9a-zA-Z\-_]+)/

  context "creating documents via replicaiton API:" do

################################################################################
## error handling
################################################################################

    context "import handling:" do
      it "is able to import json documents" do
        cn = "UnitTestsCollectionBasics"
        id = ArangoDB.create_collection(cn)

        cmd = "/_api/replication/restore-data?collection=#{cn}"
        body =
          "{ \"type\" : 2300, \"data\" : { \"_key\" : \"1234xxx1\",\"_rev\":\"_W2GDlX--_j\" , \"foo\" : \"bar1\" }}\n" \
          "{ \"type\" : 2300, \"data\" : { \"_key\" : \"1234xxx2\",\"_rev\":\"_W2GDlX--_k\" , \"foo\" : \"bar1\" }}"

        doc = ArangoDB.log_put("#{prefix}-bad-json", cmd, :body => body)

        doc.code.should eq(200)

        ArangoDB.size_collection(cn).should eq(2)
        ArangoDB.drop_collection(cn)
      end

      it "returns an error if an string attribute in the JSON body is corrupted" do
        cn = "UnitTestsCollectionBasics"
        id = ArangoDB.create_collection(cn)

        cmd = "/_api/replication/restore-data?collection=#{cn}"
        body =
          "{ \"type\" : 2300, \"data\" : { \"_key\" : \"1234xxx1\",\"_rev\":\"_W2GDlX--_j\" , \"foo\" : \"bar1\" }}\n" \
          "{ \"type\" : 2300, \"data\" : { \"_key\" : \"1234xxx2\",\"_rev\":\"_W2GDlX--_k\" , \"foo\" : \"bar1\" }}\n" \
          "{ \"type\" : 2300, \"data\" : { \"_key\" : \"1234xxx3\",\"_rev\":\"_W2GDlX--_l\" , \"foo\" : \"bar\xff\" }}"

				doc = ArangoDB.log_put("#{prefix}-bad-json", cmd, :body => body)

        doc.code.should eq(400)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(600)
        doc.parsed_response['code'].should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        ArangoDB.size_collection(cn).should eq(0)
        ArangoDB.drop_collection(cn)
      end

    end # error handling
  end # context - end
end # decribe - end
