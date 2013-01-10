# coding: utf-8

require 'rspec'
require './arangodb.rb'

describe ArangoDB do
  prefix = "api-http"

  context "dealing with HTTP methods:" do

################################################################################
## checking HTTP HEAD responses
################################################################################

    context "head requests" do
      before do
      end

      after do
      end
      
      it "checks whether HEAD returns a body on 2xx" do
	cmd = "/_api/version"
        doc = ArangoDB.log_head("#{prefix}-head-supported-method", cmd)

	doc.code.should eq(200)
        doc.response.body.should be_nil
      end
      
      it "checks whether HEAD returns a body on 3xx" do
	cmd = "/_api/collection"
        doc = ArangoDB.log_head("#{prefix}-head-unsupported-method1", cmd)

	doc.code.should eq(405)
        doc.response.body.should be_nil
      end

      it "checks whether HEAD returns a body on 4xx" do
	cmd = "/_api/cursor"
        doc = ArangoDB.log_head("#{prefix}-head-unsupported-method2", cmd)

	doc.code.should eq(405)
        doc.response.body.should be_nil
      end
      
      it "checks whether HEAD returns a body on 5xx" do
	cmd = "/_api/non-existing-method"
        doc = ArangoDB.log_head("#{prefix}-head-non-existing-method", cmd)

	doc.code.should eq(501)
        doc.response.body.should be_nil
      end

      it "checks whether HEAD returns a body on an existing document" do
        cn = "UnitTestsCollectionHttp"
	ArangoDB.drop_collection(cn)

        # create collection with one document
	@cid = ArangoDB.create_collection(cn)
	
        cmd = "/_api/document?collection=#{cn}"
	body = "{ \"Hello\" : \"World\" }"
	doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)

	did = doc.parsed_response['_id']
	did.should be_kind_of(String)

        # run a HTTP HEAD query on the existing document
	cmd = "/_api/document/" + did
        doc = ArangoDB.log_head("#{prefix}-head-check-document", cmd)

	doc.code.should eq(200)
        doc.response.body.should be_nil

        # run a HTTP HEAD query on the existing document, with wrong precondition
	cmd = "/_api/document/" + did
        doc = ArangoDB.log_head("#{prefix}-head-check-documentq", cmd, :header => { :"if-match" => "1" })

	doc.code.should eq(200)
        doc.response.body.should be_nil

	ArangoDB.drop_collection(cn)
      end
    end

################################################################################
## checking HTTP GET responses
################################################################################

    context "get requests" do
      it "checks a non-existing URL" do
	cmd = "/xxxx/yyyy"
        doc = ArangoDB.log_get("#{prefix}-get-non-existing-url", cmd)

	doc.code.should eq(501)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['code'].should eq(501)
      end

      it "checks whether GET returns a body" do
	cmd = "/_api/non-existing-method"
        doc = ArangoDB.log_get("#{prefix}-get-non-existing-method", cmd)

	doc.code.should eq(501)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(9)
	doc.parsed_response['code'].should eq(501)
      end

      it "checks whether GET returns a body" do
	cmd = "/_api/non-allowed-method"
        doc = ArangoDB.log_get("#{prefix}-get-non-allowed-method", cmd)

	doc.code.should eq(501)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(9)
	doc.parsed_response['code'].should eq(501)
      end
    end

  end

end
