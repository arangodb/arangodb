# coding: utf-8

require 'rspec'
require './arangodb.rb'

describe ArangoDB do
  api = "/_api/index"
  prefix = "api-index-bitarray"


  context "creating bitarray index:" do

  
    ## .............................................................................
    ## testing that the fields and values are in pairs
    ## .............................................................................
    
    context "testing attribute value pairs:" do
    
      before do
        @cn = "UnitTestsCollectionIndexes"
        ArangoDB.drop_collection(@cn)
        @cid = ArangoDB.create_collection(@cn)
      end
      
      after do
        ArangoDB.drop_collection(@cn)
      end
      
      it "creation failure due to inconsistent attribute value pairs" do
      
        ## .........................................................................
        # try to create the index
        ## .........................................................................
        
        cmd = "/_api/index?collection=#{@cn}"
        body = "{ \"type\" : \"bitarray\", \"unique\" : false, \"fields\" : [  \"a\", [0,1,2], \"b\"  ] }"
        doc = ArangoDB.log_post("#{prefix}-fail", cmd, :body => body)
        doc.code.should eq(400)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(10)
      end      
      
      it "creation failure due to duplicate attributes" do
      
        ## .........................................................................
        # try to create the index
        ## .........................................................................
        
        cmd = "/_api/index?collection=#{@cn}"
        body = "{ \"type\" : \"bitarray\", \"unique\" : false, \"fields\" : [  \"a\", [0,1,2], \"a\", [\"a\",\"b\",\"c\"]  ] }"
        doc = ArangoDB.log_post("#{prefix}-fail", cmd, :body => body)        
        doc.code.should eq(400)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(3415)
      end  

      it "creation failure due to duplicate values" do
      
        ## .........................................................................
        # try to create the index
        ## .........................................................................
        
        cmd = "/_api/index?collection=#{@cn}"
        body = "{ \"type\" : \"bitarray\", \"unique\" : false, \"fields\" : [  \"a\", [0,1,2,2], \"b\", [\"x\",\"y\",\"z\"] ] }"
        doc = ArangoDB.log_post("#{prefix}-fail", cmd, :body => body)
        doc.code.should eq(400)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(3417)
      end  
      
      
    end
    
    
    context "testing index creation post document insertion:" do
    
      before do
        @cn = "UnitTestsCollectionIndexes"
        ArangoDB.drop_collection(@cn)
        @cid = ArangoDB.create_collection(@cn)
        
        ## .........................................................................
        ## attempt to insert some documents 
        ## .........................................................................
        
        cmd = "/_api/document?collection=#{@cn}"
        body = "{ \"gender\" : \"male\"}"
        doc = ArangoDB.log_post("#{prefix}-document-insertion", cmd, :body => body)
        doc.code.should eq(201)
        body = "{ \"gender\" : \"female\"}"
        doc = ArangoDB.log_post("#{prefix}-document-insertion", cmd, :body => body)
        doc.code.should eq(201)
        body = "{ \"gender\" : \"unknown\"}"
        doc = ArangoDB.log_post("#{prefix}-document-insertion", cmd, :body => body)
        doc.code.should eq(201)
      end
      
      after do
        ArangoDB.drop_collection(@cn)
      end
      
      it "creation failure due to documents within collection having unsupported values" do
        ## .........................................................................
        ## try to create the index
        ## .........................................................................
        
        cmd = "/_api/index?collection=#{@cn}"
        body = "{ \"type\" : \"bitarray\", \"unique\" : false, \"fields\" : [  \"gender\",[\"male\", \"female\"] ] }"
        doc = ArangoDB.log_post("#{prefix}-fail", cmd, :body => body)
        doc.code.should eq(400)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(3413)        
      end      

      it "creation success when all documents have supported values" do
        ## .........................................................................
        ## try to create the index
        ## .........................................................................
        
        cmd = "/_api/index?collection=#{@cn}"
        body = "{ \"type\" : \"bitarray\", \"unique\" : false, \"fields\" : [  \"gender\", [\"male\", \"female\", \"unknown\"] ] }"
        doc = ArangoDB.log_post("#{prefix}-fail", cmd, :body => body)
        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
      end      
      
    end
    
    
    context "testing index creation pre document insertion:" do

      before do
        @cn = "UnitTestsCollectionIndexes"
        ArangoDB.drop_collection(@cn)
        @cid = ArangoDB.create_collection(@cn)
        
      end
      
      after do
        ArangoDB.drop_collection(@cn)
      end
    
      it "document insertion failure when index does not support values-1" do

        ## .........................................................................
        ## create the index first
        ## .........................................................................
        
        cmd = "/_api/index?collection=#{@cn}"
        body = "{ \"type\" : \"bitarray\", \"unique\" : false, \"fields\" : [  \"x\", [0,1,2,3,4,5,6,7,8,9] ] }"
        doc = ArangoDB.log_post("#{prefix}-fail", cmd, :body => body)
        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        
        cmd = "/_api/document?collection=#{@cn}"
        body = "{ \"gender\" : \"male\", \"x\": 0}"
        doc = ArangoDB.log_post("#{prefix}-document-insertion", cmd, :body => body)
        doc.code.should eq(201)
        cmd = "/_api/document?collection=#{@cn}"
        body = "{ \"gender\" : \"female\", \"x\": 1}"
        doc = ArangoDB.log_post("#{prefix}-document-insertion", cmd, :body => body)
        doc.code.should eq(201)
        cmd = "/_api/document?collection=#{@cn}"
        body = "{ \"gender\" : \"unknown\", \"x\": 10}"
        doc = ArangoDB.log_post("#{prefix}-document-insertion", cmd, :body => body)
        doc.code.should eq(500)
        
      end  

      
      it "document insertion failure when index does not support values-2" do

        ## .........................................................................
        ## create the index first
        ## .........................................................................
        
        cmd = "/_api/index?collection=#{@cn}"
        body = "{ \"type\" : \"bitarray\", \"unique\" : false, \"fields\" : [  \"x\", [0,1,2,3,4,5,6,7,8,9], \"gender\", [\"male\", \"female\"] ] }"
        doc = ArangoDB.log_post("#{prefix}-fail", cmd, :body => body)
        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        
        cmd = "/_api/document?collection=#{@cn}"
        body = "{ \"gender\" : \"male\", \"x\": 0}"
        doc = ArangoDB.log_post("#{prefix}-document-insertion", cmd, :body => body)
        doc.code.should eq(201)
        body = "{ \"gender\" : \"female\", \"x\": 1}"
        doc = ArangoDB.log_post("#{prefix}-document-insertion", cmd, :body => body)
        doc.code.should eq(201)
        body = "{ \"gender\" : \"female\", \"x\": 10}"
        doc = ArangoDB.log_post("#{prefix}-document-insertion", cmd, :body => body)
        doc.code.should eq(500)
        body = "{ \"gender\" : \"unknown\", \"x\": 0}"
        doc = ArangoDB.log_post("#{prefix}-document-insertion", cmd, :body => body)
        doc.code.should eq(500)
        
      end  
      
      it "document insertion success when index support values-1" do

        ## .........................................................................
        ## create the index first
        ## .........................................................................
        
        cmd = "/_api/index?collection=#{@cn}"
        body = "{ \"type\" : \"bitarray\", \"unique\" : false, \"fields\" : [  \"x\", [0,1,2,3,4,5,6,7,8,9] ] }"
        doc = ArangoDB.log_post("#{prefix}-fail", cmd, :body => body)
        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        
        cmd = "/_api/document?collection=#{@cn}"
        body = "{ \"gender\" : \"male\", \"x\": 0}"
        doc = ArangoDB.log_post("#{prefix}-document-insertion", cmd, :body => body)
        doc.code.should eq(201)
        body = "{ \"gender\" : \"female\", \"x\": 1}"
        doc = ArangoDB.log_post("#{prefix}-document-insertion", cmd, :body => body)
        doc.code.should eq(201)
        body = "{ \"gender\" : \"unknown\", \"x\": 2}"
        doc = ArangoDB.log_post("#{prefix}-document-insertion", cmd, :body => body)
        doc.code.should eq(201)
        
      end  

      it "document insertion success when index support values-2" do

        ## .........................................................................
        ## create the index first
        ## .........................................................................
        
        cmd = "/_api/index?collection=#{@cn}"
        body = "{ \"type\" : \"bitarray\", \"unique\" : false, \"fields\" : [\"gender\", [\"male\", \"female\", \"unknown\"],  \"x\", [0,1,2,3,4,5,6,7,8,9] ] }"
        doc = ArangoDB.log_post("#{prefix}-fail", cmd, :body => body)
        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        
        cmd = "/_api/document?collection=#{@cn}"
        body = "{ \"gender\" : \"male\", \"x\": 0}"
        doc = ArangoDB.log_post("#{prefix}-document-insertion", cmd, :body => body)
        doc.code.should eq(201)
        body = "{ \"gender\" : \"female\", \"x\": 1}"
        doc = ArangoDB.log_post("#{prefix}-document-insertion", cmd, :body => body)
        doc.code.should eq(201)
        body = "{ \"gender\" : \"unknown\", \"x\": 2}"
        doc = ArangoDB.log_post("#{prefix}-document-insertion", cmd, :body => body)
        doc.code.should eq(201)
        
      end  

      it "document insertion success when index support values-3" do

        ## .........................................................................
        ## create the index first
        ## .........................................................................
        
        cmd = "/_api/index?collection=#{@cn}"
        body = "{ \"type\" : \"bitarray\", \"unique\" : false, \"fields\" : [\"gender\", [\"male\", \"female\", \"unknown\"],  \"x\", [0,1,2,3,4,5,6,7,8,9, []] ] }"
        doc = ArangoDB.log_post("#{prefix}-fail", cmd, :body => body)
        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        
        cmd = "/_api/document?collection=#{@cn}"
        body = "{ \"gender\" : \"male\", \"x\": 0}"
        doc = ArangoDB.log_post("#{prefix}-document-insertion", cmd, :body => body)
        doc.code.should eq(201)
        body = "{ \"gender\" : \"female\", \"x\": 1}"
        doc = ArangoDB.log_post("#{prefix}-document-insertion", cmd, :body => body)
        doc.code.should eq(201)
        body = "{ \"gender\" : \"unknown\", \"x\": 2}"
        doc = ArangoDB.log_post("#{prefix}-document-insertion", cmd, :body => body)
        doc.code.should eq(201)
        body = "{ \"gender\" : \"unknown\", \"x\": \"hi there, I'm Marvin\"}"
        doc = ArangoDB.log_post("#{prefix}-document-insertion", cmd, :body => body)
        doc.code.should eq(201)
        
      end  
      
    end
    
  end

  
  
  context "updating documents in a collection with a bitarray index:" do
   
    before do
      @cn = "UnitTestsCollectionIndexes"
      ArangoDB.drop_collection(@cn)
      @cid = ArangoDB.create_collection(@cn)        
    end
      
    after do
      ArangoDB.drop_collection(@cn)
    end
    
    it "document update failure when index does not support values-1" do

        ## .........................................................................
        ## create the index 
        ## .........................................................................
        
        cmd = "/_api/index?collection=#{@cn}"
        body = "{ \"type\" : \"bitarray\", \"unique\" : false, \"fields\" : [  \"x\", [0,1,2,3,4,5,6,7,8,9] ] }"
        doc = ArangoDB.log_post("#{prefix}-fail", cmd, :body => body)
        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)

        ## .........................................................................
        ## now insert documents and test roll back 
        ## .........................................................................
        
        cmd = "/_api/document?collection=#{@cn}"
        ok = true
        theDoc = ""
        
        for i in 0..9
          for j in 0..9
            body = "{ \"x\": " + j.to_s() + ", \"y\": " + i.to_s() + "}"
            doc = ArangoDB.log_post("#{prefix}-document-insertion", cmd, :body => body)
            if (i == 5 and j == 5) then
              theDoc = doc.parsed_response["_id"]
            end              
            if doc.code != 201 then
              ok = false
              break
            end  
            doc.code.should eq(201)
            #puts "i=#{i}, j=#{j}\n"
          end
          if ok == false then
            break
          end  
        end        

        if ok == false then
          doc.code.should eq(201)
          return
        end
        
        ## .........................................................................
        ## update a document
        ## .........................................................................
        
        #puts(theDoc)
        cmd = "/_api/document/#{theDoc}"
        #puts(cmd)
        body = "{ \"x\": -1, \"y\": \"does not matter\"}"
        doc = ArangoDB.log_put("#{prefix}-document-update", cmd, :body => body)
        #puts(doc.body)
        doc.code.should eq(500)
    end  
    
  end  
  
end
