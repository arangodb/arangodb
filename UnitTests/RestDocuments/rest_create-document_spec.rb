require 'rspec'
require './avocadodb.rb'

describe AvocadoDB do
  context "creating a document in a collection" do

################################################################################
## error handling
################################################################################

    context "error handling" do
      it "returns an error if collection idenifier is missing" do
	cmd = "/document"
        doc = AvocadoDB.post(cmd)

	doc.code.should eq(400)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(1202)
	doc.parsed_response['code'].should eq(400)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	AvocadoDB.log(:method => :post, :url => cmd, :result => doc, :output => "rest_create-document-missing-cid")
      end

      it "returns an error if url contains a suffix" do
	cmd = "/document/123456"
        doc = AvocadoDB.post(cmd)

	doc.code.should eq(400)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(503)
	doc.parsed_response['code'].should eq(400)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	AvocadoDB.log(:method => :post, :url => cmd, :result => doc, :output => "rest_create.document-superfluous-suffix")
      end

      it "returns an error if the collection identifier is unknown" do
	cmd = "/document?collection=123456"
        doc = AvocadoDB.post(cmd)

	doc.code.should eq(404)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(1201)
	doc.parsed_response['code'].should eq(404)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	AvocadoDB.log(:method => :post, :url => cmd, :result => doc, :output => "rest_create-document-unknown-cid")
      end

      it "returns an error if the collection name is unknown" do
	cmd = "/document?collection=unknown_collection"
        doc = AvocadoDB.post(cmd)

	doc.code.should eq(404)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(1201)
	doc.parsed_response['code'].should eq(404)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	AvocadoDB.log(:method => :post, :url => cmd, :result => doc, :output => "rest_create-document-unknown-name")
      end

      it "returns an error if the JSON body is corrupted" do
	id = AvocadoDB.create_collection("UnitTestsCollectionBasics")

	id.should be_kind_of(Integer)
	id.should_not be_zero

	cmd = "/document?collection=#{id}"
	body = "{ 1 : 2 }"
        doc = AvocadoDB.post(cmd, :body => body)

	doc.code.should eq(400)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(502)
	doc.parsed_response['code'].should eq(400)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	AvocadoDB.log(:method => :post, :url => cmd, :body => body, :result => doc, :output => "rest_create-document-bad-json")
	AvocadoDB.drop_collection("UnitTestsCollectionBasics")
      end
    end

################################################################################
## known collection identifier, waitForSync = true
################################################################################

    context "known collection identifier, waitForSync = true" do
      before do
	@cn = "UnitTestsCollectionBasics"
	@cid = AvocadoDB.create_collection(@cn)
      end

      after do
	AvocadoDB.drop_collection(@cn)
      end

      it "creating a new document" do
	cmd = "/document?collection=#{@cid}"
	body = "{ \"Hallo\" : \"World\" }"
	doc = AvocadoDB.post(cmd, :body => body)

	doc.code.should eq(201)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")
	doc.parsed_response['error'].should eq(false)

	etag = doc.headers['etag']
	etag.should be_kind_of(String)

	location = doc.headers['location']
	location.should be_kind_of(String)

	rev = doc.parsed_response['_rev']
	rev.should be_kind_of(Integer)

	did = doc.parsed_response['_id']
	did.should be_kind_of(String)
	
	match = /([0-9]*)\/([0-9]*)/.match(did)

	match[1].should eq("#{@cid}")

	etag.should eq("\"#{rev}\"")
	location.should eq("/document/#{did}")

	AvocadoDB.log(:method => :post, :url => cmd, :body => body, :result => doc, :output => "rest_create-document")

	AvocadoDB.delete(location)
      end
    end

################################################################################
## known collection identifier, waitForSync = false
################################################################################

    context "known collection identifier, waitForSync = false" do
      before do
	@cn = "UnitTestsCollectionUnsynced"
	@cid = AvocadoDB.create_collection(@cn, false)
      end

      after do
	AvocadoDB.drop_collection(@cn)
      end

      it "creating a new document" do
	cmd = "/document?collection=#{@cid}"
	body = "{ \"Hallo\" : \"World\" }"
	doc = AvocadoDB.post(cmd, :body => body)

	doc.code.should eq(202)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")
	doc.parsed_response['error'].should eq(false)

	etag = doc.headers['etag']
	etag.should be_kind_of(String)

	location = doc.headers['location']
	location.should be_kind_of(String)

	rev = doc.parsed_response['_rev']
	rev.should be_kind_of(Integer)

	did = doc.parsed_response['_id']
	did.should be_kind_of(String)
	
	match = /([0-9]*)\/([0-9]*)/.match(did)

	match[1].should eq("#{@cid}")

	etag.should eq("\"#{rev}\"")
	location.should eq("/document/#{did}")

	AvocadoDB.log(:method => :post, :url => cmd, :body => body, :result => doc, :output => "rest_create-document-accept")

	AvocadoDB.delete(location)
      end
    end

################################################################################
## known collection name
################################################################################

    context "known collection name" do
      before do
	@cn = "UnitTestsCollectionBasics"
	@cid = AvocadoDB.create_collection(@cn)
      end

      after do
	AvocadoDB.drop_collection(@cn)
      end

      it "creating a new document" do
	cmd = "/document?collection=#{@cn}"
	body = "{ \"Hallo\" : \"World\" }"
	doc = AvocadoDB.post(cmd, :body => body)

	doc.code.should eq(201)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")
	doc.parsed_response['error'].should eq(false)

	etag = doc.headers['etag']
	etag.should be_kind_of(String)

	location = doc.headers['location']
	location.should be_kind_of(String)

	rev = doc.parsed_response['_rev']
	rev.should be_kind_of(Integer)

	did = doc.parsed_response['_id']
	did.should be_kind_of(String)
	
	match = /([0-9]*)\/([0-9]*)/.match(did)

	match[1].should eq("#{@cid}")

	etag.should eq("\"#{rev}\"")
	location.should eq("/document/#{did}")

	AvocadoDB.log(:method => :post, :url => cmd, :body => body, :result => doc, :output => "rest_create-document-new-named-collection")

	AvocadoDB.delete(location)
      end
    end

################################################################################
## unknown collection name
################################################################################

    context "unknown collection name" do
      before do
	@cn = "UnitTestsCollectionNamed#{Time.now.to_i}"
      end

      after do
	AvocadoDB.drop_collection(@cn)
      end

      it "returns an error if collection is unknown" do
	cmd = "/document?collection=#{@cn}"
	body = "{ \"Hallo\" : \"World\" }"
	doc = AvocadoDB.post(cmd, :body => body)

	doc.code.should eq(404)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(1201)
	doc.parsed_response['code'].should eq(404)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	AvocadoDB.log(:method => :post, :url => cmd, :body => body, :result => doc)
      end

      it "create the collection and the document" do
	cmd = "/document?collection=#{@cn}&create=true"
	body = "{ \"Hallo\" : \"World\" }"
	doc = AvocadoDB.post(cmd, :body => body)

	doc.code.should eq(202)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")
	doc.parsed_response['error'].should eq(false)

	etag = doc.headers['etag']
	etag.should be_kind_of(String)

	location = doc.headers['location']
	location.should be_kind_of(String)

	rev = doc.parsed_response['_rev']
	rev.should be_kind_of(Integer)

	did = doc.parsed_response['_id']
	did.should be_kind_of(String)
	
	etag.should eq("\"#{rev}\"")
	location.should eq("/document/#{did}")

	AvocadoDB.log(:method => :post, :url => cmd, :body => body, :result => doc, :output => "rest_create-document-create-collection")

	AvocadoDB.delete(location)
      end
    end

  end
end
