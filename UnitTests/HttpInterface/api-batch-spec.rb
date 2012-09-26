# coding: utf-8

require 'rspec'
require './arangodb.rb'

describe ArangoDB do
  prefix = "api-batch"

  context "dealing with HTTP batch requests:" do

################################################################################
## checking HTTP HEAD responses
################################################################################

    it "checks whether GET is allowed on /_api/batch" do
      cmd = "/_api/batch"
      doc = ArangoDB.log_get("#{prefix}-get", cmd)

      doc.code.should eq(405)
      doc.parsed_response['error'].should eq(true) 
      doc.parsed_response['code'].should eq(405)
    end
    
    it "checks whether HEAD is allowed on /_api/batch" do
      cmd = "/_api/batch"
      doc = ArangoDB.log_head("#{prefix}-get", cmd)

      doc.code.should eq(405)
      doc.response.body.should be_nil
    end
    
    it "checks whether DELETE is allowed on /_api/batch" do
      cmd = "/_api/batch"
      doc = ArangoDB.log_delete("#{prefix}-delete", cmd)

      doc.code.should eq(405)
      doc.parsed_response['error'].should eq(true) 
      doc.parsed_response['code'].should eq(405)
    end
    
    it "checks whether PATCH is allowed on /_api/batch" do
      cmd = "/_api/batch"
      doc = ArangoDB.log_patch("#{prefix}-patch", cmd)

      doc.code.should eq(405)
      doc.parsed_response['error'].should eq(true) 
      doc.parsed_response['code'].should eq(405)
    end
      
  end

end
