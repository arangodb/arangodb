# coding: utf-8

require 'rspec'
require 'arangodb.rb'
require 'arangomultipartbody.rb'

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
## checking invalid posts
################################################################################
  
    context "checking wrong/missing content-type/boundary:" do
      it "checks missing content-type" do
        cmd = "/_api/batch"

        doc = ArangoDB.log_post("#{prefix}-post-ct-none", cmd, :body => "xx" )

        doc.code.should eq(400)
        doc.parsed_response['error'].should eq(true) 
        doc.parsed_response['code'].should eq(400)
      end
      
      it "checks invalid content-type (xxx)" do
        cmd = "/_api/batch"

        doc = ArangoDB.log_post("#{prefix}-post-ct-wrong-1", cmd, :body => "xx", :headers => { "Content-Type" => "xxx/xxx" } )

        doc.code.should eq(400)
        doc.parsed_response['error'].should eq(true) 
        doc.parsed_response['code'].should eq(400)
      end
      
      it "checks invalid content-type (json)" do
        cmd = "/_api/batch"

        doc = ArangoDB.log_post("#{prefix}-post-ct-wrong-2", cmd, :body => "xx", :headers => { "Content-Type" => "application/json" } )

        doc.code.should eq(400)
        doc.parsed_response['error'].should eq(true) 
        doc.parsed_response['code'].should eq(400)
      end
      
      it "checks valid content-type with missing boundary" do
        cmd = "/_api/batch"

        doc = ArangoDB.log_post("#{prefix}-post-boundary-wrong-1", cmd, :body => "xx", :headers => { "Content-Type" => "multipart/form-data" } )

        doc.code.should eq(400)
        doc.parsed_response['error'].should eq(true) 
        doc.parsed_response['code'].should eq(400)
      end
      
      it "checks valid content-type with unexpected boundary" do
        cmd = "/_api/batch"

        doc = ArangoDB.log_post("#{prefix}-post-boundary-wrong-2", cmd, :body => "xx", :headers => { "Content-Type" => "multipart/form-data; peng" } )

        doc.code.should eq(400)
        doc.parsed_response['error'].should eq(true) 
        doc.parsed_response['code'].should eq(400)
      end
      
      it "checks valid content-type with broken boundary" do
        cmd = "/_api/batch"

        doc = ArangoDB.log_post("#{prefix}-post-boundary-wrong-3", cmd, :body => "xx", :headers => { "Content-Type" => "multipart/form-data; boundary=" } )

        doc.code.should eq(400)
        doc.parsed_response['error'].should eq(true) 
        doc.parsed_response['code'].should eq(400)
      end
      
      it "checks valid content-type with too short boundary" do
        cmd = "/_api/batch"

        doc = ArangoDB.log_post("#{prefix}-post-boundary-wrong-4", cmd, :body => "xx", :headers => { "Content-Type" => "multipart/form-data; boundary=a" } )

        doc.code.should eq(400)
        doc.parsed_response['error'].should eq(true) 
        doc.parsed_response['code'].should eq(400)
      end

    end

################################################################################
## checking simple batches
################################################################################
  
    context "checking simple requests:" do
      
      it "checks a multipart message assembled manually, broken boundary" do
        cmd = "/_api/batch"
        body = "--SomeBoundaryValue\r\nContent-Type: application/x-arango-batchpart\r\n\r\nGET /_api/version HTTP/1.1\r\n\r\n--NotExistingBoundaryValue--"
        
        doc = ArangoDB.log_post("#{prefix}-post-version-manual", cmd, :body => body, :format => :plain)

        doc.code.should eq(400)
      end
      
      it "checks a multipart message assembled manually, no boundary" do
        cmd = "/_api/batch"
        body = "Content-Type: application/x-arango-batchpart\r\n\r\nGET /_api/version HTTP/1.1\r\n\r\n"
        
        doc = ArangoDB.log_post("#{prefix}-post-version-manual", cmd, :body => body, :format => :plain)

        doc.code.should eq(400)
      end
      

      it "checks a multipart message assembled manually" do
        cmd = "/_api/batch"
        body = "--SomeBoundaryValue\r\nContent-Type: application/x-arango-batchpart\r\n\r\nGET /_api/version HTTP/1.1\r\n\r\n--SomeBoundaryValue--"
        
        doc = ArangoDB.log_post("#{prefix}-post-version-manual", cmd, :body => body, :format => :plain)

        doc.code.should eq(200)
      end
      
      it "checks a multipart message assembled manually, with 404 URLs" do
        cmd = "/_api/batch"
        body = "--SomeBoundaryValue\r\nContent-Type: application/x-arango-batchpart\r\n\r\nGET /_api/nonexisting1 HTTP/1.1\r\n\r\n--SomeBoundaryValue\r\nContent-Type: application/x-arango-batchpart\r\n\r\nGET /_api/nonexisting2 HTTP/1.1\r\n\r\n--SomeBoundaryValue--"
        
        doc = ArangoDB.log_post("#{prefix}-post-version-manual", cmd, :body => body, :format => :plain)

        doc.code.should eq(200)
        doc.headers['x-arango-errors'].should eq("2")
      end

      it "checks an empty operation multipart message" do
        cmd = "/_api/batch"

        multipart = ArangoMultipartBody.new()

        doc = ArangoDB.log_post("#{prefix}-post-version-empty", cmd, :body => multipart.to_s, :format => :json, :headers => { "Content-Type" => "multipart/form-data; boundary=" + multipart.getBoundary })

        doc.code.should eq(400)
        doc.parsed_response['error'].should eq(true) 
        doc.parsed_response['code'].should eq(400)
      end
      
      it "checks a multipart message with a single operation" do
        cmd = "/_api/batch"

        multipart = ArangoMultipartBody.new()

        multipart.addPart("GET", "/_api/version", { }, "")
        doc = ArangoDB.log_post("#{prefix}-post-version-one", cmd, :body => multipart.to_s, :format => :plain, :headers => { "Content-Type" => "multipart/form-data; boundary=" + multipart.getBoundary })

        doc.code.should eq(200)

        parts = multipart.getParts(multipart.getBoundary, doc.response.body)

        parts.each do|part|
          part[:status].should eq(200)
        end
      end
      
      it "checks valid content-type with boundary in quotes" do
        cmd = "/_api/batch"

        multipart = ArangoMultipartBody.new()

        multipart.addPart("GET", "/_api/version", { }, "")
        multipart.addPart("GET", "/_api/version", { }, "")
        multipart.addPart("GET", "/_api/version", { }, "")
        multipart.addPart("GET", "/_api/version", { }, "")
        doc = ArangoDB.log_post("#{prefix}-post-version-mult",
                                cmd,
                                :body => multipart.to_s,
                                :format => :plain,
                                :headers => {
                                  "Content-Type" => "multipart/form-data; boundary=\"" + multipart.getBoundary + "\""
                                })

        doc.code.should eq(200)

        parts = multipart.getParts(multipart.getBoundary, doc.response.body)

        parts.each do|part|
          part[:status].should eq(200)
        end
      end
      

      it "checks a multipart message with a multiple operations" do
        cmd = "/_api/batch"

        multipart = ArangoMultipartBody.new()

        multipart.addPart("GET", "/_api/version", { }, "")
        multipart.addPart("GET", "/_api/version", { }, "")
        multipart.addPart("GET", "/_api/version", { }, "")
        multipart.addPart("GET", "/_api/version", { }, "")
        doc = ArangoDB.log_post("#{prefix}-post-version-mult",
                                cmd,
                                :body => multipart.to_s,
                                :format => :plain,
                                :headers => {
                                  "Content-Type" => "multipart/form-data; boundary=" + multipart.getBoundary
                                })

        doc.code.should eq(200)

        parts = multipart.getParts(multipart.getBoundary, doc.response.body)

        parts.each do|part|
          part[:status].should eq(200)
        end
      end
      
      it "checks a multipart message with many operations" do
        cmd = "/_api/batch"

        multipart = ArangoMultipartBody.new()

        (1..128).each do
          multipart.addPart("GET", "/_api/version", { }, "")
        end
        doc = ArangoDB.log_post("#{prefix}-post-version-many", cmd, :body => multipart.to_s, :format => :plain, :headers => { "Content-Type" => "multipart/form-data; boundary=" + multipart.getBoundary })

        doc.code.should eq(200)

        parts = multipart.getParts(multipart.getBoundary, doc.response.body)

        parts.each do|part|
          part[:status].should eq(200)
        end
      end
      
      it "checks a multipart message inside a multipart message" do
        cmd = "/_api/batch"

        multipart = ArangoMultipartBody.new()
        inner = ArangoMultipartBody.new("innerBoundary")
        inner.addPart("GET", "/_api/version", { }, "")

        multipart.addPart("POST", "/_api/batch", { "Content-Type" => "multipart/form-data; boundary=innerBoundary"}, inner.to_s)
        doc = ArangoDB.log_post("#{prefix}-post-version-nested", cmd, :body => multipart.to_s, :format => :plain, :headers => { "Content-Type" => "multipart/form-data; boundary=" + multipart.getBoundary })

        doc.code.should eq(200)

        parts = multipart.getParts(multipart.getBoundary, doc.response.body)

        parts.each do|part|
          part[:status].should eq(200)
        end
      end
      
      it "checks a few multipart messages inside a multipart message" do
        cmd = "/_api/batch"

        multipart = ArangoMultipartBody.new()
        inner = ArangoMultipartBody.new("innerBoundary")
        inner.addPart("GET", "/_api/version", { }, "")
        inner.addPart("GET", "/_api/version", { }, "")
        inner.addPart("GET", "/_api/version", { }, "")
        inner.addPart("GET", "/_api/version", { }, "")
        inner.addPart("GET", "/_api/version", { }, "")
        inner.addPart("GET", "/_api/version", { }, "")

        multipart.addPart("POST", "/_api/batch", { "Content-Type" => "multipart/form-data; boundary=innerBoundary"}, inner.to_s)
        doc = ArangoDB.log_post("#{prefix}-post-version-nested", cmd, :body => multipart.to_s, :format => :plain, :headers => { "Content-Type" => "multipart/form-data; boundary=" + multipart.getBoundary })

        doc.code.should eq(200)

        parts = multipart.getParts(multipart.getBoundary, doc.response.body)

        parts.each do|part|
          part[:status].should eq(200)
        end
      end

    end

################################################################################
## checking batch document creation
################################################################################
    
    context "checking batch document creation:" do

      before do
        @cn = "UnitTestsBatch"
        ArangoDB.drop_collection(@cn)
      end

      after do
        ArangoDB.drop_collection(@cn)
      end
      
      it "checks batch document creation" do
        cmd = "/_api/batch"

        # create 10 documents
        multipart = ArangoMultipartBody.new()
        multipart.addPart("POST", "/_api/collection", { }, "{\"name\":\"#{@cn}\"}")
        (1..10).each do
          multipart.addPart("POST", "/_api/document?collection=#{@cn}", { }, "{\"a\":1,\"b\":2}")
        end

        doc = ArangoDB.log_post("#{prefix}-post-documents", cmd, :body => multipart.to_s, :format => :plain, :headers => { "Content-Type" => "multipart/form-data; boundary=" + multipart.getBoundary })

        doc.code.should eq(200)
        doc.headers['x-arango-errors'].should be_nil

        parts = multipart.getParts(multipart.getBoundary, doc.response.body)

        partNumber = 0
        parts.each do|part|
          if partNumber == 0
            part[:status].should eq(200)
          else
            part[:status].should eq(202)
          end

          partNumber = partNumber + 1
        end

        # check number of documents in collection  
        doc = ArangoDB.log_get("#{prefix}-get-collection-figures", "/_api/collection/#{@cn}/figures")

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['status'].should eq(3)
        doc.parsed_response['count'].should eq(10)
      end
    
    end

################################################################################
## checking document creation with a few errors
################################################################################
    
    context "checking batch document creation with some errors:" do

      before do
        @cn = "UnitTestsBatch"
        @cn2 = "UnitTestsBatch2"
        ArangoDB.drop_collection(@cn)
        ArangoDB.drop_collection(@cn2)
        cid = ArangoDB.create_collection(@cn)
      end
      
      after do
        @cn = "UnitTestsBatch"
        @cn2 = "UnitTestsBatch2"
        ArangoDB.drop_collection(@cn)
        ArangoDB.drop_collection(@cn2)
      end

      it "checks batch document creation error" do
        cmd = "/_api/batch"

        n = 10
        multipart = ArangoMultipartBody.new()
        (1..n).each do |partNumber|
          if partNumber % 2 == 1
            # should succeed
            multipart.addPart("POST", "/_api/document?collection=#{@cn}", { }, "{\"a\":1,\"b\":2}")
          else
            # should fail
            multipart.addPart("POST", "/_api/document?collection=#{@cn2}", { }, "{\"a\":1,\"b\":2}")
          end
        end

        doc = ArangoDB.log_post("#{prefix}-post-documents", cmd, :body => multipart.to_s, :format => :plain, :headers => { "Content-Type" => "multipart/form-data; boundary=" + multipart.getBoundary })

        doc.code.should eq(200)
        doc.headers['x-arango-errors'].should eq("5")

        parts = multipart.getParts(multipart.getBoundary, doc.response.body)

        partNumber = 1
        parts.each do|part|
          if partNumber % 2 == 1
            # assert success
            part[:status].should eq(201)
          else
            # assert failure
            part[:status].should eq(404)
          end
          partNumber = partNumber + 1
        end

        # check number of documents in collection  
        doc = ArangoDB.log_get("#{prefix}-get-collection-figures", "/_api/collection/#{@cn}/figures")

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['status'].should eq(3)
        doc.parsed_response['count'].should eq(n / 2)
      end
    
    end

################################################################################
## checking document creation with errors
################################################################################
    
    context "checking batch document creation with non-existing collection:" do

      before do
        @cn = "UnitTestsBatch"
        ArangoDB.drop_collection(@cn)
      end

      it "checks batch document creation nx collection" do
        cmd = "/_api/batch"

        multipart = ArangoMultipartBody.new()
        (1..10).each do
          multipart.addPart("POST", "/_api/document?collection=#{@cn}", { }, "{\"a\":1,\"b\":2}")
        end

        doc = ArangoDB.log_post("#{prefix}-post-documents", cmd, :body => multipart.to_s, :format => :plain, :headers => { "Content-Type" => "multipart/form-data; boundary=" + multipart.getBoundary })

        doc.code.should eq(200)
        doc.headers['x-arango-errors'].should eq("10")

        parts = multipart.getParts(multipart.getBoundary, doc.response.body)

        parts.each do|part|
          part[:status].should eq(404)
        end

        # check number of documents in collection  
        doc = ArangoDB.log_get("#{prefix}-get-collection-figures", "/_api/collection/#{@cn}/figures")

        doc.code.should eq(404)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(404)
        doc.parsed_response['errorNum'].should eq(1203)
      end
    
    end

################################################################################
## checking content ids
################################################################################
  
    context "checking content ids:" do
      
      it "checks a multipart message with content-ids" do
        cmd = "/_api/batch"

        multipart = ArangoMultipartBody.new()
        multipart.addPart("GET", "/_api/version", { }, "", "part1")
        multipart.addPart("GET", "/_api/version", { }, "", "part2")
        multipart.addPart("GET", "/_api/version", { }, "", "part3")

        doc = ArangoDB.log_post("#{prefix}-post-content-id", cmd, :body => multipart.to_s, :format => :plain, :headers => { "Content-Type" => "multipart/form-data; boundary=" + multipart.getBoundary })

        doc.code.should eq(200)

        parts = multipart.getParts(multipart.getBoundary, doc.response.body)

        i = 1;
        parts.each do|part|
          part[:status].should eq(200)
          part[:contentId].should eq("part" + i.to_s)
          i = i + 1
        end
      end
      
      it "checks a multipart message with identical content-ids" do
        cmd = "/_api/batch"

        multipart = ArangoMultipartBody.new()
        multipart.addPart("GET", "/_api/version", { }, "", "part1")
        multipart.addPart("GET", "/_api/version", { }, "", "part1")
        multipart.addPart("GET", "/_api/version", { }, "", "part1")

        doc = ArangoDB.log_post("#{prefix}-post-content-id", cmd, :body => multipart.to_s, :format => :plain, :headers => { "Content-Type" => "multipart/form-data; boundary=" + multipart.getBoundary })

        doc.code.should eq(200)

        parts = multipart.getParts(multipart.getBoundary, doc.response.body)

        parts.each do|part|
          part[:status].should eq(200)
          part[:contentId].should eq("part1")
        end
      end
      
      it "checks a multipart message with very different content-ids" do
        cmd = "/_api/batch"

        multipart = ArangoMultipartBody.new()
        multipart.addPart("GET", "/_api/version", { }, "", "    abcdef.gjhdjslrt.sjgfjss@024n5nhg.sdffns.gdfnkddgme-fghnsnfg")
        multipart.addPart("GET", "/_api/version", { }, "", "a")
        multipart.addPart("GET", "/_api/version", { }, "", "94325.566335.5555hd.3553-4354333333333333 ")

        doc = ArangoDB.log_post("#{prefix}-post-content-id", cmd, :body => multipart.to_s, :format => :plain, :headers => { "Content-Type" => "multipart/form-data; boundary=" + multipart.getBoundary })

        doc.code.should eq(200)

        parts = multipart.getParts(multipart.getBoundary, doc.response.body)

        i = 1
        parts.each do|part|
          part[:status].should eq(200)
          if i == 1
            part[:contentId].should eq("abcdef.gjhdjslrt.sjgfjss@024n5nhg.sdffns.gdfnkddgme-fghnsnfg")
          elsif i == 2
            part[:contentId].should eq("a")
          elsif i == 3
            part[:contentId].should eq("94325.566335.5555hd.3553-4354333333333333")
          end
          i = i + 1
        end
      end

    end

      
  end

end
