# coding: utf-8

require 'rspec'
require './arangodb.rb'

describe ArangoDB do
  prefix = "rest_update-document"

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
	cmd = "/document"
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
	cmd = "/document/123456"
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
	cmd = "/document//123456"
	body = "{}"
        doc = ArangoDB.log_put("#{prefix}-bad-handle2", cmd, :body => body)

	doc.code.should eq(400)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(600)
	doc.parsed_response['code'].should eq(400)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	ArangoDB.size_collection(@cid).should eq(0)
      end

      it "returns an error if collection identifier is unknown" do
	cmd = "/document/123456/234567"
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
	cmd = "/document/#{@cid}/234567"
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
	cmd = "/document?collection=#{@cid}"
	body = "{ \"Hallo\" : \"World\" }"
	doc = ArangoDB.post(cmd, :body => body)

	doc.code.should eq(201)

	location = doc.headers['location']
	location.should be_kind_of(String)

	did = doc.parsed_response['_id']
	rev = doc.parsed_response['_rev']

	# update document, different revision
	cmd = "/document/#{did}?policy=last-write"
	hdr = { "if-match" => "\"#{rev-1}\"" }
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
	cmd = "/document?collection=#{@cid}"
	body = "{ \"Hallo\" : \"World\" }"
	doc = ArangoDB.post(cmd, :body => body)

	doc.code.should eq(201)

	location = doc.headers['location']
	location.should be_kind_of(String)

	did = doc.parsed_response['_id']
	rev = doc.parsed_response['_rev']

	# update document
	cmd = "/document/#{did}"
	body = "{ \"World\" : \"Hallo\" }"
        doc = ArangoDB.log_put("#{prefix}", cmd, :body => body)

	doc.code.should eq(200)
	doc.parsed_response['error'].should eq(false)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	did2 = doc.parsed_response['_id']
	did2.should be_kind_of(String)
	did2.should eq(did)
	
	rev2 = doc.parsed_response['_rev']
	rev2.should be_kind_of(Integer)
	rev2.should_not eq(rev)

	ArangoDB.delete(location)

	ArangoDB.size_collection(@cid).should eq(0)
      end

      it "create a document and update it, using if-match" do
	cmd = "/document?collection=#{@cid}"
	body = "{ \"Hallo\" : \"World\" }"
	doc = ArangoDB.post(cmd, :body => body)

	doc.code.should eq(201)

	location = doc.headers['location']
	location.should be_kind_of(String)

	did = doc.parsed_response['_id']
	rev = doc.parsed_response['_rev']

	# update document, different revision
	cmd = "/document/#{did}"
	hdr = { "if-match" => "\"#{rev-1}\"" }
	body = "{ \"World\" : \"Hallo\" }"
        doc = ArangoDB.log_put("#{prefix}-if-match-other", cmd, :headers => hdr, :body => body)

	doc.code.should eq(412)
	doc.parsed_response['error'].should eq(true)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	did2 = doc.parsed_response['_id']
	did2.should be_kind_of(String)
	did2.should eq(did)
	
	rev2 = doc.parsed_response['_rev']
	rev2.should be_kind_of(Integer)
	rev2.should eq(rev)

	# update document, same revision
	cmd = "/document/#{did}"
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
	rev2.should be_kind_of(Integer)
	rev2.should_not eq(rev)

	ArangoDB.delete(location)

	ArangoDB.size_collection(@cid).should eq(0)
      end

      it "create a document and update it, using if-match and last-write wins" do
	cmd = "/document?collection=#{@cid}"
	body = "{ \"Hallo\" : \"World\" }"
	doc = ArangoDB.post(cmd, :body => body)

	doc.code.should eq(201)

	location = doc.headers['location']
	location.should be_kind_of(String)

	did = doc.parsed_response['_id']
	rev = doc.parsed_response['_rev']

	# update document, different revision
	cmd = "/document/#{did}?policy=last"
	hdr = { "if-match" => "\"#{rev-1}\"" }
	body = "{ \"World\" : \"Hallo\" }"
        doc = ArangoDB.log_put("#{prefix}-if-match-other-last-write", cmd, :headers => hdr, :body => body)

	doc.code.should eq(200)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")
	doc.parsed_response['error'].should eq(false)

	did2 = doc.parsed_response['_id']
	did2.should be_kind_of(String)
	did2.should eq(did)
	
	rev2 = doc.parsed_response['_rev']
	rev2.should be_kind_of(Integer)
	rev2.should_not eq(rev)

	ArangoDB.delete(location)

	ArangoDB.size_collection(@cid).should eq(0)
      end

      it "create a document and update it, using rev" do
	cmd = "/document?collection=#{@cid}"
	body = "{ \"Hallo\" : \"World\" }"
	doc = ArangoDB.post(cmd, :body => body)

	doc.code.should eq(201)

	location = doc.headers['location']
	location.should be_kind_of(String)

	did = doc.parsed_response['_id']
	rev = doc.parsed_response['_rev']

	# update document, different revision
	cmd = "/document/#{did}?rev=#{rev-1}"
	body = "{ \"World\" : \"Hallo\" }"
        doc = ArangoDB.log_put("#{prefix}-rev-other", cmd, :body => body)

	doc.code.should eq(412)
	doc.parsed_response['error'].should eq(true)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	did2 = doc.parsed_response['_id']
	did2.should be_kind_of(String)
	did2.should eq(did)
	
	rev2 = doc.parsed_response['_rev']
	rev2.should be_kind_of(Integer)
	rev2.should eq(rev)

	# update document, same revision
	cmd = "/document/#{did}?rev=#{rev}"
	body = "{ \"World\" : \"Hallo\" }"
        doc = ArangoDB.log_put("#{prefix}-rev", cmd, :body => body)

	doc.code.should eq(200)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")
	doc.parsed_response['error'].should eq(false)

	did2 = doc.parsed_response['_id']
	did2.should be_kind_of(String)
	did2.should eq(did)

	rev2 = doc.parsed_response['_rev']
	rev2.should be_kind_of(Integer)
	rev2.should_not eq(rev)

	ArangoDB.delete(location)

	ArangoDB.size_collection(@cid).should eq(0)
      end

      it "create a document and update it, using rev and last-write wins" do
	cmd = "/document?collection=#{@cid}"
	body = "{ \"Hallo\" : \"World\" }"
	doc = ArangoDB.post(cmd, :body => body)

	doc.code.should eq(201)

	location = doc.headers['location']
	location.should be_kind_of(String)

	did = doc.parsed_response['_id']
	rev = doc.parsed_response['_rev']

	# update document, different revision
	cmd = "/document/#{did}?policy=last&rev=#{rev-1}"
	body = "{ \"World\" : \"Hallo\" }"
        doc = ArangoDB.log_put("#{prefix}-rev-other-last-write", cmd, :body => body)

	doc.code.should eq(200)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")
	doc.parsed_response['error'].should eq(false)

	did2 = doc.parsed_response['_id']
	did2.should be_kind_of(String)
	did2.should eq(did)
	
	rev2 = doc.parsed_response['_rev']
	rev2.should be_kind_of(Integer)
	rev2.should_not eq(rev)

	ArangoDB.delete(location)

	ArangoDB.size_collection(@cid).should eq(0)
      end
    end

  end
end
