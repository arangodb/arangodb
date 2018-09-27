# coding: utf-8

require 'rspec'
require 'arangodb.rb'

describe ArangoDB do
  api = "/_api/collection"
  prefix = "api-collection"

  context "reading:" do
    before do
      @cn = "UnitTestsCollectionAccess"
      ArangoDB.drop_collection(@cn)
      @cid = ArangoDB.create_collection(@cn)
    end

    after do
      ArangoDB.drop_collection(@cn)
    end

    it "finds the collection by identifier" do
      cmd = api + "/" + String(@cid)
      doc = ArangoDB.log_get("#{prefix}-get-collection-access", cmd)

      doc.code.should eq(200)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")
      doc.parsed_response['error'].should eq(false)
      doc.parsed_response['code'].should eq(200)
      doc.parsed_response['id'].should eq(@cid)
      doc.parsed_response['name'].should eq(@cn)
    end
    
    it "retrieves the collection's figures by identifier" do
      cmd = api + "/" + String(@cid) + "/figures"
      doc = ArangoDB.log_get("#{prefix}-get-collection-access-figures", cmd)

      doc.code.should eq(200)
      doc.parsed_response['error'].should eq(false)
      doc.parsed_response['code'].should eq(200)
      doc.parsed_response['id'].should eq(@cid)
      doc.parsed_response['name'].should eq(@cn)
    end

  end
end
