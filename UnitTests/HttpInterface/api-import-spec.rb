# coding: utf-8

require 'rspec'
require './arangodb.rb'

describe ArangoDB do
  api = "/_api/import"
  prefix = "api-import"

  context "importing documents:" do

################################################################################
## import self-contained documents
################################################################################

    context "import self-contained documents:" do
      before do
	@cn = "UnitTestsImport"
	ArangoDB.drop_collection(@cn)
	@cid = ArangoDB.create_collection(@cn, false)
      end

      after do
	ArangoDB.drop_collection(@cn)
      end

      it "using different documents" do
        cmd = api + "?collection=#{@cn}&createCollection=true&type=documents"
	body =  "{ \"name\" : \"John\", \"age\" : 29, \"gender\" : \"male\", \"likes\" : [ \"socket\", \"bind\", \"accept\" ] }\n"
	body += "{ \"firstName\" : \"Jane\", \"lastName\" : \"Doe\", \"age\" : 35, \"gender\" : \"female\", \"livesIn\" : \"Manila\" }\n"
	body += "{ \"sample\" : \"garbage\" }\n"
	body += "{ }"
	doc = ArangoDB.log_post("#{prefix}-self-contained-different", cmd, :body => body)

	doc.code.should eq(201)
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['created'].should eq(4)
	doc.parsed_response['errors'].should eq(0)
	doc.parsed_response['empty'].should eq(0)
      end
      
      it "using whitespace" do
        cmd = api + "?collection=#{@cn}&createCollection=true&type=documents"
	body =  "\n\n      { \"name\" : \"John\", \"age\" : 29 }      \n     \n \n \r\n \n { \"rubbish\" : \"data goes in here\" }\n\n"
	doc = ArangoDB.log_post("#{prefix}-self-contained-ws", cmd, :body => body)

	doc.code.should eq(201)
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['created'].should eq(2)
	doc.parsed_response['errors'].should eq(0)
	doc.parsed_response['empty'].should eq(7)
      end
      
      it "invalid documents" do
        cmd = api + "?collection=#{@cn}&createCollection=true&type=documents"
	body =  "{ \"this doc\" : \"isValid\" }\n{ \"this one\" : is not }\n{ \"again\" : \"this is ok\" }\n\n{ \"but this isn't\" }\n"
	doc = ArangoDB.log_post("#{prefix}-self-contained-invalid", cmd, :body => body)

	doc.code.should eq(201)
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['created'].should eq(2)
	doc.parsed_response['errors'].should eq(2)
	doc.parsed_response['empty'].should eq(1)
      end
      
      it "empty body" do
        cmd = api + "?collection=#{@cn}&createCollection=true&type=documents"
	body =  "" 
	doc = ArangoDB.log_post("#{prefix}-self-contained-empty", cmd, :body => body)

	doc.code.should eq(201)
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['created'].should eq(0)
	doc.parsed_response['errors'].should eq(0)
	doc.parsed_response['empty'].should eq(0)
      end
      
      it "no documents" do
        cmd = api + "?collection=#{@cn}&createCollection=true&type=documents"
	body =  "\n\n"
	doc = ArangoDB.log_post("#{prefix}-self-contained-none", cmd, :body => body)

	doc.code.should eq(201)
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['created'].should eq(0)
	doc.parsed_response['errors'].should eq(0)
	doc.parsed_response['empty'].should eq(2)
      end
      
      it "no collection" do
        cmd = api + "?type=documents"
	body =  "\n\n"
	doc = ArangoDB.log_post("#{prefix}-self-contained-nocoll", cmd, :body => body)

	doc.code.should eq(400)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(1204)
      end
      
      it "non-existing collection" do
	ArangoDB.drop_collection(@cn)

        cmd = api + "?collection=#{@cn}&type=documents"
	body = "{ \"sample\" : \"garbage\" }"
	doc = ArangoDB.log_post("#{prefix}-self-contained-nonexist", cmd, :body => body)
 
	doc.code.should eq(400)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(1203)
      end

    end

################################################################################
## import attribute names and data
################################################################################

    context "import attribute names and data:" do
      before do
	@cn = "UnitTestsImport"
	ArangoDB.drop_collection(@cn)
	@cid = ArangoDB.create_collection(@cn, false)
      end

      after do
	ArangoDB.drop_collection(@cn)
      end

      it "regular" do
        cmd = api + "?collection=#{@cn}&createCollection=true"
	body =  "[ \"name\", \"age\", \"gender\" ]\n"
	body += "[ \"John\", 29, \"male\" ]\n"
	body += "[ \"Jane\", 35, \"female\" ]"
	doc = ArangoDB.log_post("#{prefix}-data-different", cmd, :body => body)

	doc.code.should eq(201)
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['created'].should eq(2)
	doc.parsed_response['errors'].should eq(0)
	doc.parsed_response['empty'].should eq(0)
      end
      
      it "using whitespace" do
        cmd = api + "?collection=#{@cn}&createCollection=true"
	body =  "[ \"name\", \"age\", \"gender\" ]\n\n"
	body += "       [  \"John\", 29, \"male\" ]\n\n\r\n"
	body += "[ \"Jane\", 35, \"female\" ]\n\n"
	doc = ArangoDB.log_post("#{prefix}-data-ws", cmd, :body => body)

	doc.code.should eq(201)
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['created'].should eq(2)
	doc.parsed_response['errors'].should eq(0)
	doc.parsed_response['empty'].should eq(4)
      end
      
      it "invalid documents" do
        cmd = api + "?collection=#{@cn}&createCollection=true"
	body =  "[ \"name\", \"age\", \"gender\" ]\n"
	body += "[ \"John\", 29, \"male\" ]\n"
	body += "[ \"Jane\" ]\n"
	body += "[ ]\n"
	body += "[ 1, 2, 3, 4 ]\n"
	body += "[ \"John\", 29, \"male\" ]"
	doc = ArangoDB.log_post("#{prefix}-data-invalid", cmd, :body => body)

	doc.code.should eq(201)
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['created'].should eq(2)
	doc.parsed_response['errors'].should eq(3)
	doc.parsed_response['empty'].should eq(0)
      end
      
      it "missing header" do
        cmd = api + "?collection=#{@cn}&createCollection=true"
	body =  "\n[ \"name\", \"age\", \"gender\" ]\n"
	body += "[ \"John\", 29, \"male\" ]"
	doc = ArangoDB.log_post("#{prefix}-data-missing-header", cmd, :body => body)

	doc.code.should eq(400)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(400)
      end
      
      it "wrong header" do
        cmd = api + "?collection=#{@cn}&createCollection=true"
	body =  "{ \"name\" : \"John\", \"age\" : 29 }\n"
	doc = ArangoDB.log_post("#{prefix}-data-wrong-header", cmd, :body => body)

	doc.code.should eq(400)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(400)
      end
      
      it "empty body" do
        cmd = api + "?collection=#{@cn}&createCollection=true"
	body =  "" 
	doc = ArangoDB.log_post("#{prefix}-data-empty", cmd, :body => body)

	doc.code.should eq(400)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(400)
      end
      
      it "no documents" do
        cmd = api + "?collection=#{@cn}&createCollection=true"
	body =  "[ \"name\" ]\n\n"
	doc = ArangoDB.log_post("#{prefix}-data-none", cmd, :body => body)

	doc.code.should eq(201)
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['created'].should eq(0)
	doc.parsed_response['errors'].should eq(0)
	doc.parsed_response['empty'].should eq(1)
      end
      
      it "no collection" do
        cmd = api 
	body =  "[ \"name\" ]\n"
	body += "[ \"Jane\" ]"
	doc = ArangoDB.log_post("#{prefix}-data-nocoll", cmd, :body => body)

	doc.code.should eq(400)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(1204)
      end
      
      it "non-existing collection" do
	ArangoDB.drop_collection(@cn)

        cmd = api + "?collection=#{@cn}"
	body =  "[ \"name\" ]\n"
	doc = ArangoDB.log_post("#{prefix}-data-nonexist", cmd, :body => body)
 
	doc.code.should eq(400)
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(1203)
      end

    end

  end
end
