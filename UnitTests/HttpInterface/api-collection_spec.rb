require 'rspec'
require './avocadodb.rb'

describe AvocadoDB do
  api = "/_api/collection"
  prefix = "api-collection"

  context "dealing with collections:" do

################################################################################
## error handling
################################################################################

    context "error handling:" do
      it "returns an error if collection identifier is missing" do
	cmd = api
        doc = AvocadoDB.get(cmd)

	doc.code.should eq(400)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(400)
	doc.parsed_response['code'].should eq(400)
	doc.headers['content-type'].should eq("application/json")

	AvocadoDB.log(:method => :get, :url => cmd, :result => doc, :output => "#{prefix}-bad-handle")
      end

      it "returns an error if collection identifier is unknown" do
	cmd = api + "/123456"
        doc = AvocadoDB.get(cmd)

	doc.code.should eq(404)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(1203)
	doc.parsed_response['code'].should eq(404)
	doc.headers['content-type'].should eq("application/json")

	AvocadoDB.log(:method => :get, :url => cmd, :result => doc, :output => "#{prefix}-bad-handle")
      end

      it "creating a collection without name" do
	cmd = api
        doc = AvocadoDB.post(cmd)

	doc.code.should eq(400)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['code'].should eq(400)
	doc.parsed_response['errorNum'].should eq(1208)
	doc.headers['content-type'].should eq("application/json")

	AvocadoDB.log(:method => :post, :url => cmd, :result => doc, :output => "#{prefix}-create-missing-name")
      end

      it "creating a collection with an illegal name" do
	cmd = api
	body = "{ \"name\" : \"1\" }"
        doc = AvocadoDB.post(cmd, :body => body)

	doc.code.should eq(400)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['code'].should eq(400)
	doc.parsed_response['errorNum'].should eq(1208)
	doc.headers['content-type'].should eq("application/json")

	AvocadoDB.log(:method => :post, :url => cmd, :body => body, :result => doc, :output => "#{prefix}-create-illegal-name")
      end

      it "creating a collection with a duplicate name" do
	cn = "UnitTestsCollectionBasics"
	cid = AvocadoDB.create_collection(cn)

	cmd = api
	body = "{ \"name\" : \"#{cn}\" }"
        doc = AvocadoDB.post(cmd, :body => body)

	doc.code.should eq(400)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['code'].should eq(400)
	doc.parsed_response['errorNum'].should eq(1207)
	doc.headers['content-type'].should eq("application/json")

	AvocadoDB.log(:method => :post, :url => cmd, :body => body, :result => doc, :output => "#{prefix}-create-illegal-name")
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
        doc = AvocadoDB.get(cmd)

	doc.code.should eq(200)
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.headers['content-type'].should eq("application/json")

	doc.parsed_response['id'].should eq(@cid)
	doc.parsed_response['name'].should eq(@cn)
	doc.parsed_response['status'].should be_kind_of(Integer)
	doc.parsed_response['waitForSync'].should == true
	doc.headers['location'].should eq(api + "/" + String(@cid))

	AvocadoDB.log(:method => :get, :url => cmd, :result => doc, :output => "#{prefix}-get-collection-identifier")
      end

      it "finds the collection by name" do
	cmd = api + "/" + String(@cn)
        doc = AvocadoDB.get(cmd)

	doc.code.should eq(200)
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.headers['content-type'].should eq("application/json")

	doc.parsed_response['id'].should eq(@cid)
	doc.parsed_response['name'].should eq(@cn)
	doc.parsed_response['status'].should be_kind_of(Integer)
	doc.parsed_response['waitForSync'].should == true
	doc.headers['location'].should eq(api + "/" + String(@cid))

	AvocadoDB.log(:method => :get, :url => cmd, :result => doc, :output => "#{prefix}-get-collection-name")
      end

      it "checks the size of a collection" do
	cmd = api + "/" + String(@cid) + "/count"
        doc = AvocadoDB.get(cmd)

	doc.code.should eq(200)
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.headers['content-type'].should eq("application/json")

	doc.parsed_response['id'].should eq(@cid)
	doc.parsed_response['name'].should eq(@cn)
	doc.parsed_response['status'].should eq(3)
	doc.parsed_response['waitForSync'].should == true
	doc.parsed_response['count'].should be_kind_of(Integer)
	doc.headers['location'].should eq(api + "/" + String(@cid) + "/count")

	AvocadoDB.log(:method => :get, :url => cmd, :result => doc, :output => "#{prefix}-get-collection-count")
      end

      it "extracting the figures for a collection" do
	cmd = api + "/" + String(@cid) + "/figures"
        doc = AvocadoDB.get(cmd)

	doc.code.should eq(200)
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.headers['content-type'].should eq("application/json")

	doc.parsed_response['id'].should eq(@cid)
	doc.parsed_response['name'].should eq(@cn)
	doc.parsed_response['status'].should eq(3)
	doc.parsed_response['count'].should be_kind_of(Integer)
	doc.parsed_response['waitForSync'].should == true
	doc.parsed_response['figures']['alive']['count'].should be_kind_of(Integer)
	doc.parsed_response['count'].should eq(doc.parsed_response['figures']['alive']['count'])
	doc.parsed_response['journalSize'].should be_kind_of(Integer)
	doc.headers['location'].should eq(api + "/" + String(@cid) + "/figures")

	AvocadoDB.log(:method => :get, :url => cmd, :result => doc, :output => "#{prefix}-get-collection-figures")
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
        doc = AvocadoDB.delete(cmd)

	doc.code.should eq(200)
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.parsed_response['id'].should eq(cid)
	doc.headers['content-type'].should eq("application/json")

	AvocadoDB.log(:method => :delete, :url => cmd, :result => doc, :output => "#{prefix}-delete-collection-identifier")

	cmd = api + "/" + String(cid)
	doc = AvocadoDB.get(cmd)

	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['code'].should eq(404)
      end

      it "delete an existing collection by name" do
	cid = AvocadoDB.create_collection(@cn)
	cmd = api + "/" + @cn
        doc = AvocadoDB.delete(cmd)

	doc.code.should eq(200)
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.parsed_response['id'].should eq(cid)
	doc.headers['content-type'].should eq("application/json")

	AvocadoDB.log(:method => :delete, :url => cmd, :result => doc, :output => "#{prefix}-delete-collection-name")

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
        doc = AvocadoDB.post(cmd, :body => body)

	doc.code.should eq(200)
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.parsed_response['id'].should be_kind_of(Integer)
	doc.parsed_response['name'].should eq(@cn)
	doc.parsed_response['waitForSync'].should == false
	doc.headers['content-type'].should eq("application/json")

	AvocadoDB.log(:method => :post, :url => cmd, :body => body, :result => doc, :output => "#{prefix}-create-collection")

	AvocadoDB.drop_collection(@cn)
      end

      it "create a collection, sync" do
	cmd = api
	body = "{ \"name\" : \"#{@cn}\", \"waitForSync\" : true }"
        doc = AvocadoDB.post(cmd, :body => body)

	doc.code.should eq(200)
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.parsed_response['id'].should be_kind_of(Integer)
	doc.parsed_response['name'].should eq(@cn)
	doc.parsed_response['waitForSync'].should == true
	doc.headers['content-type'].should eq("application/json")

	AvocadoDB.log(:method => :post, :url => cmd, :body => body, :result => doc, :output => "#{prefix}-create-collection-sync")

	AvocadoDB.drop_collection(@cn)
      end
    end
  end
end


