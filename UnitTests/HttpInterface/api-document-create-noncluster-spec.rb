# coding: utf-8

require 'rspec'
require 'arangodb.rb'

describe ArangoDB do
  prefix = "rest-create-document"
  didRegex = /^([0-9a-zA-Z]+)\/([0-9a-zA-Z\-_]+)/

  context "creating a document:" do

################################################################################
## unknown collection name
################################################################################

    context "unknown collection name:" do
      before do
        @cn = "UnitTestsCollectionNamed#{Time.now.to_i}"
      end

      after do
        ArangoDB.drop_collection(@cn)
      end

      it "create the collection and the document" do
        cmd = "/_api/document?collection=#{@cn}&createCollection=true"
        body = "{ \"Hallo\" : \"World\" }"
        doc = ArangoDB.log_post("#{prefix}-create-collection", cmd, :body => body, :headers => { "x-arango-version" => "1.3" })

        doc.code.should eq(202)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)

        etag = doc.headers['etag']
        etag.should be_kind_of(String)

        location = doc.headers['location']
        location.should be_kind_of(String)

        rev = doc.parsed_response['_rev']
        rev.should be_kind_of(String)

        did = doc.parsed_response['_id']
        did.should be_kind_of(String)
        
        etag.should eq("\"#{rev}\"")
        location.should eq("/_api/document/#{did}")

        ArangoDB.delete(location)

        ArangoDB.size_collection(@cn).should eq(0)
      end

      it "create the collection and the document, setting compatibility header" do
        cmd = "/_api/document?collection=#{@cn}&createCollection=true"
        body = "{ \"Hallo\" : \"World\" }"
        doc = ArangoDB.log_post("#{prefix}-create-collection", cmd, :body => body, :headers => { "x-arango-version" => "1.4" })

        doc.code.should eq(202)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)

        etag = doc.headers['etag']
        etag.should be_kind_of(String)

        location = doc.headers['location']
        location.should be_kind_of(String)

        rev = doc.parsed_response['_rev']
        rev.should be_kind_of(String)

        did = doc.parsed_response['_id']
        did.should be_kind_of(String)
        
        etag.should eq("\"#{rev}\"")
        location.should eq("/_db/_system/_api/document/#{did}")

        ArangoDB.delete(location)

        ArangoDB.size_collection(@cn).should eq(0)
      end
    end

  end
end
