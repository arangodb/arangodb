# coding: utf-8

require 'rspec'
require 'arangodb.rb'

describe ArangoDB do
  prefix = "rest-key"

  context "testing keys:" do

################################################################################
## error handling
################################################################################

    before do
      @cn = "UnitTestsKey"
      @cid = ArangoDB.create_collection(@cn, true)
    end

    after do
      ArangoDB.drop_collection(@cn)
    end
      
    it "returns an error if _key is null" do
      cmd = "/_api/document?collection=#{@cn}"
      body = "{ \"_key\" : null }"
      doc = ArangoDB.log_post("#{prefix}-null", cmd, :body => body)

      doc.code.should eq(400)
      doc.parsed_response['error'].should eq(true)
      doc.parsed_response['errorNum'].should eq(1221)
      doc.parsed_response['code'].should eq(400)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")
    end
  
    it "returns an error if _key is a bool" do
      cmd = "/_api/document?collection=#{@cn}"
      body = "{ \"_key\" : true }"
      doc = ArangoDB.log_post("#{prefix}-bool", cmd, :body => body)

      doc.code.should eq(400)
      doc.parsed_response['error'].should eq(true)
      doc.parsed_response['errorNum'].should eq(1221)
      doc.parsed_response['code'].should eq(400)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")
    end
    
    it "returns an error if _key is a number 1" do
      cmd = "/_api/document?collection=#{@cn}"
      body = "{ \"_key\" : 12 }"
      doc = ArangoDB.log_post("#{prefix}-number", cmd, :body => body)

      doc.code.should eq(400)
      doc.parsed_response['error'].should eq(true)
      doc.parsed_response['errorNum'].should eq(1221)
      doc.parsed_response['code'].should eq(400)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")
    end
    
    it "returns an error if _key is a number 2" do
      cmd = "/_api/document?collection=#{@cn}"
      body = "{ \"_key\" : 12.554 }"
      doc = ArangoDB.log_post("#{prefix}-number", cmd, :body => body)

      doc.code.should eq(400)
      doc.parsed_response['error'].should eq(true)
      doc.parsed_response['errorNum'].should eq(1221)
      doc.parsed_response['code'].should eq(400)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")
    end
    
    it "returns an error if _key is a list" do
      cmd = "/_api/document?collection=#{@cn}"
      body = "{ \"_key\" : [ ] }"
      doc = ArangoDB.log_post("#{prefix}-list", cmd, :body => body)

      doc.code.should eq(400)
      doc.parsed_response['error'].should eq(true)
      doc.parsed_response['errorNum'].should eq(1221)
      doc.parsed_response['code'].should eq(400)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")
    end
    
    it "returns an error if _key is an object" do
      cmd = "/_api/document?collection=#{@cn}"
      body = "{ \"_key\" : { } }"
      doc = ArangoDB.log_post("#{prefix}-object", cmd, :body => body)

      doc.code.should eq(400)
      doc.parsed_response['error'].should eq(true)
      doc.parsed_response['errorNum'].should eq(1221)
      doc.parsed_response['code'].should eq(400)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")
    end

    it "returns an error if _key is empty string" do
      cmd = "/_api/document?collection=#{@cn}"
      body = "{ \"_key\" : \"\" }"
      doc = ArangoDB.log_post("#{prefix}-empty-string", cmd, :body => body)

      doc.code.should eq(400)
      doc.parsed_response['error'].should eq(true)
      doc.parsed_response['errorNum'].should eq(1221)
      doc.parsed_response['code'].should eq(400)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")
    end

    it "returns an error if _key contains invalid characters" do
      cmd = "/_api/document?collection=#{@cn}"
      
      keys = [
        " ",
        "   ",
        "abcd ",
        " abcd",
        " abcd ",
        "\\tabcd",
        "\\nabcd",
        "\\rabcd",
        "abcd defg",
        "abcde/bdbg",
        "a/a",
        "/a",
        "adbfbgb/",
        "öööää",
        "müller",
        "\\\"invalid",
        "\\\\invalid",
        "\\\\\\\\invalid",
        "?invalid",
        "#invalid",
        "&invalid",
        "[invalid]",
        "a" * 255
      ]
  
      keys.each do|key|
        body = "{ \"_key\" : \"#{key}\" }"
        doc = ArangoDB.log_post("#{prefix}-invalid", cmd, :body => body)

        doc.code.should eq(400)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(1221)
        doc.parsed_response['code'].should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
      end
    end
    
    it "test valid key values" do
      cmd = "/_api/document?collection=#{@cn}"
      
      keys = [
        "0",
        "1",
        "123456",
        "0123456",
        "true",
        "false",
        "a",
        "A",
        "a1",
        "A1",
        "01ab01",
        "01AB01",
        "invalid", # actually valid
        "INVALID", # actually valid
        "inValId", # actually valid
        "abcd-efgh",
        "abcd_efgh",
        "Abcd_Efgh",
        "@",
        "@@",
        "abc@foo.bar",
        "@..abc-@-foo__bar",
        ".foobar",
        "-foobar",
        "_foobar",
        "@foobar",
        "(valid)",
        "%valid",
        "$valid",
        "$$bill,y'all",
        "'valid",
        "'a-key-is-a-key-is-a-key'",
        "m+ller",
        ";valid",
        ",valid",
        "!valid!",
        ":",
        ":::",
        ":-:-:",
        ";",
        ";;;;;;;;;;",
        "(",
        ")",
        "()xoxo()",
        "%",
        "%-%-%-%",
        ":-)",
        "!",
        "!!!!",
        "'",
        "''''",
        "this-key's-valid.",
        "=",
        "==================================================",
        "-=-=-=___xoxox-",
        "*",
        "(*)",
        "****",
        ".",
        "...",
        "-",
        "--",
        "_",
        "__",
        "(" * 254, # 254 bytes is the maximum allowed length
        ")" * 254, # 254 bytes is the maximum allowed length
        "," * 254, # 254 bytes is the maximum allowed length
        ":" * 254, # 254 bytes is the maximum allowed length
        ";" * 254, # 254 bytes is the maximum allowed length
        "*" * 254, # 254 bytes is the maximum allowed length
        "=" * 254, # 254 bytes is the maximum allowed length
        "-" * 254, # 254 bytes is the maximum allowed length
        "%" * 254, # 254 bytes is the maximum allowed length
        "@" * 254, # 254 bytes is the maximum allowed length
        "'" * 254, # 254 bytes is the maximum allowed length
        "." * 254, # 254 bytes is the maximum allowed length
        "!" * 254, # 254 bytes is the maximum allowed length
        "_" * 254, # 254 bytes is the maximum allowed length
        "a" * 254 # 254 bytes is the maximum allowed length
      ]
  
      keys.each do|key|
        body = "{ \"_key\" : \"#{key}\" }"
        doc = ArangoDB.log_post("#{prefix}-valid", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['_key'].should eq(key)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
      end
    end
    
    it "test duplicate key values" do
      cmd = "/_api/document?collection=#{@cn}"
      
      # prefill collection
      keys = [
        "0",
        "1",
        "a",
        "b",
        "c",
        "A",
        "B",
        "C",
        "aBcD-"
      ]
  
      keys.each do|key|
        body = "{ \"_key\" : \"#{key}\" }"
        # insert initially
        doc = ArangoDB.log_post("#{prefix}-valid", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['_key'].should eq(key)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
       
        # insert again. this will fail! 
        doc = ArangoDB.log_post("#{prefix}-valid", cmd, :body => body)

        doc.code.should eq(409)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(1210)
        doc.parsed_response['code'].should eq(409)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
      end
    end
      
  end
end
