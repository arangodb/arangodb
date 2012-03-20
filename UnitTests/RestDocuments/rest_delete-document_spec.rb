require 'rspec'
require './avocadodb.rb'

describe AvocadoDB do
  context "delete a document in a collection" do

################################################################################
## error handling
################################################################################

    context "error handling" do
      it "returns an error if document handle is missing" do
	cmd = "/document"
        doc = AvocadoDB.delete(cmd)

	doc.code.should eq(400)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(1202)
	doc.parsed_response['code'].should eq(400)
	doc.headers['content-type'].should eq("application/json; charset=utf-8")

	AvocadoDB.log(:method => :post, :url => cmd, :result => doc, :output => "rest_delete-document-missing-handle")
      end
    end

  end
end

