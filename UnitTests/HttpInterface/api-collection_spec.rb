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
    end

################################################################################
## reading of an collection
################################################################################

    context "reading:" do
      before do
	@cn = "UnitTestsCollectionBasics"
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
	doc.parsed_response['alive']['number'].should be_kind_of(Integer)
	doc.parsed_response['count'].should eq(doc.parsed_response['alive']['count'])
	doc.headers['location'].should eq(api + "/" + String(@cid) + "/figures")

	AvocadoDB.log(:method => :get, :url => cmd, :result => doc, :output => "#{prefix}-get-collection-figures")
      end

    end
  end
end


