require 'rspec'
require './avocadodb.rb'

describe AvocadoDB do
  api = "/_api/collection"
  prefix = "api-collection"

  context "dealing with collections:" do

################################################################################
## reading all collection
################################################################################

    context "all collections:" do
      before do
	for cn in ["units", "employees", "locations" ] do
	  AvocadoDB.drop_collection(cn)
	  @cid = AvocadoDB.create_collection(cn)
	end
      end

      after do
	for cn in ["units", "employees", "locations" ] do
	  AvocadoDB.drop_collection(cn)
	end
      end

      it "returns all collections" do
	cmd = api
        doc = AvocadoDB.log_get("#{prefix}-all-collections", cmd)

	doc.code.should eq(200)
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.headers['content-type'].should eq("application/json")

	collections = doc.parsed_response['collections']
	names = doc.parsed_response['names']

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
        doc = AvocadoDB.log_get("#{prefix}-bad-handle", cmd)

	doc.code.should eq(404)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(1203)
	doc.parsed_response['code'].should eq(404)
	doc.headers['content-type'].should eq("application/json")
      end

      it "creating a collection without name" do
	cmd = api
        doc = AvocadoDB.log_post("#{prefix}-create-missing-name", cmd)

	doc.code.should eq(400)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['code'].should eq(400)
	doc.parsed_response['errorNum'].should eq(1208)
	doc.headers['content-type'].should eq("application/json")
      end

      it "creating a collection with an illegal name" do
	cmd = api
	body = "{ \"name\" : \"1\" }"
        doc = AvocadoDB.log_post("#{prefix}-create-illegal-name", cmd, :body => body)

	doc.code.should eq(400)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['code'].should eq(400)
	doc.parsed_response['errorNum'].should eq(1208)
	doc.headers['content-type'].should eq("application/json")
      end

      it "creating a collection with a duplicate name" do
	cn = "UnitTestsCollectionBasics"
	cid = AvocadoDB.create_collection(cn)

	cmd = api
	body = "{ \"name\" : \"#{cn}\" }"
        doc = AvocadoDB.log_post("#{prefix}-create-illegal-name", cmd, :body => body)

	doc.code.should eq(400)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['code'].should eq(400)
	doc.parsed_response['errorNum'].should eq(1207)
	doc.headers['content-type'].should eq("application/json")
      end

      it "creating a collection with an illegal body" do
	cmd = api
	body = "{ name : world }"
        doc = AvocadoDB.log_post("#{prefix}-create-illegal-body", cmd, :body => body)

	doc.code.should eq(400)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['code'].should eq(400)
	doc.parsed_response['errorNum'].should eq(600)
	doc.parsed_response['errorMessage'].should eq("SyntaxError: Unexpected token n")
	doc.headers['content-type'].should eq("application/json")
      end

      it "creating a collection with a null body" do
	cmd = api
	body = "null"
        doc = AvocadoDB.log_post("#{prefix}-create-null-body", cmd, :body => body)

	doc.code.should eq(400)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['code'].should eq(400)
	doc.parsed_response['errorNum'].should eq(1208)
	doc.headers['content-type'].should eq("application/json")
      end
    end

################################################################################
## reading a collection
################################################################################

    context "reading:" do
      before do
	@cn = "UnitTestsCollectionBasics"
	AvocadoDB.drop_collection(@cn)
	@cid = AvocadoDB.create_collection(@cn)
      end

      after do
	AvocadoDB.drop_collection(@cn)
      end

      it "finds the collection by identifier" do
	cmd = api + "/" + String(@cid)
        doc = AvocadoDB.log_get("#{prefix}-get-collection-identifier", cmd)

	doc.code.should eq(200)
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.headers['content-type'].should eq("application/json")

	doc.parsed_response['id'].should eq(@cid)
	doc.parsed_response['name'].should eq(@cn)
	doc.parsed_response['status'].should be_kind_of(Integer)
	doc.headers['location'].should eq(api + "/" + String(@cid))
      end

      it "finds the collection by name" do
	cmd = api + "/" + String(@cn)
        doc = AvocadoDB.log_get("#{prefix}-get-collection-name", cmd)

	doc.code.should eq(200)
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.headers['content-type'].should eq("application/json")

	doc.parsed_response['id'].should eq(@cid)
	doc.parsed_response['name'].should eq(@cn)
	doc.parsed_response['status'].should be_kind_of(Integer)
	doc.headers['location'].should eq(api + "/" + String(@cid))
      end

      it "checks the size of a collection" do
	cmd = api + "/" + String(@cid) + "/count"
        doc = AvocadoDB.log_get("#{prefix}-get-collection-count", cmd)

	doc.code.should eq(200)
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.headers['content-type'].should eq("application/json")

	doc.parsed_response['id'].should eq(@cid)
	doc.parsed_response['name'].should eq(@cn)
	doc.parsed_response['status'].should eq(3)
	doc.parsed_response['count'].should be_kind_of(Integer)
	doc.headers['location'].should eq(api + "/" + String(@cid) + "/count")
      end

      it "extracting the figures for a collection" do
	cmd = api + "/" + String(@cid) + "/figures"
        doc = AvocadoDB.log_get("#{prefix}-get-collection-figures", cmd)

	doc.code.should eq(200)
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.headers['content-type'].should eq("application/json")

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
	cid = AvocadoDB.create_collection(@cn)
	cmd = api + "/" + String(cid)
        doc = AvocadoDB.log_delete("#{prefix}-delete-collection-identifier", cmd)

	doc.code.should eq(200)
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.parsed_response['id'].should eq(cid)
	doc.headers['content-type'].should eq("application/json")

	cmd = api + "/" + String(cid)
	doc = AvocadoDB.get(cmd)

	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['code'].should eq(404)
      end

      it "delete an existing collection by name" do
	cid = AvocadoDB.create_collection(@cn)
	cmd = api + "/" + @cn
        doc = AvocadoDB.log_delete("#{prefix}-delete-collection-name", cmd)

	doc.code.should eq(200)
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.parsed_response['id'].should eq(cid)
	doc.headers['content-type'].should eq("application/json")

	cmd = api + "/" + String(cid)
	doc = AvocadoDB.get(cmd)

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
        doc = AvocadoDB.log_post("#{prefix}-create-collection", cmd, :body => body)

	doc.code.should eq(200)
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.parsed_response['id'].should be_kind_of(Integer)
	doc.parsed_response['name'].should eq(@cn)
	doc.parsed_response['waitForSync'].should == false
	doc.headers['content-type'].should eq("application/json")

	cmd = api + "/" + String(@cn) + "/figures"
        doc = AvocadoDB.get(cmd)

	puts doc

	doc.parsed_response['waitForSync'].should == false

	AvocadoDB.drop_collection(@cn)
      end

      it "create a collection, sync" do
	cmd = api
	body = "{ \"name\" : \"#{@cn}\", \"waitForSync\" : true }"
        doc = AvocadoDB.log_post("#{prefix}-create-collection-sync", cmd, :body => body)

	doc.code.should eq(200)
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.parsed_response['id'].should be_kind_of(Integer)
	doc.parsed_response['name'].should eq(@cn)
	doc.parsed_response['waitForSync'].should == true
	doc.headers['content-type'].should eq("application/json")

	cmd = api + "/" + String(@cn) + "/figures"
        doc = AvocadoDB.get(cmd)

	puts doc

	doc.parsed_response['waitForSync'].should == true

	AvocadoDB.drop_collection(@cn)
      end
    end

################################################################################
## updating a collection
################################################################################

    context "updating:" do
      before do
	@cn = "UnitTestsCollectionBasics"
      end

      it "load a collection by identifier" do
	AvocadoDB.drop_collection(@cn)
	cid = AvocadoDB.create_collection(@cn)

	cmd = api + "/" + String(cid) + "/load"
        doc = AvocadoDB.log_put("#{prefix}-identifier-load", cmd)

	doc.code.should eq(200)
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.parsed_response['id'].should eq(cid)
	doc.parsed_response['name'].should eq(@cn)
	doc.parsed_response['status'].should eq(3)
	doc.parsed_response['count'].should be_kind_of(Integer)
	doc.headers['content-type'].should eq("application/json")

	AvocadoDB.drop_collection(@cn)
      end

      it "load a collection by name" do
	AvocadoDB.drop_collection(@cn)
	cid = AvocadoDB.create_collection(@cn)

	cmd = api + "/" + @cn + "/load"
        doc = AvocadoDB.log_put("#{prefix}-name-load", cmd)

	doc.code.should eq(200)
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.parsed_response['id'].should eq(cid)
	doc.parsed_response['name'].should eq(@cn)
	doc.parsed_response['status'].should eq(3)
	doc.parsed_response['count'].should be_kind_of(Integer)
	doc.headers['content-type'].should eq("application/json")

	AvocadoDB.drop_collection(@cn)
      end

      it "unload a collection by identifier" do
	AvocadoDB.drop_collection(@cn)
	cid = AvocadoDB.create_collection(@cn)

	cmd = api + "/" + String(cid) + "/unload"
        doc = AvocadoDB.log_put("#{prefix}-identifier-unload", cmd)

	doc.code.should eq(200)
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.parsed_response['id'].should eq(cid)
	doc.parsed_response['name'].should eq(@cn)
	doc.parsed_response['status'].should eq(4)
	doc.headers['content-type'].should eq("application/json")

	AvocadoDB.drop_collection(@cn)
      end

      it "unload a collection by name" do
	AvocadoDB.drop_collection(@cn)
	cid = AvocadoDB.create_collection(@cn)

	cmd = api + "/" + @cn + "/unload"
        doc = AvocadoDB.log_put("#{prefix}-name-unload", cmd)

	doc.code.should eq(200)
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.parsed_response['id'].should eq(cid)
	doc.parsed_response['name'].should eq(@cn)
	doc.parsed_response['status'].should eq(4)
	doc.headers['content-type'].should eq("application/json")

	AvocadoDB.drop_collection(@cn)
      end
    end

  end
end
