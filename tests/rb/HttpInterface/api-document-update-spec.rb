# coding: utf-8

require 'rspec'
require 'arangodb.rb'

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

      it "returns an error if document identifier is missing" do
        cmd = "/_api/document"
        body = "{}"
        doc = ArangoDB.log_put("#{prefix}-missing-identifier", cmd, :body => body)

        doc.code.should eq(400)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(400)
        doc.parsed_response['code'].should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        ArangoDB.size_collection(@cid).should eq(0)
      end

      it "returns an error if document identifier is corrupted" do
        cmd = "/_api/document/123456"
        body = "{}"
        doc = ArangoDB.log_put("#{prefix}-bad-identifier", cmd, :body => body)

        doc.code.should eq(400)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(1227)
        doc.parsed_response['code'].should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        ArangoDB.size_collection(@cid).should eq(0)
      end

      it "returns an error if document identifier is corrupted with empty cid" do
        cmd = "/_api/document//123456"
        body = "{}"
        doc = ArangoDB.log_put("#{prefix}-bad-identifier2", cmd, :body => body)

        doc.code.should eq(400)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(1227)
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

      it "returns an error if document identifier is unknown" do
        cmd = "/_api/document/#{@cid}/234567"
        body = "{}"
        doc = ArangoDB.log_put("#{prefix}-unknown-identifier", cmd, :body => body)

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
      
      it "create a document and update it with invalid JSON" do
        cmd = "/_api/document?collection=#{@cid}"
        body = "{ \"Hallo\" : \"World\" }"
        doc = ArangoDB.post(cmd, :body => body)

        doc.code.should eq(201)
        did = doc.parsed_response['_id']

        # update document
        cmd = "/_api/document/#{did}"
        body = "{ \"World\" : \"Hallo\xff\" }"
        doc = ArangoDB.log_put("#{prefix}", cmd, :body => body)

        doc.code.should eq(400)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(600)
        doc.parsed_response['code'].should eq(400)
      end

      it "create a document and replace it, using ignoreRevs=false" do
        cmd = "/_api/document?collection=#{@cid}"
        body = "{ \"Hallo\" : \"World\" }"
        doc = ArangoDB.post(cmd, :body => body)

        doc.code.should eq(201)

        location = doc.headers['location']
        location.should be_kind_of(String)

        did = doc.parsed_response['_id']
        rev = doc.parsed_response['_rev']

        # update document, different revision
        cmd = "/_api/document/#{did}?ignoreRevs=false"
        body = "{ \"_rev\": \"658993\", \"World\" : \"Hallo\" }"
        doc = ArangoDB.log_put("#{prefix}-ignore-revs-false", cmd, :body => body)

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
        cmd = "/_api/document/#{did}?ignoreRevs=false"
        body = "{ \"_rev\": \"#{rev}\", \"World\" : \"Hallo\" }"
        doc = ArangoDB.log_put("#{prefix}-ignore-revs-false", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        did2 = doc.parsed_response['_id']
        did2.should be_kind_of(String)
        did2.should eq(did)

        rev2 = doc.parsed_response['_rev']
        rev2.should be_kind_of(String)
        rev2.should_not eq(rev)

        cmd = "/_api/collection/#{@cid}/properties"
        body = "{ \"waitForSync\" : false }"
        doc = ArangoDB.put(cmd, :body => body)

        # wait for dbservers to pick up the change
        sleep 2

        # update document 
        cmd = "/_api/document/#{did}?ignoreRevs=false"
        body = "{ \"_rev\": \"#{rev2}\", \"World\" : \"Hallo2\" }"
        doc3 = ArangoDB.log_put("#{prefix}-ignore-revs-false", cmd, :body => body)

        doc3.code.should eq(202)
        doc3.headers['content-type'].should eq("application/json; charset=utf-8")
        rev3 = doc3.parsed_response['_rev']

        # update document 
        cmd = "/_api/document/#{did}?ignoreRevs=false&waitForSync=true"
        body = "{ \"_rev\": \"#{rev3}\", \"World\" : \"Hallo3\" }"
        doc4 = ArangoDB.log_put("#{prefix}-ignore-revs-false", cmd, :body => body)

        doc4.code.should eq(201)
        doc4.headers['content-type'].should eq("application/json; charset=utf-8")
        rev4 = doc4.parsed_response['_rev']

        ArangoDB.delete(location)

        ArangoDB.size_collection(@cid).should eq(0)
      end

      it "create a document and replace it, using ignoreRevs=false (with key)" do
        cmd = "/_api/document?collection=#{@cid}"
        body = "{ \"_key\" : \"hello\", \"Hallo\" : \"World\" }"
        doc = ArangoDB.post(cmd, :body => body)

        doc.code.should eq(201)

        location = doc.headers['location']
        location.should be_kind_of(String)

        did = doc.parsed_response['_id']
        rev = doc.parsed_response['_rev']

        # update document, different revision
        cmd = "/_api/document/#{did}?ignoreRevs=false"
        body = "{ \"_key\" : \"hello\", \"_rev\" : \"658993\", \"World\" : \"Hallo\" }"
        doc = ArangoDB.log_put("#{prefix}-ignore-revs-false", cmd, :body => body)

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
        cmd = "/_api/document/#{did}?ignoreRevs=false"
        body = "{ \"_key\" : \"hello\", \"_rev\": \"#{rev}\", \"World\" : \"Hallo\" }"
        doc = ArangoDB.log_put("#{prefix}-ignore-revs-false", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        did2 = doc.parsed_response['_id']
        did2.should be_kind_of(String)
        did2.should eq(did)

        rev2 = doc.parsed_response['_rev']
        rev2.should be_kind_of(String)
        rev2.should_not eq(rev)

        cmd = "/_api/collection/#{@cid}/properties"
        body = "{ \"waitForSync\" : false }"
        doc = ArangoDB.put(cmd, :body => body)

        # wait for dbservers to pick up the change
        sleep 2

        # update document 
        cmd = "/_api/document/#{did}?ignoreRevs=false"
        body = "{ \"_key\" : \"hello\", \"_rev\": \"#{rev2}\", \"World\" : \"Hallo2\" }"
        doc3 = ArangoDB.log_put("#{prefix}-ignore-revs-false", cmd, :body => body)

        doc3.code.should eq(202)
        doc3.headers['content-type'].should eq("application/json; charset=utf-8")
        rev3 = doc3.parsed_response['_rev']

        # update document 
        cmd = "/_api/document/#{did}?ignoreRevs=false&waitForSync=true"
        body = "{ \"_key\" : \"hello\", \"_rev\": \"#{rev3}\", \"World\" : \"Hallo3\" }"
        doc4 = ArangoDB.log_put("#{prefix}-ignore-revs-false", cmd, :body => body)

        doc4.code.should eq(201)
        doc4.headers['content-type'].should eq("application/json; charset=utf-8")
        rev4 = doc4.parsed_response['_rev']

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

        doc.code.should eq(201)
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

        # replace document, different revision
        cmd = "/_api/document/#{did}"
        hdr = { "if-match" => "\"658993\"" }
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

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        did2 = doc.parsed_response['_id']
        did2.should be_kind_of(String)
        did2.should eq(did)

        rev2 = doc.parsed_response['_rev']
        rev2.should be_kind_of(String)
        rev2.should_not eq(rev)

        cmd = "/_api/collection/#{@cid}/properties"
        body = "{ \"waitForSync\" : false }"
        doc = ArangoDB.put(cmd, :body => body)

        # wait for dbservers to pick up the change
        sleep 2

        # update document 
        cmd = "/_api/document/#{did}"
        hdr = { "if-match" => "\"#{rev2}\"" }
        body = "{ \"World\" : \"Hallo2\" }"
        doc3 = ArangoDB.log_put("#{prefix}-if-match", cmd, :headers => hdr, :body => body)

        doc3.code.should eq(202)
        doc3.headers['content-type'].should eq("application/json; charset=utf-8")
        rev3 = doc3.parsed_response['_rev']

        # update document 
        cmd = "/_api/document/#{did}?waitForSync=true"
        hdr = { "if-match" => "\"#{rev3}\"" }
        body = "{ \"World\" : \"Hallo3\" }"
        doc4 = ArangoDB.log_put("#{prefix}-if-match", cmd, :headers => hdr, :body => body)

        doc4.code.should eq(201)
        doc4.headers['content-type'].should eq("application/json; charset=utf-8")
        rev4 = doc4.parsed_response['_rev']

        ArangoDB.delete(location)

        ArangoDB.size_collection(@cid).should eq(0)
      end

      it "create a document and update it, using an invalid revision" do
        cmd = "/_api/document?collection=#{@cid}"
        body = "{ \"Hallo\" : \"World\" }"
        doc = ArangoDB.post(cmd, :body => body)

        doc.code.should eq(201)

        location = doc.headers['location']
        location.should be_kind_of(String)

        did = doc.parsed_response['_id']
        rev = doc.parsed_response['_rev']

        # update document, invalid revision
        cmd = "/_api/document/#{did}"
        hdr = { "if-match" => "\"*abcd\"" }
        doc = ArangoDB.log_put("#{prefix}-rev-invalid", cmd, :headers => hdr, :body => body)

        doc.code.should eq(412)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(1200)
        doc.parsed_response['code'].should eq(412)
        
        # update document, invalid revision
        cmd = "/_api/document/#{did}"
        hdr = { "if-match" => "'*abcd'" }
        doc = ArangoDB.log_put("#{prefix}-rev-invalid", cmd, :headers => hdr, :body => body)

        doc.code.should eq(412)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(1200)
        doc.parsed_response['code'].should eq(412)
        
        # update document, correct revision
        cmd = "/_api/document/#{did}"
        hdr = { "if-match" => "'#{rev}'" }
        doc = ArangoDB.log_put("#{prefix}-rev-invalid", cmd, :headers => hdr, :body => body)

        doc.code.should eq(201)
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

        doc.code.should eq(201)
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

        doc.code.should eq(201)
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

        doc.code.should eq(201)
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

        doc.code.should eq(201)
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

        doc.code.should eq(201)
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

      it "update a document, using duplicate attributes" do
        cmd = "/_api/document?collection=#{@cid}"
        body = "{ \"Hallo\" : \"World\" }"
        doc = ArangoDB.post(cmd, :body => body)

        doc.code.should eq(201)

        did = doc.parsed_response['_id']

        # update document
        cmd = "/_api/document/#{did}"
        body = "{ \"a\": 1, \"a\": 2 }"
        doc = ArangoDB.log_patch("#{prefix}-patch", cmd, :body => body)

        doc.code.should eq(400)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(600)
        doc.parsed_response['code'].should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
      end
      
      it "update a document, using duplicate nested attributes" do
        cmd = "/_api/document?collection=#{@cid}"
        body = "{ \"Hallo\" : \"World\" }"
        doc = ArangoDB.post(cmd, :body => body)

        doc.code.should eq(201)

        did = doc.parsed_response['_id']

        # update document
        cmd = "/_api/document/#{did}"
        body = "{ \"outer\" : { \"a\": 1, \"a\": 2 } }"
        doc = ArangoDB.log_patch("#{prefix}-patch", cmd, :body => body)

        doc.code.should eq(400)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(600)
        doc.parsed_response['code'].should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
      end
      
      it "replace a document, using duplicate attributes" do
        cmd = "/_api/document?collection=#{@cid}"
        body = "{ \"Hallo\" : \"World\" }"
        doc = ArangoDB.post(cmd, :body => body)

        doc.code.should eq(201)

        did = doc.parsed_response['_id']

        # update document
        cmd = "/_api/document/#{did}"
        body = "{ \"a\": 1, \"a\": 2 }"
        doc = ArangoDB.log_put("#{prefix}-patch", cmd, :body => body)

        doc.code.should eq(400)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(600)
        doc.parsed_response['code'].should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
      end
      
      it "replace a document, using duplicate nested attributes" do
        cmd = "/_api/document?collection=#{@cid}"
        body = "{ \"Hallo\" : \"World\" }"
        doc = ArangoDB.post(cmd, :body => body)

        doc.code.should eq(201)

        did = doc.parsed_response['_id']

        # update document
        cmd = "/_api/document/#{did}"
        body = "{ \"outer\" : { \"a\": 1, \"a\": 2 } }"
        doc = ArangoDB.log_put("#{prefix}-patch", cmd, :body => body)

        doc.code.should eq(400)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(600)
        doc.parsed_response['code'].should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
      end

      it "create a document and update it, using ignoreRevs=false" do
        cmd = "/_api/document?collection=#{@cid}"
        body = "{ \"Hallo\" : \"World\" }"
        doc = ArangoDB.post(cmd, :body => body)

        doc.code.should eq(201)

        location = doc.headers['location']
        location.should be_kind_of(String)

        did = doc.parsed_response['_id']
        rev = doc.parsed_response['_rev']

        # update document, different revision
        cmd = "/_api/document/#{did}?ignoreRevs=false"
        body = "{ \"_rev\": \"658993\", \"World\" : \"Hallo\" }"
        doc = ArangoDB.log_patch("#{prefix}-ignore-revs-false", cmd, :body => body)

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
        cmd = "/_api/document/#{did}?ignoreRevs=false"
        body = "{ \"_rev\": \"#{rev}\", \"World\" : \"Hallo\" }"
        doc = ArangoDB.log_patch("#{prefix}-ignore-revs-false", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        did2 = doc.parsed_response['_id']
        did2.should be_kind_of(String)
        did2.should eq(did)

        rev2 = doc.parsed_response['_rev']
        rev2.should be_kind_of(String)
        rev2.should_not eq(rev)

        cmd = "/_api/collection/#{@cid}/properties"
        body = "{ \"waitForSync\" : false }"
        doc = ArangoDB.put(cmd, :body => body)

        # wait for dbservers to pick up the change
        sleep 2

        # update document 
        cmd = "/_api/document/#{did}?ignoreRevs=false"
        body = "{ \"_rev\": \"#{rev2}\", \"World\" : \"Hallo2\" }"
        doc3 = ArangoDB.log_put("#{prefix}-ignore-revs-false", cmd, :body => body)

        doc3.code.should eq(202)
        doc3.headers['content-type'].should eq("application/json; charset=utf-8")
        rev3 = doc3.parsed_response['_rev']

        # update document 
        cmd = "/_api/document/#{did}?ignoreRevs=false&waitForSync=true"
        body = "{ \"_rev\": \"#{rev3}\", \"World\" : \"Hallo3\" }"
        doc4 = ArangoDB.log_patch("#{prefix}-ignore-revs-false", cmd, :body => body)

        doc4.code.should eq(201)
        doc4.headers['content-type'].should eq("application/json; charset=utf-8")
        rev4 = doc4.parsed_response['_rev']

        ArangoDB.delete(location)

        ArangoDB.size_collection(@cid).should eq(0)
      end

      it "create a document and update it, using ignoreRevs=false (with key)" do
        cmd = "/_api/document?collection=#{@cid}"
        body = "{ \"_key\" : \"hello\", \"Hallo\" : \"World\" }"
        doc = ArangoDB.post(cmd, :body => body)

        doc.code.should eq(201)

        location = doc.headers['location']
        location.should be_kind_of(String)

        did = doc.parsed_response['_id']
        rev = doc.parsed_response['_rev']

        # update document, different revision
        cmd = "/_api/document/#{did}?ignoreRevs=false"
        body = "{ \"_key\" : \"hello\", \"_rev\" : \"658993\", \"World\" : \"Hallo\" }"
        doc = ArangoDB.log_patch("#{prefix}-ignore-revs-false", cmd, :body => body)

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
        cmd = "/_api/document/#{did}?ignoreRevs=false"
        body = "{ \"_key\" : \"hello\", \"_rev\" : \"#{rev}\", \"World\" : \"Hallo\" }"
        doc = ArangoDB.log_patch("#{prefix}-ignore-revs-false", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        did2 = doc.parsed_response['_id']
        did2.should be_kind_of(String)
        did2.should eq(did)

        rev2 = doc.parsed_response['_rev']
        rev2.should be_kind_of(String)
        rev2.should_not eq(rev)

        cmd = "/_api/collection/#{@cid}/properties"
        body = "{ \"waitForSync\" : false }"
        doc = ArangoDB.put(cmd, :body => body)

        # wait for dbservers to pick up the change
        sleep 2

        # update document 
        cmd = "/_api/document/#{did}?ignoreRevs=false"
        body = "{ \"_key\" : \"hello\", \"_rev\": \"#{rev2}\", \"World\" : \"Hallo2\" }"
        doc3 = ArangoDB.log_put("#{prefix}-ignore-revs-false", cmd, :body => body)

        doc3.code.should eq(202)
        doc3.headers['content-type'].should eq("application/json; charset=utf-8")
        rev3 = doc3.parsed_response['_rev']

        # update document 
        cmd = "/_api/document/#{did}?ignoreRevs=false&waitForSync=true"
        body = "{ \"_key\" : \"hello\", \"_rev\": \"#{rev3}\", \"World\" : \"Hallo3\" }"
        doc4 = ArangoDB.log_patch("#{prefix}-ignore-revs-false", cmd, :body => body)

        doc4.code.should eq(201)
        doc4.headers['content-type'].should eq("application/json; charset=utf-8")
        rev4 = doc4.parsed_response['_rev']

        ArangoDB.delete(location)

        ArangoDB.size_collection(@cid).should eq(0)
      end
    end
  end
end
