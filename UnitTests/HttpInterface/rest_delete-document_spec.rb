require 'rspec'
require './avocadodb.rb'

describe AvocadoDB do
  prefix = "rest_delete-document"

  context "delete a document:" do

################################################################################
## error handling
################################################################################

    context "error handling:" do
      before do
	@cn = "UnitTestsCollectionBasics"
	@cid = AvocadoDB.create_collection(@cn)
      end

      after do
	AvocadoDB.drop_collection(@cn)
      end

      it "returns an error if document handle is missing" do
	cmd = "/document"
        doc = AvocadoDB.log_delete("#{prefix}-missing-handle", cmd)

	doc.code.should eq(400)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(400)
	doc.parsed_response['code'].should eq(400)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")
      end

      it "returns an error if document handle is corrupted" do
	cmd = "/document/123456"
        doc = AvocadoDB.log_delete("#{prefix}-bad-handle", cmd)

	doc.code.should eq(400)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(400)
	doc.parsed_response['code'].should eq(400)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")
      end

      it "returns an error if document handle is corrupted" do
	cmd = "/document//123456"
        doc = AvocadoDB.log_delete("#{prefix}-bad-handle2", cmd)

	doc.code.should eq(400)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(600)
	doc.parsed_response['code'].should eq(400)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")
      end

      it "returns an error if collection identifier is unknown" do
	cmd = "/document/123456/234567"
        doc = AvocadoDB.log_delete("#{prefix}-unknown-cid", cmd)

	doc.code.should eq(404)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(1203)
	doc.parsed_response['code'].should eq(404)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")
      end

      it "returns an error if document handle is unknown" do
	cmd = "/document/#{@cid}/234567"
        doc = AvocadoDB.log_delete("#{prefix}-unknown-handle", cmd)

	doc.code.should eq(404)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(1202)
	doc.parsed_response['code'].should eq(404)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")
      end

      it "returns an error if the policy parameter is bad" do
	cmd = "/document?collection=#{@cid}"
	body = "{ \"Hallo\" : \"World\" }"
	doc = AvocadoDB.post(cmd, :body => body)

	doc.code.should eq(201)

	location = doc.headers['location']
	location.should be_kind_of(String)

	did = doc.parsed_response['_id']
	rev = doc.parsed_response['_rev']

	# delete document, different revision
	cmd = "/document/#{did}?policy=last-write"
	hdr = { "if-match" => "\"#{rev-1}\"" }
        doc = AvocadoDB.log_delete("#{prefix}-policy-bad", cmd, :headers => hdr)

	doc.code.should eq(400)
	doc.parsed_response['error'].should eq(true)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	AvocadoDB.delete(location)

	AvocadoDB.size_collection(@cid).should eq(0)
      end
    end

################################################################################
## deleting documents
################################################################################

    context "deleting documents:" do
      before do
	@cn = "UnitTestsCollectionBasics"
	@cid = AvocadoDB.create_collection(@cn)
      end

      after do
	AvocadoDB.drop_collection(@cn)
      end

      it "create a document and delete it" do
	cmd = "/document?collection=#{@cid}"
	body = "{ \"Hallo\" : \"World\" }"
	doc = AvocadoDB.post(cmd, :body => body)

	doc.code.should eq(201)

	location = doc.headers['location']
	location.should be_kind_of(String)

	did = doc.parsed_response['_id']
	rev = doc.parsed_response['_rev']

	# delete document
	cmd = "/document/#{did}"
        doc = AvocadoDB.log_delete("#{prefix}", cmd)

	doc.code.should eq(200)
	doc.parsed_response['error'].should eq(false)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	did2 = doc.parsed_response['_id']
	did2.should be_kind_of(String)
	did2.should eq(did)
	
	rev2 = doc.parsed_response['_rev']
	rev2.should be_kind_of(Integer)
	rev2.should eq(rev)

	AvocadoDB.size_collection(@cid).should eq(0)
      end

      it "create a document and delete it, using if-match" do
	cmd = "/document?collection=#{@cid}"
	body = "{ \"Hallo\" : \"World\" }"
	doc = AvocadoDB.post(cmd, :body => body)

	doc.code.should eq(201)

	location = doc.headers['location']
	location.should be_kind_of(String)

	did = doc.parsed_response['_id']
	rev = doc.parsed_response['_rev']

	# delete document, different revision
	cmd = "/document/#{did}"
	hdr = { "if-match" => "\"#{rev-1}\"" }
        doc = AvocadoDB.log_delete("#{prefix}-if-match-other", cmd, :headers => hdr)

	doc.code.should eq(412)
	doc.parsed_response['error'].should eq(true)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	did2 = doc.parsed_response['_id']
	did2.should be_kind_of(String)
	did2.should eq(did)
	
	rev2 = doc.parsed_response['_rev']
	rev2.should be_kind_of(Integer)
	rev2.should eq(rev)

	# delete document, same revision
	cmd = "/document/#{did}"
	hdr = { "if-match" => "\"#{rev}\"" }
        doc = AvocadoDB.log_delete("#{prefix}-if-match", cmd, :headers => hdr)

	doc.code.should eq(200)
	doc.parsed_response['error'].should eq(false)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	did2 = doc.parsed_response['_id']
	did2.should be_kind_of(String)
	did2.should eq(did)
	
	rev2 = doc.parsed_response['_rev']
	rev2.should be_kind_of(Integer)
	rev2.should eq(rev)

	AvocadoDB.size_collection(@cid).should eq(0)
      end

      it "create a document and delete it, using if-match and last-write wins" do
	cmd = "/document?collection=#{@cid}"
	body = "{ \"Hallo\" : \"World\" }"
	doc = AvocadoDB.post(cmd, :body => body)

	doc.code.should eq(201)

	location = doc.headers['location']
	location.should be_kind_of(String)

	did = doc.parsed_response['_id']
	rev = doc.parsed_response['_rev']

	# delete document, different revision
	cmd = "/document/#{did}?policy=last"
	hdr = { "if-match" => "\"#{rev-1}\"" }
        doc = AvocadoDB.log_delete("#{prefix}-if-match-other-last-write", cmd, :headers => hdr)

	doc.code.should eq(200)
	doc.parsed_response['error'].should eq(false)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	did2 = doc.parsed_response['_id']
	did2.should be_kind_of(String)
	did2.should eq(did)
	
	rev2 = doc.parsed_response['_rev']
	rev2.should be_kind_of(Integer)
	rev2.should eq(rev)

	AvocadoDB.size_collection(@cid).should eq(0)
      end

      it "create a document and delete it, using rev" do
	cmd = "/document?collection=#{@cid}"
	body = "{ \"Hallo\" : \"World\" }"
	doc = AvocadoDB.post(cmd, :body => body)

	doc.code.should eq(201)

	location = doc.headers['location']
	location.should be_kind_of(String)

	did = doc.parsed_response['_id']
	rev = doc.parsed_response['_rev']

	# delete document, different revision
	cmd = "/document/#{did}?rev=#{rev-1}"
        doc = AvocadoDB.log_delete("#{prefix}-rev-other", cmd)

	doc.code.should eq(412)
	doc.parsed_response['error'].should eq(true)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	did2 = doc.parsed_response['_id']
	did2.should be_kind_of(String)
	did2.should eq(did)
	
	rev2 = doc.parsed_response['_rev']
	rev2.should be_kind_of(Integer)
	rev2.should eq(rev)

	# delete document, same revision
	cmd = "/document/#{did}?rev=#{rev}"
        doc = AvocadoDB.log_delete("#{prefix}-rev", cmd)

	doc.code.should eq(200)
	doc.parsed_response['error'].should eq(false)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	did2 = doc.parsed_response['_id']
	did2.should be_kind_of(String)
	did2.should eq(did)
	

	rev2 = doc.parsed_response['_rev']
	rev2.should be_kind_of(Integer)
	rev2.should eq(rev)

	AvocadoDB.size_collection(@cid).should eq(0)
      end

      it "create a document and delete it, using rev and last-write wins" do
	cmd = "/document?collection=#{@cid}"
	body = "{ \"Hallo\" : \"World\" }"
	doc = AvocadoDB.post(cmd, :body => body)

	doc.code.should eq(201)

	location = doc.headers['location']
	location.should be_kind_of(String)

	did = doc.parsed_response['_id']
	rev = doc.parsed_response['_rev']

	# delete document, different revision
	cmd = "/document/#{did}?policy=last"
	hdr = { "rev" => "\"#{rev-1}\"" }
        doc = AvocadoDB.log_delete("#{prefix}-rev-other-last-write", cmd, :headers => hdr)

	doc.code.should eq(200)
	doc.parsed_response['error'].should eq(false)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	did2 = doc.parsed_response['_id']
	did2.should be_kind_of(String)
	did2.should eq(did)
	
	rev2 = doc.parsed_response['_rev']
	rev2.should be_kind_of(Integer)
	rev2.should eq(rev)

	AvocadoDB.size_collection(@cid).should eq(0)
      end
    end

  end
end

