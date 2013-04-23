# coding: utf-8

require 'rspec'
require './arangodb.rb'

describe ArangoDB do
  api = "/_admin/"
  prefix = "admin-statistics"
  context "calculating statistics:" do


################################################################################
## check statistics-description availability
###############################################################################
  
    it "testing statistics-description correct cmd" do 
      cmd = "/_admin/statistics-description"
      doc = ArangoDB.log_get("#{prefix}", cmd) 
  
      doc.code.should eq(200)
    end

################################################################################
## check statistics-description for wrong user interaction
###############################################################################

    it "testing statistics-description wrong cmd" do 
      cmd = "/_admin/statistics-description/asd123"
      doc = ArangoDB.log_get("#{prefix}", cmd) 
   
      doc.code.should eq(404)
      doc.parsed_response['error'].should eq(true)
      doc.parsed_response['errorNum'].should eq(404)
    end

################################################################################
## check statistics availability
###############################################################################

    it "testing statistics correct cmd" do 
      cmd = "/_admin/statistics"
      doc = ArangoDB.log_get("#{prefix}", cmd) 
  
      doc.code.should eq(200)
    end

################################################################################
## check statistics for wrong user interaction
###############################################################################

    it "testing statistics wrong cmd" do 
      cmd = "/_admin/statistics/asd123"
      doc = ArangoDB.log_get("#{prefix}", cmd) 
   
      doc.code.should eq(404)
      doc.parsed_response['error'].should eq(true)
      doc.parsed_response['errorNum'].should eq(404)
    end


  end
end

