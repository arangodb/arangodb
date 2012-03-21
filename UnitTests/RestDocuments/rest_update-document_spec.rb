require 'rspec'
require './avocadodb.rb'

describe AvocadoDB do
  prefix = "rest_update-document"

  context "update a document in a collection" do

################################################################################
## error handling
################################################################################

    context "error handling" do
      before do
	@cn = "UnitTestsCollectionBasics"
	@cid = AvocadoDB.create_collection(@cn)
      end

      after do
	AvocadoDB.drop_collection(@cn)
      end

      it "returns an error if document handle is missing" do
	cmd = "/document"
        doc = AvocadoDB.put(cmd)

	doc.code.should eq(400)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(400)
	doc.parsed_response['code'].should eq(400)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	AvocadoDB.log(:method => :post, :url => cmd, :result => doc, :output => "#{prefix}-missing-handle")
      end

      it "returns an error if document handle is corrupted" do
	cmd = "/document/123456"
        doc = AvocadoDB.put(cmd)

	doc.code.should eq(400)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(400)
	doc.parsed_response['code'].should eq(400)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	AvocadoDB.log(:method => :get, :url => cmd, :result => doc, :output => "#{prefix}-bad-handle")
      end

      it "returns an error if document handle is corrupted" do
	cmd = "/document//123456"
        doc = AvocadoDB.put(cmd)

	doc.code.should eq(400)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(600)
	doc.parsed_response['code'].should eq(400)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	AvocadoDB.log(:method => :get, :url => cmd, :result => doc, :output => "#{prefix}-bad-handle2")
      end

      it "returns an error if collection identifier is unknown" do
	cmd = "/document/123456/234567"
        doc = AvocadoDB.put(cmd)

	doc.code.should eq(404)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(1203)
	doc.parsed_response['code'].should eq(404)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	AvocadoDB.log(:method => :get, :url => cmd, :result => doc, :output => "#{prefix}-unknown-cid")
      end

      it "returns an error if document handle is unknown" do
	cmd = "/document/#{@cid}/234567"
	body = "{}"
        doc = AvocadoDB.put(cmd, :body => body)

	doc.code.should eq(404)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(1202)
	doc.parsed_response['code'].should eq(404)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	AvocadoDB.log(:method => :get, :url => cmd, :body => body, :result => doc, :output => "#{prefix}-unknown-handle")
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
        doc = AvocadoDB.put(cmd, :headers => hdr)

	doc.code.should eq(400)
	doc.parsed_response['error'].should eq(true)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	AvocadoDB.log(:method => :get, :url => cmd, :headers => hdr, :result => doc, :output => "#{prefix}-policy-bad")
      end
    end

################################################################################
## updating documents
################################################################################

    context "updating documents" do
      before do
	@cn = "UnitTestsCollectionBasics"
	@cid = AvocadoDB.create_collection(@cn)
      end

      after do
	AvocadoDB.drop_collection(@cn)
      end

      it "create a document and update it" do
	cmd = "/document?collection=#{@cid}"
	body = "{ \"Hallo\" : \"World\" }"
	doc = AvocadoDB.post(cmd, :body => body)

	doc.code.should eq(201)

	location = doc.headers['location']
	location.should be_kind_of(String)

	did = doc.parsed_response['_id']
	rev = doc.parsed_response['_rev']

	# update document
	cmd = "/document/#{did}"
	body = "{ \"World\" : \"Hallo\" }"
        doc = AvocadoDB.put(cmd, :body => body)

	doc.code.should eq(200)
	doc.parsed_response['error'].should eq(false)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	did2 = doc.parsed_response['_id']
	did2.should be_kind_of(String)
	did2.should eq(did)
	
	rev2 = doc.parsed_response['_rev']
	rev2.should be_kind_of(Integer)
	rev2.should_not eq(rev)

	AvocadoDB.log(:method => :get, :url => cmd, :body => body, :result => doc, :output => "#{prefix}")

	AvocadoDB.delete(location)
      end

      it "create a document and update it, using if-match" do
	cmd = "/document?collection=#{@cid}"
	body = "{ \"Hallo\" : \"World\" }"
	doc = AvocadoDB.post(cmd, :body => body)

	doc.code.should eq(201)

	location = doc.headers['location']
	location.should be_kind_of(String)

	did = doc.parsed_response['_id']
	rev = doc.parsed_response['_rev']

	# update document, different revision
	cmd = "/document/#{did}"
	hdr = { "if-match" => "\"#{rev-1}\"" }
	body = "{ \"World\" : \"Hallo\" }"
        doc = AvocadoDB.put(cmd, :headers => hdr, :body => body)

	doc.code.should eq(412)
	doc.parsed_response['error'].should eq(true)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	did2 = doc.parsed_response['_id']
	did2.should be_kind_of(String)
	did2.should eq(did)
	
	rev2 = doc.parsed_response['_rev']
	rev2.should be_kind_of(Integer)
	rev2.should eq(rev)

	AvocadoDB.log(:method => :get, :url => cmd, :body => body, :headers => hdr, :result => doc, :output => "#{prefix}-if-match-other")

	# update document, same revision
	cmd = "/document/#{did}"
	hdr = { "if-match" => "\"#{rev}\"" }
	body = "{ \"World\" : \"Hallo\" }"
        doc = AvocadoDB.delete(cmd, :headers => hdr, :body => body)

	doc.code.should eq(200)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")
	doc.parsed_response['error'].should eq(false)

	did2 = doc.parsed_response['_id']
	did2.should be_kind_of(String)
	did2.should eq(did)
	

	rev2 = doc.parsed_response['_rev']
	rev2.should be_kind_of(Integer)
	rev2.should eq(rev)

	AvocadoDB.log(:method => :get, :url => cmd, :body => body, :headers => hdr, :result => doc, :output => "#{prefix}-if-match")
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
	body = "{ \"World\" : \"Hallo\" }"
        doc = AvocadoDB.delete(cmd, :headers => hdr, :body => body)

	doc.code.should eq(200)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")
	doc.parsed_response['error'].should eq(false)

	did2 = doc.parsed_response['_id']
	did2.should be_kind_of(String)
	did2.should eq(did)
	
	rev2 = doc.parsed_response['_rev']
	rev2.should be_kind_of(Integer)
	rev2.should eq(rev)

	AvocadoDB.log(:method => :get, :url => cmd, :body => body, :headers => hdr, :result => doc, :output => "#{prefix}-if-match-other-last-write")
      end
    end

  end
end
