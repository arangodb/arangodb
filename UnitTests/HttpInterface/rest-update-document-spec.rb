# coding: utf-8

require 'rspec'
require './arangodb.rb'

describe ArangoDB do
  prefix = "rest-update-document"

  context "update a document:" do

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

      it "returns an error if document handle is missing" do
        cmd = "/_api/document"
        body = "{}"
        doc = ArangoDB.log_put("#{prefix}-missing-handle", cmd, :body => body)

        doc.code.should eq(400)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(400)
        doc.parsed_response['code'].should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        ArangoDB.size_collection(@cid).should eq(0)
      end

      it "returns an error if document handle is corrupted" do
        cmd = "/_api/document/123456"
        body = "{}"
        doc = ArangoDB.log_put("#{prefix}-bad-handle", cmd, :body => body)

        doc.code.should eq(400)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(400)
        doc.parsed_response['code'].should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        ArangoDB.size_collection(@cid).should eq(0)
      end

      it "returns an error if document handle is corrupted with empty cid" do
        cmd = "/_api/document//123456"
        body = "{}"
        doc = ArangoDB.log_put("#{prefix}-bad-handle2", cmd, :body => body)

        doc.code.should eq(400)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(1203)
        doc.parsed_response['code'].should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        ArangoDB.size_collection(@cid).should eq(0)
      end

      it "returns an error if collection identifier is unknown" do
        cmd = "/_api/document/123456/234567"
        body = "{}"
        doc = ArangoDB.log_put("#{prefix}-unknown-cid", cmd, :body => body)

        doc.code.should eq(404)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(1203)
        doc.parsed_response['code'].should eq(404)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        ArangoDB.size_collection(@cid).should eq(0)
      end

      it "returns an error if document handle is unknown" do
        cmd = "/_api/document/#{@cid}/234567"
        body = "{}"
        doc = ArangoDB.log_put("#{prefix}-unknown-handle", cmd, :body => body)

        doc.code.should eq(404)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(1202)
        doc.parsed_response['code'].should eq(404)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        ArangoDB.size_collection(@cid).should eq(0)
      end

      it "returns an error if the policy parameter is bad" do
        cmd = "/_api/document?collection=#{@cid}"
        body = "{ \"Hallo\" : \"World\" }"
        doc = ArangoDB.post(cmd, :body => body)

        doc.code.should eq(201)

        location = doc.headers['location']
        location.should be_kind_of(String)

        did = doc.parsed_response['_id']
        rev = doc.parsed_response['_rev']

        # update document, different revision
        cmd = "/_api/document/#{did}?policy=last-write"
        hdr = { "if-match" => "\"388576#{rev}\"" }
        doc = ArangoDB.log_put("#{prefix}-policy-bad", cmd, :headers => hdr)

        doc.code.should eq(400)
        doc.parsed_response['error'].should eq(true)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        ArangoDB.delete(location)

        ArangoDB.size_collection(@cid).should eq(0)
      end
    end

################################################################################
## updating documents
################################################################################

    context "updating document:" do
      before do
        @cn = "UnitTestsCollectionBasics"
        @cid = ArangoDB.create_collection(@cn)
      end

      after do
        ArangoDB.drop_collection(@cn)
      end

      it "create a document and update it" do
        cmd = "/_api/document?collection=#{@cid}"
        body = "{ \"Hallo\" : \"World\" }"
        doc = ArangoDB.post(cmd, :body => body)

        doc.code.should eq(201)

        location = doc.headers['location']
        location.should be_kind_of(String)

        did = doc.parsed_response['_id']
        rev = doc.parsed_response['_rev']

        # update document
        cmd = "/_api/document/#{did}"
        body = "{ \"World\" : \"Hallo\" }"
        doc = ArangoDB.log_put("#{prefix}", cmd, :body => body)

        doc.code.should eq(200)
        doc.parsed_response['error'].should eq(false)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        did2 = doc.parsed_response['_id']
        did2.should be_kind_of(String)
        did2.should eq(did)
        
        rev2 = doc.parsed_response['_rev']
        rev2.should be_kind_of(String)
        rev2.should_not eq(rev)

        ArangoDB.delete(location)

        ArangoDB.size_collection(@cid).should eq(0)
      end

      it "create a document and update it, using if-match" do
        cmd = "/_api/document?collection=#{@cid}"
        body = "{ \"Hallo\" : \"World\" }"
        doc = ArangoDB.post(cmd, :body => body)

        doc.code.should eq(201)

        location = doc.headers['location']
        location.should be_kind_of(String)

        did = doc.parsed_response['_id']
        rev = doc.parsed_response['_rev']

        # update document, different revision
        cmd = "/_api/document/#{did}"
        hdr = { "if-match" => "\"658993#{rev}\"" }
        body = "{ \"World\" : \"Hallo\" }"
        doc = ArangoDB.log_put("#{prefix}-if-match-other", cmd, :headers => hdr, :body => body)

        doc.code.should eq(412)
        doc.parsed_response['error'].should eq(true)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        did2 = doc.parsed_response['_id']
        did2.should be_kind_of(String)
        did2.should eq(did)
        
        rev2 = doc.parsed_response['_rev']
        rev2.should be_kind_of(String)
        rev2.should eq(rev)

        # update document, same revision
        cmd = "/_api/document/#{did}"
        hdr = { "if-match" => "\"#{rev}\"" }
        body = "{ \"World\" : \"Hallo\" }"
        doc = ArangoDB.log_put("#{prefix}-if-match", cmd, :headers => hdr, :body => body)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)

        did2 = doc.parsed_response['_id']
        did2.should be_kind_of(String)
        did2.should eq(did)

        rev2 = doc.parsed_response['_rev']
        rev2.should be_kind_of(String)
        rev2.should_not eq(rev)

        ArangoDB.delete(location)

        ArangoDB.size_collection(@cid).should eq(0)
      end

      it "create a document and update it, using if-match and last-write wins" do
        cmd = "/_api/document?collection=#{@cid}"
        body = "{ \"Hallo\" : \"World\" }"
        doc = ArangoDB.post(cmd, :body => body)

        doc.code.should eq(201)

        location = doc.headers['location']
        location.should be_kind_of(String)

        did = doc.parsed_response['_id']
        rev = doc.parsed_response['_rev']

        # update document, different revision
        cmd = "/_api/document/#{did}?policy=last"
        hdr = { "if-match" => "\"390876#{rev}\"" }
        body = "{ \"World\" : \"Hallo\" }"
        doc = ArangoDB.log_put("#{prefix}-if-match-other-last-write", cmd, :headers => hdr, :body => body)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)

        did2 = doc.parsed_response['_id']
        did2.should be_kind_of(String)
        did2.should eq(did)
        
        rev2 = doc.parsed_response['_rev']
        rev2.should be_kind_of(String)
        rev2.should_not eq(rev)

        ArangoDB.delete(location)

        ArangoDB.size_collection(@cid).should eq(0)
      end

      it "create a document and update it, using rev" do
        cmd = "/_api/document?collection=#{@cid}"
        body = "{ \"Hallo\" : \"World\" }"
        doc = ArangoDB.post(cmd, :body => body)

        doc.code.should eq(201)

        location = doc.headers['location']
        location.should be_kind_of(String)

        did = doc.parsed_response['_id']
        rev = doc.parsed_response['_rev']

        # update document, different revision
        cmd = "/_api/document/#{did}?rev=858976#{rev}"
        body = "{ \"World\" : \"Hallo\" }"
              doc = ArangoDB.log_put("#{prefix}-rev-other", cmd, :body => body)

        doc.code.should eq(412)
        doc.parsed_response['error'].should eq(true)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        did2 = doc.parsed_response['_id']
        did2.should be_kind_of(String)
        did2.should eq(did)
        
        rev2 = doc.parsed_response['_rev']
        rev2.should be_kind_of(String)
        rev2.should eq(rev)

        # update document, same revision
        cmd = "/_api/document/#{did}?rev=#{rev}"
        body = "{ \"World\" : \"Hallo\" }"
        doc = ArangoDB.log_put("#{prefix}-rev", cmd, :body => body)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)

        did2 = doc.parsed_response['_id']
        did2.should be_kind_of(String)
        did2.should eq(did)

        rev2 = doc.parsed_response['_rev']
        rev2.should be_kind_of(String)
        rev2.should_not eq(rev)

        ArangoDB.delete(location)

        ArangoDB.size_collection(@cid).should eq(0)
      end

      it "create a document and update it, using rev and last-write wins" do
        cmd = "/_api/document?collection=#{@cid}"
        body = "{ \"Hallo\" : \"World\" }"
        doc = ArangoDB.post(cmd, :body => body)

        doc.code.should eq(201)

        location = doc.headers['location']
        location.should be_kind_of(String)

        did = doc.parsed_response['_id']
        rev = doc.parsed_response['_rev']

        # update document, different revision
        cmd = "/_api/document/#{did}?policy=last&rev=38964836#{rev}"
        body = "{ \"World\" : \"Hallo\" }"
        doc = ArangoDB.log_put("#{prefix}-rev-other-last-write", cmd, :body => body)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)

        did2 = doc.parsed_response['_id']
        did2.should be_kind_of(String)
        did2.should eq(did)
        
        rev2 = doc.parsed_response['_rev']
        rev2.should be_kind_of(String)
        rev2.should_not eq(rev)

        ArangoDB.delete(location)

        ArangoDB.size_collection(@cid).should eq(0)
      end
      
      it "create a document and update it, waitForSync URL param=false" do
        cmd = "/_api/document?collection=#{@cid}&waitForSync=false"
        body = "{ \"Hallo\" : \"World\" }"
        doc = ArangoDB.post(cmd, :body => body)

        doc.code.should eq(201)

        location = doc.headers['location']
        location.should be_kind_of(String)

        did = doc.parsed_response['_id']
        rev = doc.parsed_response['_rev']

        # update document
        cmd = "/_api/document/#{did}"
        body = "{ \"World\" : \"Hallo\" }"
        doc = ArangoDB.log_put("#{prefix}-sync-false", cmd, :body => body)

        doc.code.should eq(200)
        doc.parsed_response['error'].should eq(false)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        did2 = doc.parsed_response['_id']
        did2.should be_kind_of(String)
        did2.should eq(did)
        
        rev2 = doc.parsed_response['_rev']
        rev2.should be_kind_of(String)
        rev2.should_not eq(rev)

        ArangoDB.delete(location)

        ArangoDB.size_collection(@cid).should eq(0)
      end
      
      it "create a document and update it, waitForSync URL param=true" do
        cmd = "/_api/document?collection=#{@cid}&waitForSync=true"
        body = "{ \"Hallo\" : \"World\" }"
        doc = ArangoDB.post(cmd, :body => body)

        doc.code.should eq(201)

        location = doc.headers['location']
        location.should be_kind_of(String)

        did = doc.parsed_response['_id']
        rev = doc.parsed_response['_rev']

        # update document
        cmd = "/_api/document/#{did}"
        body = "{ \"World\" : \"Hallo\" }"
        doc = ArangoDB.log_put("#{prefix}-sync-true", cmd, :body => body)

        doc.code.should eq(200)
        doc.parsed_response['error'].should eq(false)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        did2 = doc.parsed_response['_id']
        did2.should be_kind_of(String)
        did2.should eq(did)
        
        rev2 = doc.parsed_response['_rev']
        rev2.should be_kind_of(String)
        rev2.should_not eq(rev)

        ArangoDB.delete(location)

        ArangoDB.size_collection(@cid).should eq(0)
      end

      it "update a document, using patch" do
        cmd = "/_api/document?collection=#{@cid}"
        body = "{ \"Hallo\" : \"World\" }"
        doc = ArangoDB.post(cmd, :body => body)

        doc.code.should eq(201)

        location = doc.headers['location']
        location.should be_kind_of(String)

        did = doc.parsed_response['_id']
        rev = doc.parsed_response['_rev']

        # update document
        cmd = "/_api/document/#{did}"
        body = "{ \"fox\" : \"Foxy\" }"
        doc = ArangoDB.log_patch("#{prefix}-patch", cmd, :body => body)

        doc.code.should eq(200)
        doc.parsed_response['error'].should eq(false)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        did2 = doc.parsed_response['_id']
        did2.should be_kind_of(String)
        did2.should eq(did)
        
        rev2 = doc.parsed_response['_rev']
        rev2.should be_kind_of(String)
        rev2.should_not eq(rev)

        ArangoDB.size_collection(@cid).should eq(1)

        doc = ArangoDB.get("/_api/document/#{did}")

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['Hallo'].should eq('World')
        doc.parsed_response['fox'].should eq('Foxy')
      end
      
      it "update a document, using patch, keepNull = true" do
        cmd = "/_api/document?collection=#{@cid}"
        body = "{ \"Hallo\" : \"World\" }"
        doc = ArangoDB.post(cmd, :body => body)

        doc.code.should eq(201)

        location = doc.headers['location']
        location.should be_kind_of(String)

        did = doc.parsed_response['_id']
        rev = doc.parsed_response['_rev']

        # update document
        cmd = "/_api/document/#{did}?keepNull=true"
        body = "{ \"fox\" : \"Foxy\", \"Hallo\" : null }"
        doc = ArangoDB.log_patch("#{prefix}-patch", cmd, :body => body)

        doc.code.should eq(200)
        doc.parsed_response['error'].should eq(false)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        did2 = doc.parsed_response['_id']
        did2.should be_kind_of(String)
        did2.should eq(did)
        
        rev2 = doc.parsed_response['_rev']
        rev2.should be_kind_of(String)
        rev2.should_not eq(rev)

        ArangoDB.size_collection(@cid).should eq(1)

        doc = ArangoDB.get("/_api/document/#{did}")

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response.should have_key('Hallo') # nil, but the attribute is there
        doc.parsed_response['fox'].should eq('Foxy')
      end

      it "update a document, using patch, keepNull = false" do
        cmd = "/_api/document?collection=#{@cid}"
        body = "{ \"Hallo\" : \"World\" }"
        doc = ArangoDB.post(cmd, :body => body)

        doc.code.should eq(201)

        location = doc.headers['location']
        location.should be_kind_of(String)

        did = doc.parsed_response['_id']
        rev = doc.parsed_response['_rev']

        # update document
        cmd = "/_api/document/#{did}?keepNull=false"
        body = "{ \"fox\" : \"Foxy\", \"Hallo\" : null }"
        doc = ArangoDB.log_patch("#{prefix}-patch", cmd, :body => body)

        doc.code.should eq(200)
        doc.parsed_response['error'].should eq(false)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        did2 = doc.parsed_response['_id']
        did2.should be_kind_of(String)
        did2.should eq(did)
        
        rev2 = doc.parsed_response['_rev']
        rev2.should be_kind_of(String)
        rev2.should_not eq(rev)

        ArangoDB.size_collection(@cid).should eq(1)

        doc = ArangoDB.get("/_api/document/#{did}")

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response.should_not have_key('Hallo')
        doc.parsed_response['fox'].should eq('Foxy')
      end

    end

  end
end
