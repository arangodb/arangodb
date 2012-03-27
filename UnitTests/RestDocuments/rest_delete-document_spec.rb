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
        doc = AvocadoDB.delete(cmd)

	doc.code.should eq(400)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(400)
	doc.parsed_response['code'].should eq(400)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	AvocadoDB.log(:method => :post, :url => cmd, :result => doc, :output => "#{prefix}-missing-handle")
      end

      it "returns an error if document handle is corrupted" do
	cmd = "/document/123456"
        doc = AvocadoDB.delete(cmd)

	doc.code.should eq(400)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(400)
	doc.parsed_response['code'].should eq(400)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	AvocadoDB.log(:method => :get, :url => cmd, :result => doc, :output => "#{prefix}-bad-handle")
      end

      it "returns an error if document handle is corrupted" do
	cmd = "/document//123456"
        doc = AvocadoDB.delete(cmd)

	doc.code.should eq(400)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(600)
	doc.parsed_response['code'].should eq(400)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	AvocadoDB.log(:method => :get, :url => cmd, :result => doc, :output => "#{prefix}-bad-handle2")
      end

      it "returns an error if collection identifier is unknown" do
	cmd = "/document/123456/234567"
        doc = AvocadoDB.delete(cmd)

	doc.code.should eq(404)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(1203)
	doc.parsed_response['code'].should eq(404)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	AvocadoDB.log(:method => :get, :url => cmd, :result => doc, :output => "#{prefix}-unknown-cid")
      end

      it "returns an error if document handle is unknown" do
	cmd = "/document/#{@cid}/234567"
        doc = AvocadoDB.delete(cmd)

	doc.code.should eq(404)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(1202)
	doc.parsed_response['code'].should eq(404)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	AvocadoDB.log(:method => :get, :url => cmd, :result => doc, :output => "#{prefix}-unknown-handle")
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
        doc = AvocadoDB.delete(cmd, :headers => hdr)

	doc.code.should eq(400)
	doc.parsed_response['error'].should eq(true)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	AvocadoDB.log(:method => :get, :url => cmd, :headers => hdr, :result => doc, :output => "#{prefix}-policy-bad")

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
        doc = AvocadoDB.delete(cmd)

	doc.code.should eq(200)
	doc.parsed_response['error'].should eq(false)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	did2 = doc.parsed_response['_id']
	did2.should be_kind_of(String)
	did2.should eq(did)
	
	rev2 = doc.parsed_response['_rev']
	rev2.should be_kind_of(Integer)
	rev2.should eq(rev)

	AvocadoDB.log(:method => :get, :url => cmd, :result => doc, :output => "#{prefix}")

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
        doc = AvocadoDB.delete(cmd, :headers => hdr)

	doc.code.should eq(412)
	doc.parsed_response['error'].should eq(true)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	did2 = doc.parsed_response['_id']
	did2.should be_kind_of(String)
	did2.should eq(did)
	
	rev2 = doc.parsed_response['_rev']
	rev2.should be_kind_of(Integer)
	rev2.should eq(rev)

	AvocadoDB.log(:method => :get, :url => cmd, :headers => hdr, :result => doc, :output => "#{prefix}-if-match-other")

	# delete document, same revision
	cmd = "/document/#{did}"
	hdr = { "if-match" => "\"#{rev}\"" }
        doc = AvocadoDB.delete(cmd, :headers => hdr)

	doc.code.should eq(200)
	doc.parsed_response['error'].should eq(false)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	did2 = doc.parsed_response['_id']
	did2.should be_kind_of(String)
	did2.should eq(did)
	

	rev2 = doc.parsed_response['_rev']
	rev2.should be_kind_of(Integer)
	rev2.should eq(rev)

	AvocadoDB.log(:method => :get, :url => cmd, :headers => hdr, :result => doc, :output => "#{prefix}-if-match")

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
        doc = AvocadoDB.delete(cmd, :headers => hdr)

	doc.code.should eq(200)
	doc.parsed_response['error'].should eq(false)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	did2 = doc.parsed_response['_id']
	did2.should be_kind_of(String)
	did2.should eq(did)
	
	rev2 = doc.parsed_response['_rev']
	rev2.should be_kind_of(Integer)
	rev2.should eq(rev)

	AvocadoDB.log(:method => :get, :url => cmd, :headers => hdr, :result => doc, :output => "#{prefix}-if-match-other-last-write")

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
        doc = AvocadoDB.delete(cmd)

	doc.code.should eq(412)
	doc.parsed_response['error'].should eq(true)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	did2 = doc.parsed_response['_id']
	did2.should be_kind_of(String)
	did2.should eq(did)
	
	rev2 = doc.parsed_response['_rev']
	rev2.should be_kind_of(Integer)
	rev2.should eq(rev)

	AvocadoDB.log(:method => :get, :url => cmd, :result => doc, :output => "#{prefix}-rev-other")

	# delete document, same revision
	cmd = "/document/#{did}?rev=#{rev}"
        doc = AvocadoDB.delete(cmd)

	doc.code.should eq(200)
	doc.parsed_response['error'].should eq(false)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	did2 = doc.parsed_response['_id']
	did2.should be_kind_of(String)
	did2.should eq(did)
	

	rev2 = doc.parsed_response['_rev']
	rev2.should be_kind_of(Integer)
	rev2.should eq(rev)

	AvocadoDB.log(:method => :get, :url => cmd, :result => doc, :output => "#{prefix}-rev")

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
        doc = AvocadoDB.delete(cmd, :headers => hdr)

	doc.code.should eq(200)
	doc.parsed_response['error'].should eq(false)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	did2 = doc.parsed_response['_id']
	did2.should be_kind_of(String)
	did2.should eq(did)
	
	rev2 = doc.parsed_response['_rev']
	rev2.should be_kind_of(Integer)
	rev2.should eq(rev)

	AvocadoDB.log(:method => :get, :url => cmd, :headers => hdr, :result => doc, :output => "#{prefix}-rev-other-last-write")

	AvocadoDB.size_collection(@cid).should eq(0)
      end
    end

  end
end

