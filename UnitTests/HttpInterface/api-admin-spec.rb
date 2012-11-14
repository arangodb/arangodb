# coding: utf-8

require 'rspec'
require './arangodb.rb'

describe ArangoDB do

  context "dealing with the admin interface:" do

################################################################################
## check whether admin interface is accessible
################################################################################
  
    it "checks whether the admin interface is available at /_admin/html" do
      cmd = "/_admin/html/index.html"
      doc = ArangoDB.log_get("admin-interface-get", cmd, :format => :plain)

      # check response code
      doc.code.should eq(200)
      # check whether HTML result contains expected title
      doc.response.body.should include("ArangoDB - WebAdmin")

    end
    
  end

end
