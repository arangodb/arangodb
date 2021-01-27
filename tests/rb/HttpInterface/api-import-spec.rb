# coding: utf-8

require 'rspec'
require 'arangodb.rb'

describe ArangoDB do
  api = "/_api/import"
  prefix = "api-import"

  context "importing documents:" do

################################################################################
## import a JSON array of documents 
################################################################################

    context "import, testing createCollection:" do
      before do
        @cn = "UnitTestsImport"
        ArangoDB.drop_collection(@cn)
      end

      after do
        ArangoDB.drop_collection(@cn)
      end

      it "createCollection=false" do
        cmd = api + "?collection=#{@cn}&createCollection=false&type=array"
        body =  "[ { \"foo\" : true } ]";
        doc = ArangoDB.log_post("#{prefix}-create", cmd, :body => body)

        doc.code.should eq(404)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(1203)
      end
    end

################################################################################
## import a JSON array of documents 
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

      it "JSONA using different documents" do
        cmd = api + "?collection=#{@cn}&createCollection=true&type=array"
        body =  "[ \n"
        body += "{ \"name\" : \"John\", \"age\" : 29, \"gender\" : \"male\", \"likes\" : [ \"socket\", \"bind\", \"accept\" ] },\n"
        body += "{ \"firstName\" : \"Jane\", \"lastName\" : \"Doe\", \"age\" : 35, \"gender\" : \"female\", \"livesIn\" : \"Manila\" },\n"
        body += "{ \"sample\" : \"garbage\" },\n"
        body += "{ }\n"
        body += "]\n"
        doc = ArangoDB.log_post("#{prefix}-array-different", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['created'].should eq(4)
        doc.parsed_response['errors'].should eq(0)
        doc.parsed_response['empty'].should eq(0)
        doc.parsed_response['updated'].should eq(0)
        doc.parsed_response['ignored'].should eq(0)
      end
      
      it "JSONA using different documents, type=auto" do
        cmd = api + "?collection=#{@cn}&createCollection=true&type=auto"
        body =  "[ \n"
        body += "{ \"name\" : \"John\", \"age\" : 29, \"gender\" : \"male\", \"likes\" : [ \"socket\", \"bind\", \"accept\" ] },\n"
        body += "{ \"firstName\" : \"Jane\", \"lastName\" : \"Doe\", \"age\" : 35, \"gender\" : \"female\", \"livesIn\" : \"Manila\" },\n"
        body += "{ \"sample\" : \"garbage\" },\n"
        body += "{ }\n"
        body += "]\n"
        doc = ArangoDB.log_post("#{prefix}-array-different", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['created'].should eq(4)
        doc.parsed_response['errors'].should eq(0)
        doc.parsed_response['empty'].should eq(0)
        doc.parsed_response['updated'].should eq(0)
        doc.parsed_response['ignored'].should eq(0)
      end
      
      it "JSONA using whitespace" do
        cmd = api + "?collection=#{@cn}&createCollection=true&type=array"
        body =  " [\n\n      { \"name\" : \"John\", \"age\" : 29 },      \n     \n \n \r\n \n { \"rubbish\" : \"data goes in here\" }\n\n ]"
        doc = ArangoDB.log_post("#{prefix}-array-ws", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['created'].should eq(2)
        doc.parsed_response['errors'].should eq(0)
        doc.parsed_response['empty'].should eq(0)
        doc.parsed_response['updated'].should eq(0)
        doc.parsed_response['ignored'].should eq(0)
      end
      
      it "JSONA invalid documents" do
        cmd = api + "?collection=#{@cn}&createCollection=true&type=array"
        body =  "[ { \"this doc\" : \"isValid\" },\n{ \"this one\" : is not },\n{ \"again\" : \"this is ok\" },\n\n{ \"but this isn't\" }\n ]"
        doc = ArangoDB.log_post("#{prefix}-array-invalid", cmd, :body => body)

        doc.code.should eq(400)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(400)
      end
      
      it "JSONA empty body" do
        cmd = api + "?collection=#{@cn}&createCollection=true&type=array"
        body =  "" 
        doc = ArangoDB.log_post("#{prefix}-array-empty", cmd, :body => body)

        doc.code.should eq(400)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(400)
      end
      
      it "JSONA no documents" do
        cmd = api + "?collection=#{@cn}&createCollection=true&type=array"
        body =  "[\n\n]"
        doc = ArangoDB.log_post("#{prefix}-array-none", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['created'].should eq(0)
        doc.parsed_response['errors'].should eq(0)
        doc.parsed_response['empty'].should eq(0)
        doc.parsed_response['updated'].should eq(0)
        doc.parsed_response['ignored'].should eq(0)
      end
      
      it "JSONA no collection" do
        cmd = api + "?type=array"
        body =  "[ \n\n ]"
        doc = ArangoDB.log_post("#{prefix}-array-nocoll", cmd, :body => body)

        doc.code.should eq(400)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(1204)
      end
      
      it "JSONA non-existing collection" do
        ArangoDB.drop_collection(@cn)

        cmd = api + "?collection=#{@cn}&type=array"
        body = "[ { \"sample\" : \"garbage\" } ]"
        doc = ArangoDB.log_post("#{prefix}-array-nonexist", cmd, :body => body)
      
        doc.code.should eq(404)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(1203)
      end

    end

################################################################################
## import self-contained documents (one doc per line)
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

      it "JSONL using different documents" do
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
        doc.parsed_response['updated'].should eq(0)
        doc.parsed_response['ignored'].should eq(0)
      end
      
      it "JSONL using different documents, type=auto" do
        cmd = api + "?collection=#{@cn}&createCollection=true&type=auto"
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
        doc.parsed_response['updated'].should eq(0)
        doc.parsed_response['ignored'].should eq(0)
      end
      
      it "JSONL using whitespace" do
        cmd = api + "?collection=#{@cn}&createCollection=true&type=documents"
        body =  "\n\n      { \"name\" : \"John\", \"age\" : 29 }      \n     \n \n \r\n \n { \"rubbish\" : \"data goes in here\" }\n\n"
        doc = ArangoDB.log_post("#{prefix}-self-contained-ws", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['created'].should eq(2)
        doc.parsed_response['errors'].should eq(0)
        doc.parsed_response['empty'].should eq(7)
        doc.parsed_response['updated'].should eq(0)
        doc.parsed_response['ignored'].should eq(0)
      end
      
      it "JSONL invalid documents" do
        cmd = api + "?collection=#{@cn}&createCollection=true&type=documents"
        body =  "{ \"this doc\" : \"isValid\" }\n{ \"this one\" : is not }\n{ \"again\" : \"this is ok\" }\n\n{ \"but this isn't\" }\n"
        doc = ArangoDB.log_post("#{prefix}-self-contained-invalid", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['created'].should eq(2)
        doc.parsed_response['errors'].should eq(2)
        doc.parsed_response['empty'].should eq(1)
        doc.parsed_response['updated'].should eq(0)
        doc.parsed_response['ignored'].should eq(0)
      end
      
      it "JSONL empty body" do
        cmd = api + "?collection=#{@cn}&createCollection=true&type=documents"
        body =  "" 
        doc = ArangoDB.log_post("#{prefix}-self-contained-empty", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['created'].should eq(0)
        doc.parsed_response['errors'].should eq(0)
        doc.parsed_response['empty'].should eq(0)
        doc.parsed_response['updated'].should eq(0)
        doc.parsed_response['ignored'].should eq(0)
      end
      
      it "JSONL no documents" do
        cmd = api + "?collection=#{@cn}&createCollection=true&type=documents"
        body =  "\n\n"
        doc = ArangoDB.log_post("#{prefix}-self-contained-none", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['created'].should eq(0)
        doc.parsed_response['errors'].should eq(0)
        doc.parsed_response['empty'].should eq(2)
        doc.parsed_response['updated'].should eq(0)
        doc.parsed_response['ignored'].should eq(0)
      end
      
      it "JSONL no collection" do
        cmd = api + "?type=documents"
        body =  "\n\n"
        doc = ArangoDB.log_post("#{prefix}-self-contained-nocoll", cmd, :body => body)

        doc.code.should eq(400)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(1204)
      end
      
      it "JSONL non-existing collection" do
        ArangoDB.drop_collection(@cn)

        cmd = api + "?collection=#{@cn}&type=documents"
        body = "{ \"sample\" : \"garbage\" }"
        doc = ArangoDB.log_post("#{prefix}-self-contained-nonexist", cmd, :body => body)
      
        doc.code.should eq(404)
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
        doc.parsed_response['updated'].should eq(0)
        doc.parsed_response['ignored'].should eq(0)
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
        doc.parsed_response['updated'].should eq(0)
        doc.parsed_response['ignored'].should eq(0)
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
        doc.parsed_response['updated'].should eq(0)
        doc.parsed_response['ignored'].should eq(0)
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
        doc.parsed_response['updated'].should eq(0)
        doc.parsed_response['ignored'].should eq(0)
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
      
        doc.code.should eq(404)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(1203)
      end

    end

################################################################################
## import documents into an edge collection, using a JSON array
################################################################################

    context "import into an edge collection, using a JSON array:" do
      before do
        @vn = "UnitTestsImportVertex"
        @en = "UnitTestsImportEdge"
        ArangoDB.drop_collection(@vn)
        ArangoDB.drop_collection(@en)
        
        ArangoDB.create_collection(@vn, false, 2)
        ArangoDB.create_collection(@en, false, 3)
        
        body = ""
        (0..50).each do |i|
          body += "{ \"_key\" : \"vertex" + i.to_s + "\", \"value\" : " + i.to_s + " }\n" 
        end

        ArangoDB.post("/_api/import?collection=" + @vn + "&type=documents", :body => body)
      end

      after do
        ArangoDB.drop_collection(@vn)
        ArangoDB.drop_collection(@en)
      end

      it "JSONA no _from" do
        cmd = api + "?collection=#{@en}&createCollection=false&type=array"
        body = "[ { \"_to\" : \"" + @vn + "/vertex1\" } ]";
        doc = ArangoDB.log_post("#{prefix}-edge-json-nofrom", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['created'].should eq(0)
        doc.parsed_response['errors'].should eq(1)
        doc.parsed_response['empty'].should eq(0)
        doc.parsed_response['updated'].should eq(0)
        doc.parsed_response['ignored'].should eq(0)
      end

      it "JSONA no _to" do
        cmd = api + "?collection=#{@en}&createCollection=false&type=array"
        body = "[ { \"_from\" : \"" + @vn + "/vertex1\" } ]";
        doc = ArangoDB.log_post("#{prefix}-edge-json-noto", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['created'].should eq(0)
        doc.parsed_response['errors'].should eq(1)
        doc.parsed_response['empty'].should eq(0)
        doc.parsed_response['updated'].should eq(0)
        doc.parsed_response['ignored'].should eq(0)
      end

      it "JSONA with _from and _to" do
        cmd = api + "?collection=#{@en}&createCollection=false&type=array"
        body = "[ { \"_from\" : \"" + @vn + "/vertex1\", \"_to\" : \"" + @vn + "/vertex2\" } ]"
        doc = ArangoDB.log_post("#{prefix}-edge-json-fromto", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['created'].should eq(1)
        doc.parsed_response['errors'].should eq(0)
        doc.parsed_response['empty'].should eq(0)
        doc.parsed_response['updated'].should eq(0)
        doc.parsed_response['ignored'].should eq(0)
      end

      it "JSONA multiple docs, with _from and _to" do
        cmd = api + "?collection=#{@en}&createCollection=false&type=array"
        body = "[\n"
        body += "{ \"a\" : 1, \"_from\" : \"" + @vn + "/vertex1\", \"_to\" : \"" + @vn + "/vertex2\" },\n"
        body += "{ \"foo\" : true, \"bar\": \"baz\", \"_from\" : \"" + @vn + "/vertex1\", \"_to\" : \"" + @vn + "/vertex2\" },\n"
        body += "{ \"from\" : \"" + @vn + "/vertex1\", \"to\" : \"" + @vn + "/vertex2\" }\n"
        body += "]";
        doc = ArangoDB.log_post("#{prefix}-edge-json-fromto", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['created'].should eq(2)
        doc.parsed_response['errors'].should eq(1)
        doc.parsed_response['empty'].should eq(0)
        doc.parsed_response['updated'].should eq(0)
        doc.parsed_response['ignored'].should eq(0)
      end

      it "JSONA multiple docs, with wrong fromPrefix" do
        cmd = api + "?collection=#{@en}&createCollection=false&type=array&fromPrefix=a+b"
        body = "[\n"
        body += "{ \"a\" : 1, \"_from\" : \"vertex1\", \"_to\" : \"" + @vn + "/vertex2\" }\n"
        body += "]";
        doc = ArangoDB.log_post("#{prefix}-edge-json-fromto", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['created'].should eq(0)
        doc.parsed_response['errors'].should eq(1)
        doc.parsed_response['empty'].should eq(0)
        doc.parsed_response['updated'].should eq(0)
        doc.parsed_response['ignored'].should eq(0)
      end
      
      it "JSONA multiple docs, with wrong toPrefix" do
        cmd = api + "?collection=#{@en}&createCollection=false&type=array&toPrefix=a+b"
        body = "[\n"
        body += "{ \"a\" : 1, \"_to\" : \"vertex1\", \"_from\" : \"" + @vn + "/vertex2\" }\n"
        body += "]";
        doc = ArangoDB.log_post("#{prefix}-edge-json-fromto", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['created'].should eq(0)
        doc.parsed_response['errors'].should eq(1)
        doc.parsed_response['empty'].should eq(0)
        doc.parsed_response['updated'].should eq(0)
        doc.parsed_response['ignored'].should eq(0)
      end
      
      it "JSONA multiple docs, with fromPrefix" do
        cmd = api + "?collection=#{@en}&createCollection=false&type=array&fromPrefix=#{@vn}"
        body = "[\n"
        body += "{ \"a\" : 1, \"_from\" : \"vertex1\", \"_to\" : \"" + @vn + "/vertex2\" },\n"
        body += "{ \"foo\" : true, \"bar\": \"baz\", \"_from\" : \"vertex1\", \"_to\" : \"" + @vn + "/vertex2\" },\n"
        body += "{ \"from\" : \"vertex1\", \"to\" : \"" + @vn + "/vertex2\" }\n"
        body += "]";
        doc = ArangoDB.log_post("#{prefix}-edge-json-fromto", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['created'].should eq(2)
        doc.parsed_response['errors'].should eq(1)
        doc.parsed_response['empty'].should eq(0)
        doc.parsed_response['updated'].should eq(0)
        doc.parsed_response['ignored'].should eq(0)
      end
      
      it "JSONA multiple docs, with toPrefix" do
        cmd = api + "?collection=#{@en}&createCollection=false&type=array&toPrefix=#{@vn}"
        body = "[\n"
        body += "{ \"a\" : 1, \"_to\" : \"vertex1\", \"_from\" : \"" + @vn + "/vertex2\" },\n"
        body += "{ \"foo\" : true, \"bar\": \"baz\", \"_to\" : \"vertex1\", \"_from\" : \"" + @vn + "/vertex2\" },\n"
        body += "{ \"to\" : \"vertex1\", \"from\" : \"" + @vn + "/vertex2\" }\n"
        body += "]";
        doc = ArangoDB.log_post("#{prefix}-edge-json-fromto", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['created'].should eq(2)
        doc.parsed_response['errors'].should eq(1)
        doc.parsed_response['empty'].should eq(0)
        doc.parsed_response['updated'].should eq(0)
        doc.parsed_response['ignored'].should eq(0)
      end
      
      it "JSONA multiple docs, with fromPrefix and toPrefix" do
        cmd = api + "?collection=#{@en}&createCollection=false&type=array&fromPrefix=#{@vn}&toPrefix=#{@vn}"
        body = "[\n"
        body += "{ \"a\" : 1, \"_to\" : \"vertex1\", \"_from\" : \"vertex2\" },\n"
        body += "{ \"foo\" : true, \"bar\": \"baz\", \"_to\" : \"vertex1\", \"_from\" : \"vertex2\" },\n"
        body += "{ \"to\" : \"vertex1\", \"from\" : \"vertex2\" }\n"
        body += "]";
        doc = ArangoDB.log_post("#{prefix}-edge-json-fromto", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['created'].should eq(2)
        doc.parsed_response['errors'].should eq(1)
        doc.parsed_response['empty'].should eq(0)
        doc.parsed_response['updated'].should eq(0)
        doc.parsed_response['ignored'].should eq(0)
      end
      
      it "JSONA multiple docs, with fromPrefix and toPrefix, double prefixing" do
        cmd = api + "?collection=#{@en}&createCollection=false&type=array&fromPrefix=#{@vn}&toPrefix=#{@vn}"
        body = "[\n"
        body += "{ \"a\" : 1, \"_to\" : \"#{@vn}/vertex1\", \"_from\" : \"#{@vn}/vertex2\" },\n"
        body += "{ \"foo\" : true, \"bar\": \"baz\", \"_to\" : \"#{@vn}/vertex1\", \"_from\" : \"#{@vn}vertex2\" },\n"
        body += "{ \"to\" : \"#{@vn}/vertex1\", \"from\" : \"#{@vn}/vertex2\" }\n"
        body += "]";
        doc = ArangoDB.log_post("#{prefix}-edge-json-fromto", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['created'].should eq(2)
        doc.parsed_response['errors'].should eq(1)
        doc.parsed_response['empty'].should eq(0)
        doc.parsed_response['updated'].should eq(0)
        doc.parsed_response['ignored'].should eq(0)
      end
    end

################################################################################
## import documents into an edge collection, using individual documents
################################################################################

    context "import into an edge collection, using individual docs:" do
      before do
        @vn = "UnitTestsImportVertex"
        @en = "UnitTestsImportEdge"
        ArangoDB.drop_collection(@vn)
        ArangoDB.drop_collection(@en)
        
        ArangoDB.create_collection(@vn, false, 2)
        ArangoDB.create_collection(@en, false, 3)
        
        body = ""
        (0..50).each do |i|
          body += "{ \"_key\" : \"vertex" + i.to_s + "\", \"value\" : " + i.to_s + " }\n" 
        end

        ArangoDB.post("/_api/import?collection=" + @vn + "&type=documents", :body => body)
      end

      after do
        ArangoDB.drop_collection(@vn)
        ArangoDB.drop_collection(@en)
      end

      it "no _from" do
        cmd = api + "?collection=#{@en}&createCollection=false&type=documents"
        body = "{ \"_to\" : \"" + @vn + "/vertex1\" }";
        doc = ArangoDB.log_post("#{prefix}-edge-documents-nofrom", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['created'].should eq(0)
        doc.parsed_response['errors'].should eq(1)
        doc.parsed_response['empty'].should eq(0)
        doc.parsed_response['updated'].should eq(0)
        doc.parsed_response['ignored'].should eq(0)
      end

      it "no _to" do
        cmd = api + "?collection=#{@en}&createCollection=false&type=documents"
        body = "{ \"_from\" : \"" + @vn + "/vertex1\" }"
        doc = ArangoDB.log_post("#{prefix}-edge-documents-noto", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['created'].should eq(0)
        doc.parsed_response['errors'].should eq(1)
        doc.parsed_response['empty'].should eq(0)
        doc.parsed_response['updated'].should eq(0)
        doc.parsed_response['ignored'].should eq(0)
      end

      it "with _from and _to" do
        cmd = api + "?collection=#{@en}&createCollection=false&type=documents"
        body = "{ \"_from\" : \"" + @vn + "/vertex1\", \"_to\" : \"" + @vn + "/vertex2\" }"
        doc = ArangoDB.log_post("#{prefix}-edge-documents-fromto", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['created'].should eq(1)
        doc.parsed_response['errors'].should eq(0)
        doc.parsed_response['empty'].should eq(0)
        doc.parsed_response['updated'].should eq(0)
        doc.parsed_response['ignored'].should eq(0)
      end

      it "multiple documents, with _from and _to" do
        cmd = api + "?collection=#{@en}&createCollection=false&type=documents"
        body  = "{ \"a\" : 1, \"_from\" : \"" + @vn + "/vertex1\", \"_to\" : \"" + @vn + "/vertex2\" }\n";
        body += "{ \"foo\" : true, \"bar\": \"baz\", \"_from\" : \"" + @vn + "/vertex1\", \"_to\" : \"" + @vn + "/vertex2\" }\n"
        body += "{ \"from\" : \"" + @vn + "/vertex1\", \"to\" : \"" + @vn + "/vertex2\" }\n"
        doc = ArangoDB.log_post("#{prefix}-edge-documents-fromto", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['created'].should eq(2)
        doc.parsed_response['errors'].should eq(1)
        doc.parsed_response['empty'].should eq(0)
        doc.parsed_response['updated'].should eq(0)
        doc.parsed_response['ignored'].should eq(0)
      end

    end

################################################################################
## import documents into an edge collection, using CSV
################################################################################

    context "import into an edge collection, using CSV:" do
      before do
        @vn = "UnitTestsImportVertex"
        @en = "UnitTestsImportEdge"
        ArangoDB.drop_collection(@vn)
        ArangoDB.drop_collection(@en)
        
        ArangoDB.create_collection(@vn, false, 2)
        ArangoDB.create_collection(@en, false, 3)
        
        body = ""
        (0..50).each do |i|
          body += "{ \"_key\" : \"vertex" + i.to_s + "\", \"value\" : " + i.to_s + " }\n" 
        end

        ArangoDB.post("/_api/import?collection=" + @vn + "&type=documents", :body => body)
      end

      after do
        ArangoDB.drop_collection(@vn)
        ArangoDB.drop_collection(@en)
      end

      it "CSV no _from" do
        cmd = api + "?collection=#{@en}&createCollection=false"
        body  = "[ \"_to\" ]\n"
        body += "[ \"" + @vn + "/vertex1\" ]";
        doc = ArangoDB.log_post("#{prefix}-edge-list-nofrom", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['created'].should eq(0)
        doc.parsed_response['errors'].should eq(1)
        doc.parsed_response['empty'].should eq(0)
        doc.parsed_response['updated'].should eq(0)
        doc.parsed_response['ignored'].should eq(0)
      end

      it "CSV no _to" do
        cmd = api + "?collection=#{@en}&createCollection=false"
        body  = "[ \"_from\" ]\n"
        body += "[ \"" + @vn + "/vertex1\" ]"
        doc = ArangoDB.log_post("#{prefix}-edge-list-noto", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['created'].should eq(0)
        doc.parsed_response['errors'].should eq(1)
        doc.parsed_response['empty'].should eq(0)
        doc.parsed_response['updated'].should eq(0)
        doc.parsed_response['ignored'].should eq(0)
      end

      it "CSV with _from and _to" do
        cmd = api + "?collection=#{@en}&createCollection=false"
        body  = "[ \"_from\", \"_to\" ]\n"
        body += "[ \"" + @vn + "/vertex1\", \"" + @vn + "/vertex2\" ]"
        doc = ArangoDB.log_post("#{prefix}-edge-list-fromto", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['created'].should eq(1)
        doc.parsed_response['errors'].should eq(0)
        doc.parsed_response['empty'].should eq(0)
        doc.parsed_response['updated'].should eq(0)
        doc.parsed_response['ignored'].should eq(0)
      end

      it "CSV multiple documents, with _from and _to" do
        cmd = api + "?collection=#{@en}&createCollection=false"
        body  = "[ \"a\", \"_from\", \"_to\", \"foo\" ]\n"
        body += "[ 1, \"" + @vn + "/vertex1\", \"" + @vn + "/vertex2\", \"\" ]\n"
        body += "\n"
        body += "[ 2, \"" + @vn + "/vertex3\", \"" + @vn + "/vertex4\", \"\" ]\n"
        body += "[ 2, 1, 2, 3 ]"
        doc = ArangoDB.log_post("#{prefix}-edge-list-fromto", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['created'].should eq(2)
        doc.parsed_response['errors'].should eq(1)
        doc.parsed_response['empty'].should eq(1)
        doc.parsed_response['updated'].should eq(0)
        doc.parsed_response['ignored'].should eq(0)
      end

    end

################################################################################
## import with update
################################################################################

    context "import with update:" do
      before do
        @cn = "UnitTestsImport"
        ArangoDB.drop_collection(@cn)
        @cid = ArangoDB.create_collection(@cn, false)
        
        cmd = api + "?collection=#{@cn}&type=documents"
        body =  "{ \"_key\" : \"test1\", \"value1\" : 1, \"value2\" : \"test\" }\n"
        body += "{ \"_key\" : \"test2\", \"value1\" : \"abc\", \"value2\" : 3 }\n"
        ArangoDB.post(cmd, :body => body)
      end

      after do
        ArangoDB.drop_collection(@cn)
      end
      
      it "using onDuplicate=error" do
        cmd = api + "?collection=#{@cn}&type=documents&onDuplicate=error"
        body =  "{ \"_key\" : \"test1\", \"value3\" : 3, \"value2\" : \"test-updated\" }\n"
        body += "{ \"_key\" : \"test2\" }\n"
        body += "{ \"_key\" : \"test3\", \"foo\" : \"bar\" }\n"
        doc = ArangoDB.log_post("#{prefix}-update", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['created'].should eq(1)
        doc.parsed_response['errors'].should eq(2)
        doc.parsed_response['empty'].should eq(0)
        doc.parsed_response['ignored'].should eq(0)
        doc.parsed_response['updated'].should eq(0)

        doc = ArangoDB.get("/_api/document/#{@cn}/test1")
        doc.code.should eq(200)
        doc.parsed_response['_key'].should eq('test1')
        doc.parsed_response['value1'].should eq(1)
        doc.parsed_response['value2'].should eq('test')
        doc.parsed_response.should_not have_key('value3')
        
        doc = ArangoDB.get("/_api/document/#{@cn}/test2")
        doc.code.should eq(200)
        doc.parsed_response['_key'].should eq('test2')
        doc.parsed_response['value1'].should eq('abc') 
        doc.parsed_response['value2'].should eq(3)
        
        doc = ArangoDB.get("/_api/document/#{@cn}/test3")
        doc.code.should eq(200)
        doc.parsed_response['_key'].should eq('test3')
        doc.parsed_response['foo'].should eq('bar')
      end
      
      it "using onDuplicate=error, without _key" do
        cmd = api + "?collection=#{@cn}&type=documents&onDuplicate=error"
        body =  "{ \"value3\" : 3, \"value2\" : \"test-updated\" }\n"
        body += "{ }\n"
        body += "{ \"foo\" : \"bar\" }\n"
        doc = ArangoDB.log_post("#{prefix}-update", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['created'].should eq(3)
        doc.parsed_response['errors'].should eq(0)
        doc.parsed_response['empty'].should eq(0)
        doc.parsed_response['ignored'].should eq(0)
        doc.parsed_response['updated'].should eq(0)
      end
      
      it "using onDuplicate=error, mixed _keys" do
        cmd = api + "?collection=#{@cn}&type=documents&onDuplicate=error"
        body =  "{ \"_key\" : \"test1\", \"value3\" : 3, \"value2\" : \"test-updated\" }\n"
        body += "{ \"foo\" : \"bar\" }\n"
        doc = ArangoDB.log_post("#{prefix}-update", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['created'].should eq(1)
        doc.parsed_response['errors'].should eq(1)
        doc.parsed_response['empty'].should eq(0)
        doc.parsed_response['ignored'].should eq(0)
        doc.parsed_response['updated'].should eq(0)

        doc = ArangoDB.get("/_api/document/#{@cn}/test1")
        doc.code.should eq(200)
        doc.parsed_response['_key'].should eq('test1')
        doc.parsed_response['value1'].should eq(1)
        doc.parsed_response['value2'].should eq('test')
        doc.parsed_response.should_not have_key('value3')
      end

      it "using onDuplicate=update" do
        cmd = api + "?collection=#{@cn}&type=documents&onDuplicate=update"
        body =  "{ \"_key\" : \"test1\", \"value3\" : 3, \"value2\" : \"test-updated\" }\n"
        body += "{ \"_key\" : \"test2\" }\n"
        body += "{ \"_key\" : \"test3\", \"foo\" : \"bar\" }\n"
        doc = ArangoDB.log_post("#{prefix}-update", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['created'].should eq(1)
        doc.parsed_response['errors'].should eq(0)
        doc.parsed_response['empty'].should eq(0)
        doc.parsed_response['ignored'].should eq(0)
        doc.parsed_response['updated'].should eq(2)
        
        doc = ArangoDB.get("/_api/document/#{@cn}/test1")
        doc.code.should eq(200)
        doc.parsed_response['_key'].should eq('test1')
        doc.parsed_response['value1'].should eq(1)
        doc.parsed_response['value2'].should eq('test-updated')
        doc.parsed_response['value3'].should eq(3)
        
        doc = ArangoDB.get("/_api/document/#{@cn}/test2")
        doc.code.should eq(200)
        doc.parsed_response['_key'].should eq('test2')
        doc.parsed_response['value1'].should eq('abc') 
        doc.parsed_response['value2'].should eq(3)
        
        doc = ArangoDB.get("/_api/document/#{@cn}/test3")
        doc.code.should eq(200)
        doc.parsed_response['_key'].should eq('test3')
        doc.parsed_response['foo'].should eq('bar')
      end
      
      it "using onDuplicate=update, without _key" do
        cmd = api + "?collection=#{@cn}&type=documents&onDuplicate=update"
        body =  "{ \"value3\" : 3, \"value2\" : \"test-updated\" }\n"
        body += "{ }\n"
        body += "{ \"foo\" : \"bar\" }\n"
        doc = ArangoDB.log_post("#{prefix}-update", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['created'].should eq(3)
        doc.parsed_response['errors'].should eq(0)
        doc.parsed_response['empty'].should eq(0)
        doc.parsed_response['ignored'].should eq(0)
        doc.parsed_response['updated'].should eq(0)
      end
      
      it "using onDuplicate=update, mixed _keys" do
        cmd = api + "?collection=#{@cn}&type=documents&onDuplicate=update"
        body =  "{ \"_key\" : \"test1\", \"value3\" : 3, \"value2\" : \"test-updated\" }\n"
        body += "{ \"foo\" : \"bar\" }\n"
        doc = ArangoDB.log_post("#{prefix}-update", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['created'].should eq(1)
        doc.parsed_response['errors'].should eq(0)
        doc.parsed_response['empty'].should eq(0)
        doc.parsed_response['ignored'].should eq(0)
        doc.parsed_response['updated'].should eq(1)
        
        doc = ArangoDB.get("/_api/document/#{@cn}/test1")
        doc.code.should eq(200)
        doc.parsed_response['_key'].should eq('test1')
        doc.parsed_response['value1'].should eq(1)
        doc.parsed_response['value2'].should eq('test-updated')
        doc.parsed_response['value3'].should eq(3)
      end
      
      it "using onDuplicate=replace" do
        cmd = api + "?collection=#{@cn}&type=documents&onDuplicate=replace"
        body =  "{ \"_key\" : \"test1\", \"value3\" : 3, \"value2\" : \"test-updated\" }\n"
        body += "{ \"_key\" : \"test2\" }\n"
        body += "{ \"_key\" : \"test3\", \"foo\" : \"bar\" }\n"
        doc = ArangoDB.log_post("#{prefix}-update", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['created'].should eq(1)
        doc.parsed_response['errors'].should eq(0)
        doc.parsed_response['empty'].should eq(0)
        doc.parsed_response['ignored'].should eq(0)
        doc.parsed_response['updated'].should eq(2)

        doc = ArangoDB.get("/_api/document/#{@cn}/test1")
        doc.code.should eq(200)
        doc.parsed_response['_key'].should eq('test1') 
        doc.parsed_response.should_not have_key('value1')
        doc.parsed_response['value2'].should eq('test-updated')
        doc.parsed_response['value3'].should eq(3) 
        
        doc = ArangoDB.get("/_api/document/#{@cn}/test2")
        doc.code.should eq(200)
        doc.parsed_response['_key'].should eq('test2') 
        doc.parsed_response.should_not have_key('value1')
        doc.parsed_response.should_not have_key('value2')
        
        doc = ArangoDB.get("/_api/document/#{@cn}/test3")
        doc.code.should eq(200)
        doc.parsed_response['_key'].should eq('test3')
        doc.parsed_response['foo'].should eq('bar')
      end
      
      it "using onDuplicate=replace, without _key" do
        cmd = api + "?collection=#{@cn}&type=documents&onDuplicate=replace"
        body =  "{ \"value3\" : 3, \"value2\" : \"test-updated\" }\n"
        body += "{ }\n"
        body += "{ \"foo\" : \"bar\" }\n"
        doc = ArangoDB.log_post("#{prefix}-update", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['created'].should eq(3)
        doc.parsed_response['errors'].should eq(0)
        doc.parsed_response['empty'].should eq(0)
        doc.parsed_response['ignored'].should eq(0)
        doc.parsed_response['updated'].should eq(0)
      end
      
      it "using onDuplicate=replace, mixed _keys" do
        cmd = api + "?collection=#{@cn}&type=documents&onDuplicate=replace"
        body =  "{ \"_key\" : \"test1\", \"value3\" : 3, \"value2\" : \"test-updated\" }\n"
        body += "{ \"foo\" : \"bar\" }\n"
        doc = ArangoDB.log_post("#{prefix}-update", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['created'].should eq(1)
        doc.parsed_response['errors'].should eq(0)
        doc.parsed_response['empty'].should eq(0)
        doc.parsed_response['ignored'].should eq(0)
        doc.parsed_response['updated'].should eq(1)

        doc = ArangoDB.get("/_api/document/#{@cn}/test1")
        doc.code.should eq(200)
        doc.parsed_response['_key'].should eq('test1') 
        doc.parsed_response.should_not have_key('value1')
        doc.parsed_response['value2'].should eq('test-updated')
        doc.parsed_response['value3'].should eq(3) 
      end

      it "using onDuplicate=ignore" do
        cmd = api + "?collection=#{@cn}&type=documents&onDuplicate=ignore"
        body =  "{ \"_key\" : \"test1\", \"value3\" : 3, \"value2\" : \"test\" }\n"
        body += "{ \"_key\" : \"test2\" }\n"
        body += "{ \"_key\" : \"test3\", \"foo\" : \"bar\" }\n"
        doc = ArangoDB.log_post("#{prefix}-update", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['created'].should eq(1)
        doc.parsed_response['errors'].should eq(0)
        doc.parsed_response['empty'].should eq(0)
        doc.parsed_response['ignored'].should eq(2)
        doc.parsed_response['updated'].should eq(0)

        doc = ArangoDB.get("/_api/document/#{@cn}/test1")
        doc.code.should eq(200)
        doc.parsed_response['_key'].should eq('test1')
        doc.parsed_response['value1'].should eq(1)
        doc.parsed_response['value2'].should eq('test')
        doc.parsed_response.should_not have_key('value3')
        
        doc = ArangoDB.get("/_api/document/#{@cn}/test2")
        doc.code.should eq(200)
        doc.parsed_response['_key'].should eq('test2')
        doc.parsed_response['value1'].should eq('abc') 
        doc.parsed_response['value2'].should eq(3)
        
        doc = ArangoDB.get("/_api/document/#{@cn}/test3")
        doc.code.should eq(200)
        doc.parsed_response['_key'].should eq('test3')
        doc.parsed_response['foo'].should eq('bar')
      end
      
      it "using onDuplicate=ignore, without _key" do
        cmd = api + "?collection=#{@cn}&type=documents&onDuplicate=ignore"
        body =  "{ \"value3\" : 3, \"value2\" : \"test\" }\n"
        body += "{ }\n"
        body += "{ \"foo\" : \"bar\" }\n"
        doc = ArangoDB.log_post("#{prefix}-update", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['created'].should eq(3)
        doc.parsed_response['errors'].should eq(0)
        doc.parsed_response['empty'].should eq(0)
        doc.parsed_response['ignored'].should eq(0)
        doc.parsed_response['updated'].should eq(0)
      end
      
      it "using onDuplicate=ignore, mixed _keys" do
        cmd = api + "?collection=#{@cn}&type=documents&onDuplicate=ignore"
        body =  "{ \"_key\" : \"test1\", \"value3\" : 3, \"value2\" : \"test\" }\n"
        body += "{ \"foo\" : \"bar\" }\n"
        doc = ArangoDB.log_post("#{prefix}-update", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['created'].should eq(1)
        doc.parsed_response['errors'].should eq(0)
        doc.parsed_response['empty'].should eq(0)
        doc.parsed_response['ignored'].should eq(1)
        doc.parsed_response['updated'].should eq(0)

        doc = ArangoDB.get("/_api/document/#{@cn}/test1")
        doc.code.should eq(200)
        doc.parsed_response['_key'].should eq('test1')
        doc.parsed_response['value1'].should eq(1)
        doc.parsed_response['value2'].should eq('test')
        doc.parsed_response.should_not have_key('value3')
      end
    end

  end
end
