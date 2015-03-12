# coding: utf-8

require 'rspec'
require 'arangodb.rb'
require 'json'

describe ArangoDB do

  before do
    @api = "/_api/query"
    @prefix = "api-query"
    @current = "#{@api}/current"
    @slow = "#{@api}/slow"
    @properties = "#{@api}/properties"
    @queryBody = JSON.dump({query: "FOR x IN 1..5 LET y = SLEEP(1) RETURN x"})
    @longQuery = JSON.dump({query: "FOR x IN 1..1 LET y = SLEEP(5) LET a = y LET b = a LET c = b LET d = c LET e = d LET f = e  RETURN x"})
    @queryEndpoint ="/_api/cursor"
    @queryPrefix = "api-cursor"
  end

  def send_queries (count = 1, async = "true") 
    count.times do
      ArangoDB.log_post(@queryPrefix, @queryEndpoint, :body => @queryBody, :headers => {"X-Arango-Async" => async })
    end
  end

  it "should activate Tracking" do
    doc = ArangoDB.log_put("#{@prefix}-properties", @properties, :body => JSON.dump({enable: true}))
    doc.code.should eq(200)
  end

  describe "tracking" do

    before do
      ArangoDB.log_put("#{@prefix}-properties", @properties, :body => JSON.dump({enable: true, slowQueryThreshold: 20, trackSlowQueries: true}))
    end

    before(:each) do
      ArangoDB.log_delete("#{@prefix}-delete", @slow)
    end

    after(:each) do
      # Let the queries finish
      doc = ArangoDB.log_get("#{@prefix}-current", @current)
      res = JSON.parse doc.body
      if (res.length > 0)
        sleep 6
      end
    end

    it "should track running queries" do
      send_queries
      doc = ArangoDB.log_get("#{@prefix}-current", @current)
      doc.code.should eq(200)
      res = JSON.parse doc.body
      res.length.should eq(1)
    end

    it "should track slow queries by threshold" do
      send_queries 1,"false"
      doc = ArangoDB.log_get("#{@prefix}-slow", @slow)
      doc.code.should eq(200)
      res = JSON.parse doc.body
      res.length.should eq(0)

      ArangoDB.log_put("#{@prefix}-properties", @properties, :body => JSON.dump({slowQueryThreshold: 2}))

      send_queries 1,"false"
      doc = ArangoDB.log_get("#{@prefix}-current", @current)
      doc.code.should eq(200)
      res = JSON.parse doc.body
      res.length.should eq(0)
      doc = ArangoDB.log_get("#{@prefix}-slow", @slow)
      doc.code.should eq(200)
      res = JSON.parse doc.body
      res.length.should eq(1)
    end

    it "should track at most n slow queries" do 
      max = 3
      ArangoDB.log_put("#{@prefix}-properties", @properties, :body => JSON.dump({slowQueryThreshold: 2, maxSlowQueries: max}))
      send_queries 6

      sleep 12

      doc = ArangoDB.log_get("#{@prefix}-slow", @slow)
      doc.code.should eq(200)
      res = JSON.parse doc.body
      res.length.should eq(max)
    end

    it "should not track slow queries if turned off" do
      doc = ArangoDB.log_get("#{@prefix}-slow", @slow)
      doc.code.should eq(200)
      res = JSON.parse doc.body
      res.length.should eq(0)

      ArangoDB.log_put("#{@prefix}-properties", @properties, :body => JSON.dump({slowQueryThreshold: 2, trackSlowQueries: false}))
      send_queries 1, "false"

      doc = ArangoDB.log_get("#{@prefix}-slow", @slow)
      doc.code.should eq(200)
      res = JSON.parse doc.body
      res.length.should eq(0)
    end

    it "should truncate the query string to at least 64 bit" do
      ArangoDB.log_put("#{@prefix}-properties", @properties, :body => JSON.dump({slowQueryThreshold: 2, maxQueryStringLength: 12}))
      doc = ArangoDB.log_get("#{@prefix}-properties", @properties)
      res = JSON.parse doc.body
      res["maxQueryStringLength"].should eq(64)
    end

    it "should truncate the query string" do
      ArangoDB.log_put("#{@prefix}-properties", @properties, :body => JSON.dump({slowQueryThreshold: 2, maxQueryStringLength: 64}))
      ArangoDB.log_post(@queryPrefix, @queryEndpoint, :body => @longQuery)
      doc = ArangoDB.log_get("#{@prefix}-slow", @slow)
      doc.code.should eq(200)
      res = JSON.parse doc.body
      res.length.should eq(1)
      # This string is exactly 64 bit long
      shortend = "FOR x IN 1..1 LET y = SLEEP(5) LET a = y LET b = a LET c = b LET"
      res[0]["query"].should eq(shortend + "...")
    end

    it "should properly truncate UTF8 symbols" do
      ArangoDB.log_put("#{@prefix}-properties", @properties, :body => JSON.dump({slowQueryThreshold: 2, maxQueryStringLength: 64}))
      ArangoDB.log_post(@queryPrefix, @queryEndpoint, :body => JSON.dump({query: 'FOR x IN 1..1 LET y = SLEEP(5) LET a = y LET b = a LET c= "ööööööö" RETURN c'}))
      doc = ArangoDB.log_get("#{@prefix}-slow", @slow)
      doc.code.should eq(200)
      res = JSON.parse doc.body
      res.length.should eq(1)
      # This string is exactly 64 bit long
      shortend = "FOR x IN 1..1 LET y = SLEEP(5) LET a = y LET b = a LET c= \"öö"
      res[0]["query"].should eq(shortend + "...")
    end

    it "should be able to kill a running query" do
      send_queries
      doc = ArangoDB.log_get("#{@prefix}-current", @current)
      res = JSON.parse doc.body
      res.length.should eq(1)
      doc = ArangoDB.log_delete(@prefix, "#{@api}/" + res[0]["id"])
      doc.code.should eq(200)
      sleep 1
      doc = ArangoDB.log_get("#{@prefix}-current", @current)
      res = JSON.parse doc.body
      res.length.should eq(0)
    end

  end

end
