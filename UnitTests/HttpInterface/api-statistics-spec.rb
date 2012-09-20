# coding: utf-8

require 'rspec'
require './arangodb.rb'

describe ArangoDB do
  api = "/_admin/"
  prefix = "admin-statistics"
  context "calculating statistics:" do


################################################################################
## check requests-statistics availability
###############################################################################
  
    it "testing requests-statistics correct cmd" do 
      cmd = "/_admin/request-statistics"
      doc = ArangoDB.log_post("#{prefix}", cmd) 
  
      doc.code.should eq(200)
    end

################################################################################
## check requests-statistics for wrong user interaction
###############################################################################

    it "testing requests-statistics wrong cmd" do 
      cmd = "/_admin/request-statistics/asd123"
      doc = ArangoDB.log_post("#{prefix}", cmd) 
   
      doc.code.should eq(501)
      doc.parsed_response['error'].should eq(true)
      doc.parsed_response['errorNum'].should eq(9)
    end

################################################################################
## check requests-statistics availability
###############################################################################

    it "testing connection-statistics correct cmd" do 
      cmd = "/_admin/connection-statistics"
      doc = ArangoDB.log_post("#{prefix}", cmd) 
  
      doc.code.should eq(200)
    end

################################################################################
## check requests-connection for wrong user interaction
###############################################################################

    it "testing connection-statistics wrong cmd" do 
      cmd = "/_admin/connection-statistics/asd123"
      doc = ArangoDB.log_post("#{prefix}", cmd) 
   
      doc.code.should eq(501)
      doc.parsed_response['error'].should eq(true)
      doc.parsed_response['errorNum'].should eq(9)
    end


  end
end

