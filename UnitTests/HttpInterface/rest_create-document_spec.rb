require 'rspec'
require './avocadodb.rb'

describe AvocadoDB do
  prefix = "rest_create-document"

  context "creating a document:" do

################################################################################
## error handling
################################################################################

    context "error handling:" do
      it "returns an error if url contains a suffix" do
	cmd = "/document/123456"
	body = "{}"
        doc = AvocadoDB.log_post("#{prefix}-superfluous-suffix", cmd, :body => body)

	doc.code.should eq(400)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(601)
	doc.parsed_response['code'].should eq(400)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")
      end

      it "returns an error if collection idenifier is missing" do
	cmd = "/document"
	body = "{}"
        doc = AvocadoDB.log_post("#{prefix}-missing-cid", cmd, :body => body)

	doc.code.should eq(400)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(1204)
	doc.parsed_response['code'].should eq(400)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")
      end

      it "returns an error if the collection identifier is unknown" do
	cmd = "/document?collection=123456"
	body = "{}"
        doc = AvocadoDB.log_post("#{prefix}-unknown-cid", cmd, :body => body)

	doc.code.should eq(404)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(1203)
	doc.parsed_response['code'].should eq(404)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")
      end

      it "returns an error if the collection name is unknown" do
	cmd = "/document?collection=unknown_collection"
	body = "{}"
        doc = AvocadoDB.log_post("#{prefix}-unknown-name", cmd, :body => body)

	doc.code.should eq(404)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(1203)
	doc.parsed_response['code'].should eq(404)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")
      end

      it "returns an error if the JSON body is corrupted" do
	cn = "UnitTestsCollectionBasics"
	id = AvocadoDB.create_collection(cn)

	id.should be_kind_of(Integer)
	id.should_not be_zero

	cmd = "/document?collection=#{id}"
	body = "{ 1 : 2 }"
        doc = AvocadoDB.log_post("#{prefix}-bad-json", cmd, :body => body)

	doc.code.should eq(400)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(600)
	doc.parsed_response['code'].should eq(400)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	AvocadoDB.size_collection(cn).should eq(0)

	AvocadoDB.drop_collection(cn)
      end
    end

################################################################################
## known collection identifier, waitForSync = true
################################################################################

    context "known collection identifier, waitForSync = true:" do
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
	doc = AvocadoDB.log_post("#{prefix}", cmd, :body => body)

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

	AvocadoDB.delete(location)

	AvocadoDB.size_collection(@cid).should eq(0)
      end

      it "creating a new umlaut document" do
	cmd = "/document?collection=#{@cid}"
	body = "{ \"Hallo\" : \"öäüÖÄÜßあ寿司\" }"
	doc = AvocadoDB.log_post("#{prefix}-complex", cmd, :body => body)

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

	cmd = "/document/#{did}"
	doc = AvocadoDB.log_get("#{prefix}-complex", cmd)

	doc.code.should eq(200)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	newBody = doc.body()
	newBody = newBody.sub!(/^.*"Hallo":"([^"]*)".*$/, '\1')

	newBody.should eq("\\u00F6\\u00E4\\u00FC\\u00D6\\u00C4\\u00DC\\u00DF\\u3042\\u5BFF\\u53F8")

	doc.parsed_response['Hallo'].should eq('öäüÖÄÜßあ寿司')
	doc.parsed_response['str"ange'].should eq('""a')

	AvocadoDB.delete(location)

	AvocadoDB.size_collection(@cid).should eq(0)
      end
    end

################################################################################
## known collection identifier, waitForSync = false
################################################################################

    context "known collection identifier, waitForSync = false:" do
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
	doc = AvocadoDB.log_post("#{prefix}-accept", cmd, :body => body)

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

	AvocadoDB.delete(location)

	AvocadoDB.size_collection(@cid).should eq(0)
      end
    end

################################################################################
## known collection name
################################################################################

    context "known collection name:" do
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
	doc = AvocadoDB.log_post("#{prefix}-named-collection", cmd, :body => body)

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

	AvocadoDB.delete(location)

	AvocadoDB.size_collection(@cid).should eq(0)
      end
    end

################################################################################
## unknown collection name
################################################################################

    context "unknown collection name:" do
      before do
	@cn = "UnitTestsCollectionNamed#{Time.now.to_i}"
      end

      after do
	AvocadoDB.drop_collection(@cn)
      end

      it "returns an error if collection is unknown" do
	cmd = "/document?collection=#{@cn}"
	body = "{ \"Hallo\" : \"World\" }"
	doc = AvocadoDB.log_post("#{prefix}-unknown-collection-name", cmd, :body => body)

	doc.code.should eq(404)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(1203)
	doc.parsed_response['code'].should eq(404)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")
      end

      it "create the collection and the document" do
	cmd = "/document?collection=#{@cn}&createCollection=true"
	body = "{ \"Hallo\" : \"World\" }"
	doc = AvocadoDB.log_post("#{prefix}-create-collection", cmd, :body => body)

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

	AvocadoDB.delete(location)

	AvocadoDB.size_collection(@cn).should eq(0)
      end
    end

  end
end
