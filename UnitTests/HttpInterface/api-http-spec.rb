# coding: utf-8

require 'rspec'
require './arangodb.rb'

describe ArangoDB do
  prefix = "api-http"

  context "dealing with HTTP methods:" do

################################################################################
## checking invalid body sizes
################################################################################

    context "invalid sizes of body:" do
      it "checks negative content-length" do
        cmd = "/_api/version"
        doc = ArangoDB.log_post("#{prefix}-content-length", cmd, { :headers => { "Content-Length" => "-1" } })

        doc.code.should eq(411)
        doc.response.body.should eq("")
      end
      
      it "checks too big content-length" do
        cmd = "/_api/version"
        doc = ArangoDB.log_post("#{prefix}-content-length", cmd, { :headers => { "Content-Length" => "9999999999" } })

        doc.code.should eq(413)
        doc.response.body.should eq("")
      end

    end

################################################################################
## checking HTTP HEAD responses
################################################################################

    context "head requests:" do
      it "checks whether HEAD returns a body on 2xx" do
        cmd = "/_api/version"
        doc = ArangoDB.log_head("#{prefix}-head-supported-method", cmd)

        doc.code.should eq(200)
        doc.response.body.should be_nil
      end
      
      it "checks whether HEAD returns a body on 3xx" do
        cmd = "/_api/collection"
        doc = ArangoDB.log_head("#{prefix}-head-unsupported-method1", cmd)

        doc.code.should eq(405)
        doc.response.body.should be_nil
      end

      it "checks whether HEAD returns a body on 4xx" do
        cmd = "/_api/cursor"
        doc = ArangoDB.log_head("#{prefix}-head-unsupported-method2", cmd)

        doc.code.should eq(405)
        doc.response.body.should be_nil
      end
      
      it "checks whether HEAD returns a body on 4xx" do
        cmd = "/_api/non-existing-method"
        doc = ArangoDB.log_head("#{prefix}-head-non-existing-method", cmd)

        doc.code.should eq(404)
        doc.response.body.should be_nil
      end

      it "checks whether HEAD returns a body on an existing document" do
        cn = "UnitTestsCollectionHttp"
        ArangoDB.drop_collection(cn)

        # create collection with one document
        @cid = ArangoDB.create_collection(cn)
  
        cmd = "/_api/document?collection=#{cn}"
        body = "{ \"Hello\" : \"World\" }"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)

        did = doc.parsed_response['_id']
        did.should be_kind_of(String)

        # run a HTTP HEAD query on the existing document
        cmd = "/_api/document/" + did
        doc = ArangoDB.log_head("#{prefix}-head-check-document", cmd)

        doc.code.should eq(200)
        doc.response.body.should be_nil

        # run a HTTP HEAD query on the existing document, with wrong precondition
        cmd = "/_api/document/" + did
        doc = ArangoDB.log_head("#{prefix}-head-check-documentq", cmd, :header => { :"if-match" => "1" })

        doc.code.should eq(200)
        doc.response.body.should be_nil

        ArangoDB.drop_collection(cn)
      end
    end

################################################################################
## checking HTTP GET responses
################################################################################

    context "get requests" do
      it "checks a non-existing URL" do
        cmd = "/xxxx/yyyy"
        doc = ArangoDB.log_get("#{prefix}-get-non-existing-url", cmd)

        doc.code.should eq(404)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(404)
      end

      it "checks whether GET returns a body" do
        cmd = "/_api/non-existing-method"
        doc = ArangoDB.log_get("#{prefix}-get-non-existing-method", cmd)

        doc.code.should eq(404)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(404)
        doc.parsed_response['code'].should eq(404)
      end

      it "checks whether GET returns a body" do
        cmd = "/_api/non-allowed-method"
        doc = ArangoDB.log_get("#{prefix}-get-non-allowed-method", cmd)

        doc.code.should eq(404)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(404)
        doc.parsed_response['code'].should eq(404)
      end
    end

################################################################################
## checking HTTP OPTIONS 
################################################################################

    context "options requests" do
      before do
        @headers = "DELETE, GET, HEAD, PATCH, POST, PUT"
      end

      it "checks handling of an OPTIONS request, without body" do
        cmd = "/_api/version"
        doc = ArangoDB.log_options("#{prefix}-options", cmd)
        doc.headers['allow'].should eq(@headers)

        doc.code.should eq(200)
        doc.response.body.should be_nil
      end

      it "checks handling of an OPTIONS request, with body" do
        cmd = "/_api/version"
        doc = ArangoDB.log_options("#{prefix}-options", cmd, { :body => "some stuff" })
        doc.headers['allow'].should eq(@headers)

        doc.code.should eq(200)
        doc.response.body.should be_nil
      end
    end

################################################################################
## checking CORS requests
################################################################################

    context "CORS requests" do
      before do
        @headers = "DELETE, GET, HEAD, PATCH, POST, PUT"
      end

      it "checks handling of a non-CORS GET request" do
        cmd = "/_api/version"
        doc = ArangoDB.log_get("#{prefix}-cors", cmd )

        doc.code.should eq(200)
        doc.headers['access-control-allow-origin'].should be_nil
        doc.headers['access-control-allow-methods'].should be_nil
        doc.headers['access-control-allow-credentials'].should be_nil
      end

      it "checks handling of a CORS GET request, with null origin" do
        cmd = "/_api/version"
        doc = ArangoDB.log_get("#{prefix}-cors", cmd, { :headers => { "Origin" => "null" } } )

        doc.code.should eq(200)
        doc.headers['access-control-allow-origin'].should eq("null")
        doc.headers['access-control-allow-methods'].should be_nil
        doc.headers['access-control-allow-headers'].should be_nil
        doc.headers['access-control-allow-credentials'].should eq("true")
        doc.headers['access-control-max-age'].should be_nil
      end

      it "checks handling of a CORS GET request" do
        cmd = "/_api/version"
        doc = ArangoDB.log_get("#{prefix}-cors", cmd, { :headers => { "Origin" => "http://127.0.0.1" } } )

        doc.code.should eq(200)
        doc.headers['access-control-allow-origin'].should eq("http://127.0.0.1")
        doc.headers['access-control-allow-methods'].should be_nil
        doc.headers['access-control-allow-headers'].should be_nil
        doc.headers['access-control-allow-credentials'].should eq("true")
        doc.headers['access-control-max-age'].should be_nil
      end

      it "checks handling of a CORS POST request" do
        cmd = "/_api/version"
        doc = ArangoDB.log_get("#{prefix}-cors", cmd, { :headers => { "Origin" => "http://www.some-url.com/" } } )

        doc.code.should eq(200)
        doc.headers['access-control-allow-origin'].should eq("http://www.some-url.com/")
        doc.headers['access-control-allow-methods'].should be_nil
        doc.headers['access-control-allow-headers'].should be_nil
        doc.headers['access-control-allow-credentials'].should eq("true")
        doc.headers['access-control-max-age'].should be_nil
      end

      it "checks handling of a CORS OPTIONS preflight request, no headers" do
        cmd = "/_api/version"
        doc = ArangoDB.log_options("#{prefix}-cors", cmd, { :headers => { "origin" => "http://from.here.we.come/really/really", "access-control-request-method" => "delete" } } )

        doc.code.should eq(200)
        doc.headers['access-control-allow-origin'].should eq("http://from.here.we.come/really/really")
        doc.headers['access-control-allow-methods'].should eq(@headers)
        doc.headers['access-control-allow-headers'].should be_nil
        doc.headers['access-control-allow-credentials'].should eq("true")
        doc.headers['access-control-max-age'].should eq("1800")
        doc.headers['allow'].should eq(@headers)
        doc.headers['content-length'].should eq("0")
        doc.response.body.should be_nil
      end

      it "checks handling of a CORS OPTIONS preflight request, empty headers" do
        cmd = "/_api/version"
        doc = ArangoDB.log_options("#{prefix}-cors", cmd, { :headers => { "oRiGiN" => "HTTPS://this.is.our/site-yes", "access-control-request-method" => "delete", "access-control-request-headers" => "   " } } )

        doc.code.should eq(200)
        doc.headers['access-control-allow-origin'].should eq("HTTPS://this.is.our/site-yes")
        doc.headers['access-control-allow-methods'].should eq(@headers)
        doc.headers['access-control-allow-headers'].should be_nil
        doc.headers['access-control-allow-credentials'].should eq("true")
        doc.headers['access-control-max-age'].should eq("1800")
        doc.headers['allow'].should eq(@headers)
        doc.headers['content-length'].should eq("0")
        doc.response.body.should be_nil
      end

      it "checks handling of a CORS OPTIONS preflight request, populated headers" do
        cmd = "/_api/version"
        doc = ArangoDB.log_options("#{prefix}-cors", cmd, { :headers => { "ORIGIN" => "https://mysite.org", "Access-Control-Request-Method" => "put", "ACCESS-CONTROL-request-headers" => "foo,bar,baz" } } )

        doc.code.should eq(200)
        doc.headers['access-control-allow-origin'].should eq("https://mysite.org")
        doc.headers['access-control-allow-methods'].should eq(@headers)
        doc.headers['access-control-allow-headers'].should eq("foo,bar,baz")
        doc.headers['access-control-allow-credentials'].should eq("true")
        doc.headers['access-control-max-age'].should eq("1800")
        doc.headers['allow'].should eq(@headers)
        doc.headers['content-length'].should eq("0")
        doc.response.body.should be_nil
      end

      it "checks handling of a CORS GET request, with credentials" do
        cmd = "/_api/version"
        doc = ArangoDB.log_get("#{prefix}-cors", cmd, { :headers => { "Origin" => "http://127.0.0.1", "Access-Control-Allow-Credentials" => "true" } } )

        doc.code.should eq(200)
        doc.headers['access-control-allow-origin'].should eq("http://127.0.0.1")
        doc.headers['access-control-allow-methods'].should be_nil
        doc.headers['access-control-allow-headers'].should be_nil
        doc.headers['access-control-allow-credentials'].should eq("true")
        doc.headers['access-control-max-age'].should be_nil
      end

      it "checks handling of a CORS GET request, with credentials disabled" do
        cmd = "/_api/version"
        doc = ArangoDB.log_get("#{prefix}-cors", cmd, { :headers => { "Origin" => "http://127.0.0.1", "Access-Control-Allow-Credentials" => "false" } } )

        doc.code.should eq(200)
        doc.headers['access-control-allow-origin'].should eq("http://127.0.0.1")
        doc.headers['access-control-allow-methods'].should be_nil
        doc.headers['access-control-allow-headers'].should be_nil
        doc.headers['access-control-allow-credentials'].should eq("false")
        doc.headers['access-control-max-age'].should be_nil
      end

      it "checks handling of a CORS OPTIONS preflight request, with credentials" do
        cmd = "/_api/version"
        doc = ArangoDB.log_options("#{prefix}-cors", cmd, { :headers => { "ORIGIN" => "https://mysite.org", "Access-Control-Request-Method" => "put", "ACCESS-CONTROL-allow-credentials" => "true" } } )

        doc.code.should eq(200)
        doc.headers['access-control-allow-origin'].should eq("https://mysite.org")
        doc.headers['access-control-allow-methods'].should eq(@headers)
        doc.headers['access-control-allow-credentials'].should eq("true")
        doc.headers['access-control-max-age'].should eq("1800")
        doc.headers['allow'].should eq(@headers)
        doc.headers['content-length'].should eq("0")
        doc.response.body.should be_nil
      end

    end

  end

end
