# coding: utf-8

require 'rspec'
require './arangodb.rb'

describe ArangoDB do
  api = "/_api/collection"
  prefix = "api-collection"

  context "dealing with collections:" do

################################################################################
## reading all collection
################################################################################

    context "all collections:" do
      before do
	for cn in ["units", "employees", "locations" ] do
	  ArangoDB.drop_collection(cn)
	  @cid = ArangoDB.create_collection(cn)
	end
      end

      after do
	for cn in ["units", "employees", "locations" ] do
	  ArangoDB.drop_collection(cn)
	end
      end

      it "returns all collections" do
	cmd = api
        doc = ArangoDB.log_get("#{prefix}-all-collections", cmd)

	doc.code.should eq(200)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)

	collections = doc.parsed_response['collections']
	names = doc.parsed_response['names']

	collections.length.should eq(3)

	for collection in collections do
	  names[collection['name']].should eq(collection)
	end
      end
    end

################################################################################
## error handling
################################################################################

    context "error handling:" do
      it "returns an error if collection identifier is unknown" do
	cmd = api + "/123456"
        doc = ArangoDB.log_get("#{prefix}-bad-identifier", cmd)

	doc.code.should eq(404)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(1203)
	doc.parsed_response['code'].should eq(404)
      end

      it "creating a collection without name" do
	cmd = api
        doc = ArangoDB.log_post("#{prefix}-create-missing-name", cmd)

	doc.code.should eq(400)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['code'].should eq(400)
	doc.parsed_response['errorNum'].should eq(1208)
      end

      it "creating a collection with an illegal name" do
	cmd = api
	body = "{ \"name\" : \"1\" }"
        doc = ArangoDB.log_post("#{prefix}-create-illegal-name", cmd, :body => body)

	doc.code.should eq(400)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['code'].should eq(400)
	doc.parsed_response['errorNum'].should eq(1208)
      end

      it "creating a collection with a duplicate name" do
	cn = "UnitTestsCollectionBasics"
	cid = ArangoDB.create_collection(cn)

	cmd = api
	body = "{ \"name\" : \"#{cn}\" }"
        doc = ArangoDB.log_post("#{prefix}-create-illegal-name", cmd, :body => body)

	doc.code.should eq(400)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['code'].should eq(400)
	doc.parsed_response['errorNum'].should eq(1207)
      end

      it "creating a collection with an illegal body" do
	cmd = api
	body = "{ name : world }"
        doc = ArangoDB.log_post("#{prefix}-create-illegal-body", cmd, :body => body)

	doc.code.should eq(400)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['code'].should eq(400)
	doc.parsed_response['errorNum'].should eq(600)
	doc.parsed_response['errorMessage'].should eq("SyntaxError: Unexpected token n")
      end

      it "creating a collection with a null body" do
	cmd = api
	body = "null"
        doc = ArangoDB.log_post("#{prefix}-create-null-body", cmd, :body => body)

	doc.code.should eq(400)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['code'].should eq(400)
	doc.parsed_response['errorNum'].should eq(1208)
      end
    end

################################################################################
## reading a collection
################################################################################

    context "reading:" do
      before do
	@cn = "UnitTestsCollectionBasics"
	ArangoDB.drop_collection(@cn)
	@cid = ArangoDB.create_collection(@cn)
      end

      after do
	ArangoDB.drop_collection(@cn)
      end

      # get
      it "finds the collection by identifier" do
	cmd = api + "/" + String(@cid)
        doc = ArangoDB.log_get("#{prefix}-get-collection-identifier", cmd)

	doc.code.should eq(200)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.parsed_response['id'].should eq(@cid)
	doc.parsed_response['name'].should eq(@cn)
	doc.parsed_response['status'].should eq(3)
	doc.headers['location'].should eq(api + "/" + String(@cid))

	cmd2 = api + "/" + String(@cid) + "/unload"
        doc = ArangoDB.put(cmd2)

        doc = ArangoDB.log_get("#{prefix}-get-collection-identifier", cmd)

	doc.code.should eq(200)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.parsed_response['id'].should eq(@cid)
	doc.parsed_response['name'].should eq(@cn)
	[2,4].include?(doc.parsed_response['status']).should be_true
	doc.headers['location'].should eq(api + "/" + String(@cid))
      end

      # get
      it "finds the collection by name" do
	cmd = api + "/" + String(@cn)
        doc = ArangoDB.log_get("#{prefix}-get-collection-name", cmd)

	doc.code.should eq(200)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.parsed_response['id'].should eq(@cid)
	doc.parsed_response['name'].should eq(@cn)
	doc.parsed_response['status'].should eq(3)
	doc.headers['location'].should eq(api + "/" + String(@cid))

	cmd2 = api + "/" + String(@cid) + "/unload"
        doc = ArangoDB.put(cmd2)

        doc = ArangoDB.log_get("#{prefix}-get-collection-name", cmd)

	doc.code.should eq(200)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.parsed_response['id'].should eq(@cid)
	doc.parsed_response['name'].should eq(@cn)
	[2,4].include?(doc.parsed_response['status']).should be_true
	doc.headers['location'].should eq(api + "/" + String(@cid))
      end

      # get count
      it "checks the size of a collection" do
	cmd = api + "/" + String(@cid) + "/count"
        doc = ArangoDB.log_get("#{prefix}-get-collection-count", cmd)

	doc.code.should eq(200)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.parsed_response['id'].should eq(@cid)
	doc.parsed_response['name'].should eq(@cn)
	doc.parsed_response['status'].should eq(3)
	doc.parsed_response['count'].should be_kind_of(Integer)
	doc.headers['location'].should eq(api + "/" + String(@cid) + "/count")
      end

      # get count
      it "checks the properties of a collection" do
	cmd = api + "/" + String(@cid) + "/properties"
        doc = ArangoDB.log_get("#{prefix}-get-collection-properties", cmd)

	doc.code.should eq(200)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.parsed_response['id'].should eq(@cid)
	doc.parsed_response['name'].should eq(@cn)
	doc.parsed_response['status'].should eq(3)
	doc.parsed_response['waitForSync'].should eq(true)
	doc.parsed_response['journalSize'].should be_kind_of(Integer)
	doc.headers['location'].should eq(api + "/" + String(@cid) + "/properties")
      end

      # get figures
      it "extracting the figures for a collection" do
	cmd = api + "/" + String(@cid) + "/figures"
        doc = ArangoDB.log_get("#{prefix}-get-collection-figures", cmd)

	doc.code.should eq(200)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.parsed_response['id'].should eq(@cid)
	doc.parsed_response['name'].should eq(@cn)
	doc.parsed_response['status'].should eq(3)
	doc.parsed_response['count'].should be_kind_of(Integer)
	doc.parsed_response['figures']['alive']['count'].should be_kind_of(Integer)
	doc.parsed_response['count'].should eq(doc.parsed_response['figures']['alive']['count'])
	doc.parsed_response['journalSize'].should be_kind_of(Integer)
	doc.headers['location'].should eq(api + "/" + String(@cid) + "/figures")
      end
    end

################################################################################
## deleting of collection
################################################################################

    context "deleting:" do
      before do
	@cn = "UnitTestsCollectionBasics"
      end

      it "delete an existing collection by identifier" do
	cid = ArangoDB.create_collection(@cn)
	cmd = api + "/" + String(cid)
        doc = ArangoDB.log_delete("#{prefix}-delete-collection-identifier", cmd)

	doc.code.should eq(200)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.parsed_response['id'].should eq(cid)

	cmd = api + "/" + String(cid)
	doc = ArangoDB.get(cmd)

	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['code'].should eq(404)
      end

      it "delete an existing collection by name" do
	cid = ArangoDB.create_collection(@cn)
	cmd = api + "/" + @cn
        doc = ArangoDB.log_delete("#{prefix}-delete-collection-name", cmd)

	doc.code.should eq(200)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.parsed_response['id'].should eq(cid)

	cmd = api + "/" + String(cid)
	doc = ArangoDB.get(cmd)

	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['code'].should eq(404)
      end
    end

################################################################################
## creating a collection
################################################################################

    context "creating:" do
      before do
	@cn = "UnitTestsCollectionBasics"
      end

      it "create a collection" do
	cmd = api
	body = "{ \"name\" : \"#{@cn}\" }"
        doc = ArangoDB.log_post("#{prefix}-create-collection", cmd, :body => body)

	doc.code.should eq(200)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.parsed_response['id'].should be_kind_of(Integer)
	doc.parsed_response['name'].should eq(@cn)
	doc.parsed_response['waitForSync'].should == false

	cmd = api + "/" + String(@cn) + "/figures"
        doc = ArangoDB.get(cmd)

	doc.parsed_response['waitForSync'].should == false

	ArangoDB.drop_collection(@cn)
      end

      it "create a collection, sync" do
	cmd = api
	body = "{ \"name\" : \"#{@cn}\", \"waitForSync\" : true }"
        doc = ArangoDB.log_post("#{prefix}-create-collection-sync", cmd, :body => body)

	doc.code.should eq(200)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.parsed_response['id'].should be_kind_of(Integer)
	doc.parsed_response['name'].should eq(@cn)
	doc.parsed_response['waitForSync'].should == true

	cmd = api + "/" + String(@cn) + "/figures"
        doc = ArangoDB.get(cmd)

	doc.parsed_response['waitForSync'].should == true

	ArangoDB.drop_collection(@cn)
      end
      
      it "create a collection, using a collection id" do
	ArangoDB.drop_collection(@cn)
	cmd = api
	body = "{ \"name\" : \"#{@cn}\", \"_id\" : 12345678 }"
        doc = ArangoDB.log_post("#{prefix}-create-collection-id", cmd, :body => body)

	doc.code.should eq(200)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.parsed_response['id'].should be_kind_of(Integer)
	doc.parsed_response['id'].should eq(12345678)
	doc.parsed_response['name'].should eq(@cn)

	ArangoDB.drop_collection(@cn)
      end
      
      it "create a collection, using duplicate collection id" do
	ArangoDB.drop_collection(@cn)
	ArangoDB.drop_collection(123456789)
	cmd = api
	body = "{ \"name\" : \"#{@cn}\", \"_id\" : 123456789 }"
        doc = ArangoDB.log_post("#{prefix}-create-collection-id-dup", cmd, :body => body)

	doc.code.should eq(200)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.parsed_response['id'].should be_kind_of(Integer)
	doc.parsed_response['id'].should eq(123456789)
	doc.parsed_response['name'].should eq(@cn)
        
	body = "{ \"name\" : \"#{@cn}2\", \"_id\" : 123456789 }"
        doc = ArangoDB.log_post("#{prefix}-create-collection-id-dup", cmd, :body => body)
	
        doc.code.should eq(400)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['code'].should eq(400)

	ArangoDB.drop_collection(@cn)
      end
      
      it "create a collection, invalid name" do
	cmd = api
	body = "{ \"name\" : \"_invalid\" }"
        doc = ArangoDB.log_post("#{prefix}-create-collection-invalid", cmd, :body => body)

	doc.code.should eq(400)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['code'].should eq(400)
      end
      
      it "create a collection, already existing" do
	ArangoDB.drop_collection(@cn)
	cmd = api
	body = "{ \"name\" : \"#{@cn}\" }"
        doc = ArangoDB.log_post("#{prefix}-create-collection-existing", cmd, :body => body)

	doc.code.should eq(200)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
        
	body = "{ \"name\" : \"#{@cn}\" }"
        doc = ArangoDB.log_post("#{prefix}-create-collection-existing", cmd, :body => body)

	doc.code.should eq(400)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['code'].should eq(400)
	
        ArangoDB.drop_collection(@cn)
      end
    end

################################################################################
## load a collection
################################################################################

	ArangoDB.drop_collection(@cn)
    context "loading:" do
      before do
	@cn = "UnitTestsCollectionBasics"
      end

      it "load a collection by identifier" do
	ArangoDB.drop_collection(@cn)
	cid = ArangoDB.create_collection(@cn)

	cmd = api + "/" + String(cid) + "/load"
        doc = ArangoDB.log_put("#{prefix}-identifier-load", cmd)

	doc.code.should eq(200)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.parsed_response['id'].should eq(cid)
	doc.parsed_response['name'].should eq(@cn)
	doc.parsed_response['status'].should eq(3)
	doc.parsed_response['count'].should be_kind_of(Integer)

	ArangoDB.drop_collection(@cn)
      end

      it "load a collection by name" do
	ArangoDB.drop_collection(@cn)
	cid = ArangoDB.create_collection(@cn)

	cmd = api + "/" + @cn + "/load"
        doc = ArangoDB.log_put("#{prefix}-name-load", cmd)

	doc.code.should eq(200)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.parsed_response['id'].should eq(cid)
	doc.parsed_response['name'].should eq(@cn)
	doc.parsed_response['status'].should eq(3)
	doc.parsed_response['count'].should be_kind_of(Integer)

	ArangoDB.drop_collection(@cn)
      end
    end

################################################################################
## unloading a collection
################################################################################

    context "unloading:" do
      before do
	@cn = "UnitTestsCollectionBasics"
      end

      it "unload a collection by identifier" do
	ArangoDB.drop_collection(@cn)
	cid = ArangoDB.create_collection(@cn)

	cmd = api + "/" + String(cid) + "/unload"
        doc = ArangoDB.log_put("#{prefix}-identifier-unload", cmd)

	doc.code.should eq(200)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.parsed_response['id'].should eq(cid)
	doc.parsed_response['name'].should eq(@cn)
        [2,4].include?(doc.parsed_response['status']).should be_true

	ArangoDB.drop_collection(@cn)
      end

      it "unload a collection by name" do
	ArangoDB.drop_collection(@cn)
	cid = ArangoDB.create_collection(@cn)

	cmd = api + "/" + @cn + "/unload"
        doc = ArangoDB.log_put("#{prefix}-name-unload", cmd)

	doc.code.should eq(200)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.parsed_response['id'].should eq(cid)
	doc.parsed_response['name'].should eq(@cn)
	[2,4].include?(doc.parsed_response['status']).should be_true

	ArangoDB.drop_collection(@cn)
      end
    end

################################################################################
## truncate a collection
################################################################################

    context "truncating:" do
      before do
	@cn = "UnitTestsCollectionBasics"
	@cid = ArangoDB.create_collection(@cn)
      end

      after do
	ArangoDB.drop_collection(@cn)
      end

      it "truncate a collection by identifier" do
	cmd = "/_api/document?collection=#{@cid}"
	body = "{ \"Hallo\" : \"World\" }"

	for i in ( 1 .. 10 )
	  doc = ArangoDB.post(cmd, :body => body)
	end

	ArangoDB.size_collection(@cid).should eq(10)

	cmd = api + "/" + String(@cid) + "/truncate"
        doc = ArangoDB.log_put("#{prefix}-identifier-truncate", cmd)

	doc.code.should eq(200)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.parsed_response['id'].should eq(@cid)
	doc.parsed_response['name'].should eq(@cn)
	doc.parsed_response['status'].should eq(3)

	ArangoDB.size_collection(@cid).should eq(0)

	ArangoDB.drop_collection(@cn)
      end
    end

################################################################################
## renames a collection
################################################################################

    context "renaming:" do
      it "rename a collection by identifier" do
	cn = "UnitTestsCollectionBasics"
	ArangoDB.drop_collection(cn)
	cid = ArangoDB.create_collection(cn)

	cmd = "/_api/document?collection=#{cid}"
	body = "{ \"Hallo\" : \"World\" }"

	for i in ( 1 .. 10 )
	  doc = ArangoDB.post(cmd, :body => body)
	end

	ArangoDB.size_collection(cid).should eq(10)
	ArangoDB.size_collection(cn).should eq(10)

	cn2 = "UnitTestsCollectionBasics2"
	ArangoDB.drop_collection(cn2)

	body = "{ \"name\" : \"#{cn2}\" }"
	cmd = api + "/" + String(cid) + "/rename"
        doc = ArangoDB.log_put("#{prefix}-identifier-rename", cmd, :body => body)

	doc.code.should eq(200)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.parsed_response['id'].should eq(cid)
	doc.parsed_response['name'].should eq(cn2)
	doc.parsed_response['status'].should eq(3)

	ArangoDB.size_collection(cid).should eq(10)
	ArangoDB.size_collection(cn2).should eq(10)

	cmd = api + "/" + String(cn)
        doc = ArangoDB.get(cmd)

	doc.code.should eq(404)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(1203)

	cmd = api + "/" + String(cn2)
        doc = ArangoDB.get(cmd)

	doc.code.should eq(200)
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['name'].should eq(cn2)
	doc.parsed_response['status'].should eq(3)

	ArangoDB.drop_collection(cn)
	ArangoDB.drop_collection(cn2)
      end

      it "rename a collection by identifier with conflict" do
	cn = "UnitTestsCollectionBasics"
	ArangoDB.drop_collection(cn)
	cid = ArangoDB.create_collection(cn)

	cn2 = "UnitTestsCollectionBasics2"
	ArangoDB.drop_collection(cn2)
	cid2 = ArangoDB.create_collection(cn2)

	body = "{ \"Hallo\" : \"World\" }"

	cmd = "/_api/document?collection=#{cid}"

	for i in ( 1 .. 10 )
	  doc = ArangoDB.post(cmd, :body => body)
	end

	ArangoDB.size_collection(cid).should eq(10)
	ArangoDB.size_collection(cn).should eq(10)

	cmd = "/_api/document?collection=#{cid2}"

	for i in ( 1 .. 20 )
	  doc = ArangoDB.post(cmd, :body => body)
	end

	ArangoDB.size_collection(cid2).should eq(20)
	ArangoDB.size_collection(cn2).should eq(20)

	body = "{ \"name\" : \"#{cn2}\" }"
	cmd = api + "/" + String(cid) + "/rename"
        doc = ArangoDB.log_put("#{prefix}-identifier-rename-conflict", cmd, :body => body)

	doc.code.should eq(400)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['code'].should eq(400)
	doc.parsed_response['errorNum'].should eq(1207)

	ArangoDB.size_collection(cid).should eq(10)
	ArangoDB.size_collection(cn).should eq(10)

	ArangoDB.size_collection(cid2).should eq(20)
	ArangoDB.size_collection(cn2).should eq(20)

	ArangoDB.drop_collection(cn)
	ArangoDB.drop_collection(cn2)
      end

      it "rename a new-born collection by identifier" do
	cn = "UnitTestsCollectionBasics"
	ArangoDB.drop_collection(cn)
	cid = ArangoDB.create_collection(cn)

	cn2 = "UnitTestsCollectionBasics2"
	ArangoDB.drop_collection(cn2)

	body = "{ \"name\" : \"#{cn2}\" }"
	cmd = api + "/" + String(cid) + "/rename"
        doc = ArangoDB.log_put("#{prefix}-identifier-rename-new-born", cmd, :body => body)

	doc.code.should eq(200)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.parsed_response['id'].should eq(cid)
	doc.parsed_response['name'].should eq(cn2)
	doc.parsed_response['status'].should eq(3)

	cmd = api + "/" + String(cn)
        doc = ArangoDB.get(cmd)

	doc.code.should eq(404)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(1203)

	cmd = api + "/" + String(cn2)
        doc = ArangoDB.get(cmd)

	doc.code.should eq(200)
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['name'].should eq(cn2)
	doc.parsed_response['status'].should eq(3)

	ArangoDB.drop_collection(cn)
	ArangoDB.drop_collection(cn2)
      end

      it "rename a new-born collection by identifier with conflict" do
	cn = "UnitTestsCollectionBasics"
	ArangoDB.drop_collection(cn)
	cid = ArangoDB.create_collection(cn)

	cn2 = "UnitTestsCollectionBasics2"
	ArangoDB.drop_collection(cn2)
	cid2 = ArangoDB.create_collection(cn2)

	cmd = "/_api/document?collection=#{cid2}"

	body = "{ \"name\" : \"#{cn2}\" }"
	cmd = api + "/" + String(cid) + "/rename"
        doc = ArangoDB.log_put("#{prefix}-identifier-rename-conflict", cmd, :body => body)

	doc.code.should eq(400)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['code'].should eq(400)
	doc.parsed_response['errorNum'].should eq(1207)

	ArangoDB.drop_collection(cn)
	ArangoDB.drop_collection(cn2)
      end
    end

################################################################################
## properties of a collection
################################################################################

    context "properties:" do
      it "changing the properties of a collection by identifier" do
	cn = "UnitTestsCollectionBasics"
	ArangoDB.drop_collection(cn)
	cid = ArangoDB.create_collection(cn)

	cmd = "/_api/document?collection=#{cid}"
	body = "{ \"Hallo\" : \"World\" }"

	for i in ( 1 .. 10 )
	  doc = ArangoDB.post(cmd, :body => body)
	end

	ArangoDB.size_collection(cid).should eq(10)
	ArangoDB.size_collection(cn).should eq(10)

	cmd = api + "/" + String(cid) + "/properties"
	body = "{ \"waitForSync\" : true }"
        doc = ArangoDB.log_put("#{prefix}-identifier-properties-sync", cmd, :body => body)

	doc.code.should eq(200)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.parsed_response['id'].should eq(cid)
	doc.parsed_response['name'].should eq(cn)
	doc.parsed_response['status'].should eq(3)
	doc.parsed_response['waitForSync'].should eq(true)

	cmd = api + "/" + String(cid) + "/properties"
	body = "{ \"waitForSync\" : false }"
        doc = ArangoDB.log_put("#{prefix}-identifier-properties-no-sync", cmd, :body => body)

	doc.code.should eq(200)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.parsed_response['id'].should eq(cid)
	doc.parsed_response['name'].should eq(cn)
	doc.parsed_response['status'].should eq(3)
	doc.parsed_response['waitForSync'].should eq(false)

	ArangoDB.drop_collection(cn)
      end
    end

  end
end
