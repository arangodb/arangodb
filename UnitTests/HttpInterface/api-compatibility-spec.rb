# coding: utf-8

require 'rspec'
require 'arangodb.rb'

describe ArangoDB do

################################################################################
## general tests
################################################################################
  
  context "checking compatibility features:" do
    it "tests the compatibility value when no header is set" do
      doc = ArangoDB.get("/_admin/echo", :headers => { })

      doc.code.should eq(200)
      compatibility = doc.parsed_response['compatibility']
      compatibility.should be_kind_of(Integer)
      compatibility.should eq(20500)
    end
    
    it "tests the compatibility value when a broken header is set" do
      versions = [ "1", "1.", "-1.3", "-1.3.", "x.4", "xx", "", " ", ".", "foobar", "foobar1.3", "xx1.4" ]

      versions.each do|value|
        doc = ArangoDB.get("/_admin/echo", :headers => { "x-arango-version" => value })

        doc.code.should eq(200)
        compatibility = doc.parsed_response['compatibility']
        compatibility.should be_kind_of(Integer)
        compatibility.should eq(20500)
      end
    end
    
    it "tests the compatibility value when a valid header is set" do
      versions = [ "1.3.0", "1.3", "1.3-devel", "1.3.1", "1.3.99", "10300", "10303" ]
      
      versions.each do|value|
        doc = ArangoDB.get("/_admin/echo", :headers => { "x-arango-version" => value })

        doc.code.should eq(200)
        compatibility = doc.parsed_response['compatibility']
        compatibility.should be_kind_of(Integer)
        compatibility.should eq(10300)
      end
    end
    
    it "tests the compatibility value when a valid header is set" do
      versions = [ "1.4.0", "1.4.1", "1.4.2", "1.4.0-devel", "1.4.0-beta2", "   1.4", "1.4  ", " 1.4.0", "  1.4.0  ", "10400", "10401", "10499" ]

      versions.each do|value|
        doc = ArangoDB.get("/_admin/echo", :headers => { "x-arango-version" => value })

        doc.code.should eq(200)
        compatibility = doc.parsed_response['compatibility']
        compatibility.should be_kind_of(Integer)
        compatibility.should eq(10400)
      end
    end
    
    it "tests the compatibility value when a valid header is set" do
      versions = [ "1.5.0", "1.5.1", "1.5.2", "1.5.0-devel", "1.5.0-beta2", "   1.5", "1.5  ", " 1.5.0", "  1.5.0  ", "10500", "10501", "10599" ]

      versions.each do|value|
        doc = ArangoDB.get("/_admin/echo", :headers => { "x-arango-version" => value })

        doc.code.should eq(200)
        compatibility = doc.parsed_response['compatibility']
        compatibility.should be_kind_of(Integer)
        compatibility.should eq(10500)
      end
    end
    
    it "tests the compatibility value when a valid header is set" do
      versions = [ "2.0.0", "2.0.0-devel", "2.0.0-alpha", "2.0", "   2.0", "2.0  ", " 2.0.0", "  2.0.0  ", "20000", "20000 ", "20099" ]

      versions.each do|value|
        doc = ArangoDB.get("/_admin/echo", :headers => { "x-arango-version" => value })

        doc.code.should eq(200)
        compatibility = doc.parsed_response['compatibility']
        compatibility.should be_kind_of(Integer)
        compatibility.should eq(20000)
      end
    end
    
    it "tests the compatibility value when a valid header is set" do
      versions = [ "2.1.0", "2.1.0-devel", "2.1.0-alpha", "2.1", "   2.1", "2.1  ", " 2.1.0", "  2.1.0  ", "20100", "20100 ", "20199" ]

      versions.each do|value|
        doc = ArangoDB.get("/_admin/echo", :headers => { "x-arango-version" => value })

        doc.code.should eq(200)
        compatibility = doc.parsed_response['compatibility']
        compatibility.should be_kind_of(Integer)
        compatibility.should eq(20100)
      end
    end

    it "tests the compatibility value when a valid header is set" do
      versions = [ "2.2.0", "2.2.0-devel", "2.2.0-alpha", "2.2", "   2.2", "2.2  ", " 2.2.0", "  2.2.0  ", "20200", "20200 ", "20299" ]

      versions.each do|value|
        doc = ArangoDB.get("/_admin/echo", :headers => { "x-arango-version" => value })

        doc.code.should eq(200)
        compatibility = doc.parsed_response['compatibility']
        compatibility.should be_kind_of(Integer)
        compatibility.should eq(20200)
      end
    end

    it "tests the compatibility value when a too low version is set" do
      versions = [ "0.0", "0.1", "0.2", "0.9", "1.0", "1.1", "1.2" ]

      versions.each do|value|
        doc = ArangoDB.get("/_admin/echo", :headers => { "x-arango-version" => value })

        doc.code.should eq(200)
        compatibility = doc.parsed_response['compatibility']
        compatibility.should be_kind_of(Integer)
        compatibility.should eq(10300)
      end
    end
    
    it "tests the compatibility value when a too high version is set" do
      doc = ArangoDB.get("/_admin/echo", :headers => { "x-arango-version" => "2.4" })

      doc.code.should eq(200)
      compatibility = doc.parsed_response['compatibility']
      compatibility.should be_kind_of(Integer)
      compatibility.should eq(20400)
    end

  end

end
