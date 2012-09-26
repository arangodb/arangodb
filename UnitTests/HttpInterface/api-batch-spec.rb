# coding: utf-8

require 'rspec'
require './arangodb.rb'
require './arangomultipartbody.rb'

describe ArangoDB do
  prefix = "api-batch"

  context "dealing with HTTP batch requests:" do

################################################################################
## checking disallowed methods
################################################################################
  
    context "using disallowed methods:" do

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

################################################################################
## checking simple batches
################################################################################
  
    context "checking simple requests:" do

      it "checks whether POST is allowed on /_api/batch" do
        cmd = "/_api/batch"

        multipart = ArangoMultipartBody.new()

        multipart.addPart("GET", "/_api/version", { }, "")
#        multipart.addPart("GET", "/_api/version", { }, "")
        doc = ArangoDB.log_post("#{prefix}-post-version1", cmd, :body => multipart.to_s, :format => :plain, :headers => { "Content-Type" => "multipart/form-data; boundary=" + multipart.getBoundary })

        doc.code.should eq(200)
#        doc.parsed_response['error'].should eq(false) 
#        doc.parsed_response['code'].should eq(200)
      end
    
    end
      
  end

end
