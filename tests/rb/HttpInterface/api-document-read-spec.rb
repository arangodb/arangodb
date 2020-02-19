# coding: utf-8

require 'rspec'
require 'arangodb.rb'

describe ArangoDB do
  prefix = "rest-read-document"

  context "reading a document:" do
 
  before do  
    @rePath = Regexp.new('^/_db/[^/]+/_api/document/[a-zA-Z0-9_@:\.\-]+/\d+$')
    @reFull = Regexp.new('^[a-zA-Z0-9_\-]+/\d+$')
    @reRev  = Regexp.new('^[-_0-9a-zA-Z]+$')
  end

################################################################################
## error handling
################################################################################

    context "error handling:" do
      before do
        @cn = "UnitTestsCollectionBasics"
        @cid = ArangoDB.create_collection(@cn)
      end

      after do
        ArangoDB.drop_collection(@cn)
      end

      it "returns an error if document identifier is corrupted" do
        cmd = "/_api/document/123456"
        doc = ArangoDB.log_get("#{prefix}-bad-identifier", cmd)

        doc.code.should eq(404)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(1203)
        doc.parsed_response['code'].should eq(404)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
      end

      it "returns an error if document identifier is corrupted with empty cid" do
        cmd = "/_api/document//123456"
        doc = ArangoDB.log_get("#{prefix}-bad-identifier2", cmd)

        doc.code.should eq(404)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(1203)
        doc.parsed_response['code'].should eq(404)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
      end

      it "returns an error if collection identifier is unknown" do
        cmd = "/_api/document/123456/234567"
        doc = ArangoDB.log_get("#{prefix}-unknown-cid", cmd)

        doc.code.should eq(404)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(1203)
        doc.parsed_response['code'].should eq(404)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
      end

      it "returns an error if document identifier is unknown" do
        cmd = "/_api/document/#{@cid}/234567"
        doc = ArangoDB.log_get("#{prefix}-unknown-identifier", cmd)

        doc.code.should eq(404)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(1202)
        doc.parsed_response['code'].should eq(404)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        ArangoDB.size_collection(@cid).should eq(0)
      end
    end

################################################################################
## reading documents
################################################################################

    context "reading a document:" do
      before do
        @cn = "UnitTestsCollectionBasics"
        @cid = ArangoDB.create_collection(@cn)
        @reStart = Regexp.new('^' + @cn + '/') 
      end

      after do
        ArangoDB.drop_collection(@cn)
      end

      it "create a document and read it" do
        cmd = "/_api/document?collection=#{@cid}"
        body = "{ \"Hallo\" : \"World\" }"
        doc = ArangoDB.post(cmd, :body => body)

        doc.code.should eq(201)

        location = doc.headers['location']
        location.should be_kind_of(String)

        did = doc.parsed_response['_id']
        
        did.should match(@reFull)
        did.should match(@reStart)
        
        rev = doc.parsed_response['_rev']
        rev.should match(@reRev)

        # get document
        cmd = "/_api/document/#{did}"
        doc = ArangoDB.log_get("#{prefix}", cmd)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        did2 = doc.parsed_response['_id']
        did2.should be_kind_of(String)
        did2.should match(@reFull)
        did2.should match(@reStart)
        did2.should eq(did)
        
        rev2 = doc.parsed_response['_rev']
        rev2.should be_kind_of(String)
        rev2.should match(@reRev)
        rev2.should eq(rev)

        etag = doc.headers['etag']
        etag.should be_kind_of(String)

        etag.should eq("\"#{rev}\"")

        ArangoDB.delete(location)

        ArangoDB.size_collection(@cid).should eq(0)
      end

      it "create a document and read it, using collection name" do
        cmd = "/_api/document?collection=#{@cn}"
        body = "{ \"Hallo\" : \"World\" }"
        doc = ArangoDB.post(cmd, :body => body)

        doc.code.should eq(201)

        location = doc.headers['location']
        location.should be_kind_of(String)

        did = doc.parsed_response['_id']
        did.should match(@reFull)
        did.should match(@reStart)

        rev = doc.parsed_response['_rev']
        rev.should match(@reRev)

        # get document
        cmd = "/_api/document/#{did}"
        doc = ArangoDB.log_get("#{prefix}", cmd)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        did2 = doc.parsed_response['_id']
        did2.should be_kind_of(String)
        did2.should match(@reFull)
        did2.should match(@reStart)
        did2.should eq(did)
        
        rev2 = doc.parsed_response['_rev']
        rev2.should be_kind_of(String)
        rev2.should match(@reRev)
        rev2.should eq(rev)

        etag = doc.headers['etag']
        etag.should be_kind_of(String)

        etag.should eq("\"#{rev}\"")

        ArangoDB.delete(location)

        ArangoDB.size_collection(@cid).should eq(0)
      end

      it "create a document and read it, use if-none-match" do
        cmd = "/_api/document?collection=#{@cid}"
        body = "{ \"Hallo\" : \"World\" }"
        doc = ArangoDB.post(cmd, :body => body)

        doc.code.should eq(201)

        location = doc.headers['location']
        location.should be_kind_of(String)

        did = doc.parsed_response['_id']
        did.should match(@reFull)
        did.should match(@reStart)

        rev = doc.parsed_response['_rev']
        rev.should match(@reRev)

        # get document, if-none-match with same rev
        cmd = "/_api/document/#{did}"
        hdr = { "if-none-match" => "\"#{rev}\"" }
        doc = ArangoDB.log_get("#{prefix}-if-none-match", cmd, :headers => hdr)

        doc.code.should eq(304)

        etag = doc.headers['etag']
        etag.should be_kind_of(String)

        etag.should eq("\"#{rev}\"")

        # get document, if-none-match with different rev
        cmd = "/_api/document/#{did}"
        hdr = { "if-none-match" => "\"54454\"" }
        doc = ArangoDB.log_get("#{prefix}-if-none-match-other", cmd, :headers => hdr)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        etag = doc.headers['etag']
        etag.should be_kind_of(String)

        etag.should eq("\"#{rev}\"")

        did2 = doc.parsed_response['_id']
        did2.should be_kind_of(String)
        did2.should match(@reFull)
        did2.should match(@reStart)
        did2.should eq(did)
        
        rev2 = doc.parsed_response['_rev']
        rev2.should be_kind_of(String)
        rev2.should match(@reRev)
        rev2.should eq(rev)

        etag = doc.headers['etag']
        etag.should be_kind_of(String)

        etag.should eq("\"#{rev}\"")

        ArangoDB.delete(location)

        ArangoDB.size_collection(@cid).should eq(0)
      end

      it "create a document and read it, use if-match" do
        cmd = "/_api/document?collection=#{@cid}"
        body = "{ \"Hallo\" : \"World\" }"
        doc = ArangoDB.post(cmd, :body => body)

        doc.code.should eq(201)

        location = doc.headers['location']
        location.should be_kind_of(String)

        did = doc.parsed_response['_id']
        did.should match(@reFull)
        did.should match(@reStart)

        rev = doc.parsed_response['_rev']
        rev.should match(@reRev)

        # get document, if-match with same rev
        cmd = "/_api/document/#{did}"
        hdr = { "if-match" => "\"#{rev}\"" }
        doc = ArangoDB.log_get("#{prefix}-if-match", cmd, :headers => hdr)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        did2 = doc.parsed_response['_id']
        did2.should be_kind_of(String)
        did2.should match(@reFull) 
        did2.should match(@reStart)
        did2.should eq(did)
        
        rev2 = doc.parsed_response['_rev']
        rev2.should be_kind_of(String)
        rev2.should match(@reRev) 
        rev2.should eq(rev)

        etag = doc.headers['etag']
        etag.should be_kind_of(String)

        etag.should eq("\"#{rev}\"")

        # get document, if-match with different rev
        cmd = "/_api/document/#{did}"
        hdr = { "if-match" => "\"348574\"" }
        doc = ArangoDB.log_get("#{prefix}-if-match-other", cmd, :headers => hdr)

        doc.code.should eq(412)

        did2 = doc.parsed_response['_id']
        did2.should be_kind_of(String)
        did2.should match(@reFull) 
        did2.should match(@reStart)
        did2.should eq(did)
        
        rev2 = doc.parsed_response['_rev']
        rev2.should be_kind_of(String)
        rev2.should match(@reRev)
        rev2.should eq(rev)

        ArangoDB.delete(location)

        ArangoDB.size_collection(@cid).should eq(0)
      end
    end

################################################################################
## reading all documents
################################################################################

    context "reading all documents:" do
      before do
        @cn = "UnitTestsCollectionAll"
        @cid = ArangoDB.create_collection(@cn)
        @reStart = Regexp.new('^/_db/[^/]+/_api/document/' + @cn + '/')
      end

      after do
        ArangoDB.drop_collection(@cn)
      end

      it "get all documents of an empty collection" do
        # get documents
        cmd = "/_api/simple/all-keys"
        body = "{ \"collection\" : \"" + @cn + "\" }"
        doc = ArangoDB.log_put("#{prefix}-all-0", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        documents = doc.parsed_response['result']
        documents.should be_kind_of(Array)
        documents.length.should eq(0)

        ArangoDB.size_collection(@cid).should eq(0)
      end

      it "get all documents of an empty collection, using type=id" do
        # get documents
        cmd = "/_api/simple/all-keys"
        body = "{ \"collection\" : \"" + @cn + "\", \"type\" : \"id\" }"
        doc = ArangoDB.log_put("#{prefix}-all-type-id", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        documents = doc.parsed_response['result']
        documents.should be_kind_of(Array)
        documents.length.should eq(0)

        ArangoDB.size_collection(@cid).should eq(0)
      end

      it "create three documents and read them using the collection identifier" do
        cmd = "/_api/document?collection=#{@cid}"

        location = []

        for i in [ 1, 2, 3 ]
          body = "{ \"Hallo\" : \"World-#{i}\" }"
          doc = ArangoDB.post(cmd, :body => body)

          doc.code.should eq(201)

          location.push(doc.headers['location'])
        end

        # get document
        cmd = "/_api/simple/all-keys"
        body = "{ \"collection\" : \"" + @cn + "\" }"
        doc = ArangoDB.log_put("#{prefix}-all", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        documents = doc.parsed_response['result']
        documents.should be_kind_of(Array)
        documents.length.should eq(3)

        documents.each { |document|
          document.should match(@rePath)
          document.should match(@reStart)
        }

        for l in location
          ArangoDB.delete(l)
        end

        ArangoDB.size_collection(@cid).should eq(0)
      end

      it "create three documents and read them using the collection name" do
        cmd = "/_api/document?collection=#{@cn}"

        location = []

        for i in [ 1, 2, 3 ]
          body = "{ \"Hallo\" : \"World-#{i}\" }"
          doc = ArangoDB.post(cmd, :body => body)

          doc.code.should eq(201)

          location.push(doc.headers['location'])
        end

        # get document
        cmd = "/_api/simple/all-keys"
        body = "{ \"collection\" : \"" + @cn + "\" }"
        doc = ArangoDB.log_put("#{prefix}-all-name", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        documents = doc.parsed_response['result']
        documents.should be_kind_of(Array)
        documents.length.should eq(3)
        
        documents.each { |document|
          document.should match(@rePath)
          document.should match(@reStart)
        }

        for l in location
          ArangoDB.delete(l)
        end

        ArangoDB.size_collection(@cid).should eq(0)
      end

      it "create three documents and read them using the collection name, type=id" do
        cmd = "/_api/document?collection=#{@cn}"

        location = []

        for i in [ 1, 2, 3 ]
          body = "{ \"Hallo\" : \"World-#{i}\" }"
          doc = ArangoDB.post(cmd, :body => body)

          doc.code.should eq(201)

          location.push(doc.headers['location'])
        end

        # get documents
        cmd = "/_api/simple/all-keys"
        body = "{ \"collection\" : \"" + @cn + "\", \"type\" : \"id\" }"
        doc = ArangoDB.log_put("#{prefix}-all-name", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        documents = doc.parsed_response['result']
        documents.should be_kind_of(Array)
        documents.length.should eq(3)
        
        regex = Regexp.new('^' + @cn + '/\d+$');  
        documents.each { |document|
          document.should match(regex)
        }
      end

      it "create three documents and read them using the collection name, type=key" do
        cmd = "/_api/document?collection=#{@cn}"

        location = []

        for i in [ 1, 2, 3 ]
          body = "{ \"Hallo\" : \"World-#{i}\" }"
          doc = ArangoDB.post(cmd, :body => body)

          doc.code.should eq(201)

          location.push(doc.headers['location'])
        end

        # get documents
        #
        cmd = "/_api/simple/all-keys"
        body = "{ \"collection\" : \"" + @cn + "\", \"type\": \"key\" }"
        doc = ArangoDB.log_put("#{prefix}-all-name", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        documents = doc.parsed_response['result']
        documents.should be_kind_of(Array)
        documents.length.should eq(3)
        
        regex = Regexp.new('^\d+$');  
        documents.each { |document|
          document.should match(regex)
        }
      end
    end

################################################################################
## checking document
################################################################################

    context "checking a document:" do
      before do
        @cn = "UnitTestsCollectionBasics"
        @cid = ArangoDB.create_collection(@cn)
      end

      after do
        ArangoDB.drop_collection(@cn)
      end

      it "create a document and read it" do
        cmd = "/_api/document?collection=#{@cid}"
        body = "{ \"Hallo\" : \"World\" }"
        doc = ArangoDB.post(cmd, :body => body)

        doc.code.should eq(201)
        location = doc.headers['location']
        location.should be_kind_of(String)

        # get document
        cmd = location
        doc = ArangoDB.log_get("#{prefix}-head", cmd)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        content_length = doc.headers['content-length']

        # get the document head
        doc = ArangoDB.head(cmd)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.headers['content-length'].should eq(content_length)
        doc.body.should eq(nil)

        ArangoDB.delete(location)

        ArangoDB.size_collection(@cid).should eq(0)
      end

      it "use an invalid revision for HEAD" do
        cmd = "/_api/document?collection=#{@cid}"
        body = "{ \"Hallo\" : \"World\" }"
        doc = ArangoDB.post(cmd, :body => body)

        doc.code.should eq(201)
        location = doc.headers['location']
        location.should be_kind_of(String)

        # get document
        cmd = location
        doc = ArangoDB.log_get("#{prefix}-head-rev-invalid", cmd)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        # get the document head, withdrawn for 3.0
        doc = ArangoDB.head(cmd + "?rev=abcd")

        doc.code.should eq(200)
        
        hdr = { "if-match" => "'*abcd'" }
        doc = ArangoDB.log_head("#{prefix}-head-rev-invalid", cmd, :headers => hdr)
        
        doc.code.should eq(412)
      end

    end

  end
end
