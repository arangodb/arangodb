require 'rspec'
require './avocadodb.rb'

describe AvocadoDB do
  prefix = "rest_read-document"

  context "reading a document in a collection" do

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

      it "returns an error if document handle is corrupted" do
	cmd = "/document/123456"
        doc = AvocadoDB.get(cmd)

	doc.code.should eq(400)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(503)
	doc.parsed_response['code'].should eq(400)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	AvocadoDB.log(:method => :get, :url => cmd, :result => doc, :output => "#{prefix}-bad-handle")
      end

      it "returns an error if document handle is corrupted" do
	cmd = "/document//123456"
        doc = AvocadoDB.get(cmd)

	doc.code.should eq(400)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(503)
	doc.parsed_response['code'].should eq(400)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	AvocadoDB.log(:method => :get, :url => cmd, :result => doc, :output => "#{prefix}-bad-handle2")
      end

      it "returns an error if collection identifier is unknown" do
	cmd = "/document/123456/234567"
        doc = AvocadoDB.get(cmd)

	doc.code.should eq(404)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(1201)
	doc.parsed_response['code'].should eq(404)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	AvocadoDB.log(:method => :get, :url => cmd, :result => doc, :output => "#{prefix}-unknown-cid")
      end

      it "returns an error if document handle is unknown" do
	cmd = "/document/#{@cid}/234567"
        doc = AvocadoDB.get(cmd)

	doc.code.should eq(404)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(1200)
	doc.parsed_response['code'].should eq(404)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	AvocadoDB.log(:method => :get, :url => cmd, :result => doc, :output => "#{prefix}-unknown-handle")
      end
    end

################################################################################
## reading documents
################################################################################

    context "reading documents" do
      before do
	@cn = "UnitTestsCollectionBasics"
	@cid = AvocadoDB.create_collection(@cn)
      end

      after do
	AvocadoDB.drop_collection(@cn)
      end

      it "create a document and read it" do
	cmd = "/document?collection=#{@cid}"
	body = "{ \"Hallo\" : \"World\" }"
	doc = AvocadoDB.post(cmd, :body => body)

	doc.code.should eq(201)

	location = doc.headers['location']
	location.should be_kind_of(String)

	did = doc.parsed_response['_id']
	rev = doc.parsed_response['_rev']

	# get document
	cmd = "/document/#{did}"
        doc = AvocadoDB.get(cmd)

	doc.code.should eq(200)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	did2 = doc.parsed_response['_id']
	did2.should be_kind_of(String)
	did2.should eq(did)
	
	rev2 = doc.parsed_response['_rev']
	rev2.should be_kind_of(Integer)
	rev2.should eq(rev)

	etag = doc.headers['etag']
	etag.should be_kind_of(String)

	etag.should eq("\"#{rev}\"")

	AvocadoDB.log(:method => :get, :url => cmd, :result => doc, :output => "#{prefix}")

	AvocadoDB.delete(location)
      end

      it "create a document and read it, use if-none-match" do
	cmd = "/document?collection=#{@cid}"
	body = "{ \"Hallo\" : \"World\" }"
	doc = AvocadoDB.post(cmd, :body => body)

	doc.code.should eq(201)

	location = doc.headers['location']
	location.should be_kind_of(String)

	did = doc.parsed_response['_id']
	rev = doc.parsed_response['_rev']

	# get document, if-none-match with same rev
	cmd = "/document/#{did}"
	hdr = { "if-none-match" => "\"#{rev}\"" }
        doc = AvocadoDB.get(cmd, :headers => hdr)

	doc.code.should eq(304)

	etag = doc.headers['etag']
	etag.should be_kind_of(String)

	etag.should eq("\"#{rev}\"")

	AvocadoDB.log(:method => :get, :url => cmd, :headers => hdr, :result => doc, :output => "#{prefix}-if-none-match")

	# get document, if-none-match with different rev
	cmd = "/document/#{did}"
	hdr = { "if-none-match" => "\"#{rev-1}\"" }
        doc = AvocadoDB.get(cmd, :headers => hdr)

	doc.code.should eq(200)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	etag = doc.headers['etag']
	etag.should be_kind_of(String)

	etag.should eq("\"#{rev}\"")

	did2 = doc.parsed_response['_id']
	did2.should be_kind_of(String)
	did2.should eq(did)
	
	rev2 = doc.parsed_response['_rev']
	rev2.should be_kind_of(Integer)
	rev2.should eq(rev)

	etag = doc.headers['etag']
	etag.should be_kind_of(String)

	etag.should eq("\"#{rev}\"")

	AvocadoDB.log(:method => :get, :url => cmd, :headers => hdr, :result => doc, :output => "#{prefix}-if-none-match-other")

	AvocadoDB.delete(location)
      end

      it "create a document and read it, use if-match" do
	cmd = "/document?collection=#{@cid}"
	body = "{ \"Hallo\" : \"World\" }"
	doc = AvocadoDB.post(cmd, :body => body)

	doc.code.should eq(201)

	location = doc.headers['location']
	location.should be_kind_of(String)

	did = doc.parsed_response['_id']
	rev = doc.parsed_response['_rev']

	# get document, if-match with same rev
	cmd = "/document/#{did}"
	hdr = { "if-match" => "\"#{rev}\"" }
        doc = AvocadoDB.get(cmd, :headers => hdr)

	doc.code.should eq(200)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	did2 = doc.parsed_response['_id']
	did2.should be_kind_of(String)
	did2.should eq(did)
	
	rev2 = doc.parsed_response['_rev']
	rev2.should be_kind_of(Integer)
	rev2.should eq(rev)

	etag = doc.headers['etag']
	etag.should be_kind_of(String)

	etag.should eq("\"#{rev}\"")

	AvocadoDB.log(:method => :get, :url => cmd, :headers => hdr, :result => doc, :output => "#{prefix}-if-match")

	# get document, if-match with different rev
	cmd = "/document/#{did}"
	hdr = { "if-match" => "\"#{rev-1}\"" }
        doc = AvocadoDB.get(cmd, :headers => hdr)

	doc.code.should eq(412)

	did2 = doc.parsed_response['_id']
	did2.should be_kind_of(String)
	did2.should eq(did)
	
	rev2 = doc.parsed_response['_rev']
	rev2.should be_kind_of(Integer)
	rev2.should eq(rev)

	AvocadoDB.log(:method => :get, :url => cmd, :headers => hdr, :result => doc, :output => "#{prefix}-if-match-other")

	AvocadoDB.delete(location)
      end
    end

  end
end
