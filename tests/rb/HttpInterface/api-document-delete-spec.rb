# coding: utf-8

require 'rspec'
require 'arangodb.rb'

describe ArangoDB do
  prefix = "rest-delete-document"

  context "delete a document:" do

################################################################################
## error handling
################################################################################

    context "error handling:" do
      before do
        @cn = "UnitTestsCollectionBasics"
        @cid = ArangoDB.create_collection(@cn)
      end

      after do
        ArangoDB.drop_collection(@cn)
      end

      it "returns an error if document identifier is missing" do
        cmd = "/_api/document"
        doc = ArangoDB.log_delete("#{prefix}-missing-identifier", cmd)

        doc.code.should eq(400)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(400)
        doc.parsed_response['code'].should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
      end

      it "returns an error if document identifier is corrupted" do
        cmd = "/_api/document/123456"
        doc = ArangoDB.log_delete("#{prefix}-bad-identifier", cmd)

        doc.code.should eq(400)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(400) # bad parameter
        doc.parsed_response['code'].should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
      end

      it "returns an error if document identifier is corrupted" do
        cmd = "/_api/document//123456"
        doc = ArangoDB.log_delete("#{prefix}-bad-identifier2", cmd)

        doc.code.should eq(400)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(400)
        doc.parsed_response['code'].should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
      end

      it "returns an error if collection identifier is unknown" do
        cmd = "/_api/document/123456/234567"
        doc = ArangoDB.log_delete("#{prefix}-unknown-cid", cmd)

        doc.code.should eq(404)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(1203)
        doc.parsed_response['code'].should eq(404)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
      end

      it "returns an error if document identifier is unknown" do
        cmd = "/_api/document/#{@cid}/234567"
        doc = ArangoDB.log_delete("#{prefix}-unknown-identifier", cmd)

        doc.code.should eq(404)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(1202)
        doc.parsed_response['code'].should eq(404)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
      end
    end

################################################################################
## deleting documents
################################################################################

    context "deleting documents:" do
      before do
        @cn = "UnitTestsCollectionBasics"
        @cid = ArangoDB.create_collection(@cn)
      end

      after do
        ArangoDB.drop_collection(@cn)
      end

      it "create a document and delete it" do
        cmd = "/_api/document?collection=#{@cid}"
        body = "{ \"Hallo\" : \"World\" }"
        doc = ArangoDB.post(cmd, :body => body)

        doc.code.should eq(201)

        location = doc.headers['location']
        location.should be_kind_of(String)

        did = doc.parsed_response['_id']
        rev = doc.parsed_response['_rev']

        # delete document
        cmd = "/_api/document/#{did}"
        doc = ArangoDB.log_delete("#{prefix}", cmd)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        did2 = doc.parsed_response['_id']
        did2.should be_kind_of(String)
        did2.should eq(did)
        
        rev2 = doc.parsed_response['_rev']
        rev2.should be_kind_of(String)
        rev2.should eq(rev)

        ArangoDB.size_collection(@cid).should eq(0)
      end

      it "create a document and delete it, using if-match" do
        cmd = "/_api/document?collection=#{@cid}"
        body = "{ \"Hallo\" : \"World\" }"
        doc = ArangoDB.post(cmd, :body => body)

        doc.code.should eq(201)

        location = doc.headers['location']
        location.should be_kind_of(String)

        did = doc.parsed_response['_id']
        rev = doc.parsed_response['_rev']

        # delete document, different revision
        cmd = "/_api/document/#{did}"
        hdr = { "if-match" => "\"386897\"" }
        doc = ArangoDB.log_delete("#{prefix}-if-match-other", cmd, :headers => hdr)

        doc.code.should eq(412)
        doc.parsed_response['error'].should eq(true)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        did2 = doc.parsed_response['_id']
        did2.should be_kind_of(String)
        did2.should eq(did)
        
        rev2 = doc.parsed_response['_rev']
        rev2.should be_kind_of(String)
        rev2.should eq(rev)

        # delete document, same revision
        cmd = "/_api/document/#{did}"
        hdr = { "if-match" => "\"#{rev}\"" }
        doc = ArangoDB.log_delete("#{prefix}-if-match", cmd, :headers => hdr)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        did2 = doc.parsed_response['_id']
        did2.should be_kind_of(String)
        did2.should eq(did)
        
        rev2 = doc.parsed_response['_rev']
        rev2.should be_kind_of(String)
        rev2.should eq(rev)

        ArangoDB.size_collection(@cid).should eq(0)
      end

      it "create a document and delete it, using an invalid revision" do
        cmd = "/_api/document?collection=#{@cid}"
        body = "{ \"Hallo\" : \"World\" }"
        doc = ArangoDB.post(cmd, :body => body)

        doc.code.should eq(201)

        location = doc.headers['location']
        location.should be_kind_of(String)

        did = doc.parsed_response['_id']
        rev = doc.parsed_response['_rev']

        # delete document, invalid revision
        cmd = "/_api/document/#{did}"
        hdr = { "if-match" => "\"*abcd\"" }
        doc = ArangoDB.log_delete("#{prefix}-rev-invalid", cmd, :headers => hdr )

        doc.code.should eq(412)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(1200)
        doc.parsed_response['code'].should eq(412)
        
        # delete document, invalid revision
        cmd = "/_api/document/#{did}"
        hdr = { "if-match" => "'*abcd'" }
        doc = ArangoDB.log_delete("#{prefix}-rev-invalid", cmd, :headers => hdr)

        doc.code.should eq(412)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(1200)
        doc.parsed_response['code'].should eq(412)
        
        # delete document, correct revision
        cmd = "/_api/document/#{did}"
        hdr = { "if-match" => "'#{rev}'" }
        doc = ArangoDB.log_delete("#{prefix}-rev-invalid", cmd, :headers => hdr)

        doc.code.should eq(200)

        ArangoDB.size_collection(@cid).should eq(0)
      end
      
      it "create a document and delete it, waitForSync URL param = false" do
        cmd = "/_api/document?collection=#{@cid}&waitForSync=false"
        body = "{ \"Hallo\" : \"World\" }"
        doc = ArangoDB.post(cmd, :body => body)

        doc.code.should eq(201)

        location = doc.headers['location']
        location.should be_kind_of(String)

        did = doc.parsed_response['_id']
        rev = doc.parsed_response['_rev']

        # delete document
        cmd = "/_api/document/#{did}"
        doc = ArangoDB.log_delete("#{prefix}-sync-false", cmd)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        did2 = doc.parsed_response['_id']
        did2.should be_kind_of(String)
        did2.should eq(did)
        
        rev2 = doc.parsed_response['_rev']
        rev2.should be_kind_of(String)
        rev2.should eq(rev)

        ArangoDB.size_collection(@cid).should eq(0)
      end
      
      it "create a document and delete it, waitForSync URL param = true" do
        cmd = "/_api/document?collection=#{@cid}&waitForSync=true"
        body = "{ \"Hallo\" : \"World\" }"
        doc = ArangoDB.post(cmd, :body => body)

        doc.code.should eq(201)

        location = doc.headers['location']
        location.should be_kind_of(String)

        did = doc.parsed_response['_id']
        rev = doc.parsed_response['_rev']

        # delete document
        cmd = "/_api/document/#{did}"
        doc = ArangoDB.log_delete("#{prefix}-sync-true", cmd)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        did2 = doc.parsed_response['_id']
        did2.should be_kind_of(String)
        did2.should eq(did)
        
        rev2 = doc.parsed_response['_rev']
        rev2.should be_kind_of(String)
        rev2.should eq(rev)

        ArangoDB.size_collection(@cid).should eq(0)
      end
    end

  end
end

