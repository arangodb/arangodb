/*jshint globalstrict:false, strict:false */
/*global assertEqual, fail */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const store = require("@arangodb/foxx/store");
const db = arangodb.db;
const ERRORS = arangodb.errors;

function FoxxManagerSuite () {
  'use strict';

  const apps = [{"author":"Frank Celler","description":"Nonces in a box.","license":"Apache-2.0","keywords":["service"],"versions":{"1.0.0":{"type":"github","location":"arangodb-foxx/service-nonce","tag":"v1.0.0","engines":{"arangodb":"^2.5.0"}}},"_key":"nonce","name":"nonce"},{"author":"ArangoDB GmbH","description":"Mailer job type for Foxx using SendGrid.","license":"Apache-2.0","keywords":["util","mail"],"versions":{"2.0.0":{"type":"github","location":"arangodb-foxx/util-mailer-sendgrid","tag":"v2.0.0","engines":{"arangodb":"^2.6.0"},"provides":{"@foxx/sendgrid":"2.0.0"}},"1.0.0":{"type":"github","location":"arangodb-foxx/util-mailer-sendgrid","tag":"v1.0.0","engines":{"arangodb":"2.4 - 2.5"},"provides":{"@foxx/sendgrid":"1.0.0"}}},"_key":"mailer-sendgrid","name":"mailer-sendgrid"},{"author":"ArangoDB GmbH","contributors":["Alan Plum <me@pluma.io>"],"description":"Example Grafana Simple JSON connector for ArangoDB.","license":"Apache-2.0","keywords":["connector","service"],"versions":{"1.0.0":{"type":"github","location":"arangodb-foxx/grafana-connector","tag":"v1.0.0","engines":{"arangodb":"^3.0.0"}}},"_key":"grafana-connector","name":"grafana-connector"},{"author":"Michael Hackstein","description":"This a demo application implementing a ToDo list based on TodoMVC.","license":"Apache-2.0","keywords":["demo","service","todo"],"versions":{"1.4.0":{"type":"github","location":"arangodb-foxx/demo-aye-aye","tag":"v1.4.0","engines":{"arangodb":"3 - 3.3"}},"1.3.0":{"type":"github","location":"arangodb-foxx/demo-aye-aye","tag":"v1.3.0","engines":{"arangodb":"^2.7.0"}},"1.2.1":{"type":"github","location":"arangodb-foxx/demo-aye-aye","tag":"v1.2.1","engines":{"arangodb":"2 - 2.6"}}},"_key":"aye-aye","name":"aye-aye"},{"author":"ArangoDB GmbH","description":"Mailer job type for Foxx using Mailgun.","license":"Apache-2.0","keywords":["util","mail"],"versions":{"2.0.0":{"type":"github","location":"arangodb-foxx/util-mailer-mailgun","tag":"v2.0.0","engines":{"arangodb":"^2.6.0"},"provides":{"@foxx/mailgun":"2.0.0"}},"1.0.0":{"type":"github","location":"arangodb-foxx/util-mailer-mailgun","tag":"v1.0.0","engines":{"arangodb":"2.4 - 2.5"},"provides":{"@foxx/mailgun":"1.0.0"}}},"_key":"mailer-mailgun","name":"mailer-mailgun"},{"aliases":["user-service"],"description":"User Service for ArangoDB.","author":"ArangoDB GmbH","license":"Apache-2.0","keywords":["service","users"],"versions":{"1.1.1":{"type":"github","location":"arangodb-foxx/service-users","tag":"v1.1.0","engines":{"arangodb":"^2.6.0"}},"1.1.0":{"type":"github","location":"arangodb-foxx/service-users","tag":"v1.1.0","engines":{"arangodb":"^2.6.0"}},"1.0.0":{"type":"github","location":"arangodb-foxx/service-users","tag":"v1.0.0","engines":{"arangodb":"^2.6.0"}}},"_key":"users","name":"users"},{"author":"Michael Hackstein","description":"This an application to view the Marvel Hero Universe using AngularJS, d3.js and Foxx","license":"Apache-2.0","keywords":["demo","service"],"versions":{"1.1.1":{"type":"github","location":"mchacki/demo-marvel-hero-universe","tag":"v1.1.1","engines":{"arangodb":"1.4 - 2"}},"1.0.0":{"type":"github","location":"mchacki/demo-marvel-hero-universe","tag":"v1.0.0","engines":{"arangodb":"1.4 - 2"}}},"_key":"marvel-universe","name":"marvel-universe"},{"author":"Christian Pekeler","description":"Gives ArangoDB a heartbeat for external monitoring.","license":"Apache-2.0","keywords":["service"],"versions":{"1.0.2":{"type":"github","location":"pekeler/foxx-heartbeat","tag":"v1.0.2","engines":{"arangodb":"^2.6.0"}}},"_key":"foxx-heartbeat","name":"foxx-heartbeat"},{"author":"Andreas Streichardt","description":"A small and trashy game that moves your shards on the cluster.","license":"Apache-2.0","keywords":["service","cluster","game","resilience"],"versions":{"0.0.1":{"type":"github","location":"arangodb-foxx/pronto-move-shard","tag":"v0.0.1","engines":{"arangodb":"3.2 - 3.3"}}},"_key":"pronto-move-shard","name":"pronto-move-shard"},{"author":"ArangoDB GmbH","description":"Remote REST-based session storage for Foxx.","license":"Apache-2.0","keywords":["util","sessions"],"versions":{"1.0.0":{"type":"github","location":"arangodb-foxx/util-sessions-remote","tag":"v1.0.0","engines":{"arangodb":"^2.6.0"},"provides":{"@foxx/sessions":"2.0.0"}}},"_key":"sessions-remote","name":"sessions-remote"},{"author":"Heiko Kernbach","description":"A Foxx based geo example using the new (v3.4+) s2 geospatial index","license":"Apache-2.0","keywords":["geo","service"],"versions":{"1.0.0":{"type":"github","location":"arangodb-foxx/demo-geo-s2","tag":"v1.0.0","engines":{"arangodb":"^3.0.0"}}},"_key":"demo-geo-s2","name":"demo-geo-s2"},{"author":"Frederic Hemberger","description":"A client side error logging service for ArangoDB.","license":"MIT","keywords":["service","logging"],"versions":{"0.2.4":{"type":"github","location":"arangodb-foxx/service-fugu","tag":"v0.2.4","engines":{"arangodb":"^2.2.0"}},"0.2.3":{"type":"github","location":"arangodb-foxx/service-fugu","tag":"v0.2.3","engines":{"arangodb":"2.2 - 2.7"}},"0.2.2":{"type":"github","location":"arangodb-foxx/service-fugu","tag":"v0.2.2","engines":{"arangodb":"2 - 2.1"}}},"_key":"fugu","name":"fugu"},{"author":"Frank Celler","description":"This is 'Hello World' for ArangoDB Foxx.","license":"Apache-2.0","keywords":["demo","service"],"versions":{"2.0.0":{"type":"github","location":"arangodb-foxx/demo-hello-foxx","tag":"v2.0.0","engines":{"arangodb":"3 - 3.3"}},"1.8.0":{"type":"github","location":"arangodb-foxx/demo-hello-foxx","tag":"v1.8.0","engines":{"arangodb":"^2.7.0"}},"1.7.0":{"type":"github","location":"arangodb-foxx/demo-hello-foxx","tag":"v1.7.0","engines":{"arangodb":"^2.7.0"}},"1.6.0":{"type":"github","location":"arangodb-foxx/demo-hello-foxx","tag":"v1.6.0","engines":{"arangodb":"^2.7.0"}},"1.5.1":{"type":"github","location":"arangodb-foxx/demo-hello-foxx","tag":"v1.5.1","engines":{"arangodb":"2 - 2.6"}}},"_key":"hello-foxx","name":"hello-foxx"},{"author":"Michael Hackstein","description":"This a demo application implementing a user profile management using AngularJS.","license":"Apache-2.0","keywords":["demo","service"],"versions":{"1.1.1":{"type":"github","location":"mchacki/profile-manager","tag":"v1.1.1","engines":{"arangodb":"1.4 - 2"}},"1.0.0":{"type":"github","location":"mchacki/profile-manager","tag":"v1.0.0","engines":{"arangodb":"1.4 - 2"}}},"_key":"profile","name":"profile"},{"author":"Andrea Di Mario","description":"Multisite multicontent REST API.","license":"MIT","keywords":["service","rest","api","content"],"versions":{"0.0.2":{"type":"github","location":"anddimario/mumu","tag":"v0.0.2","engines":{"arangodb":"3 - 3.3"}},"0.0.1":{"type":"github","location":"anddimario/mumu","tag":"v0.0.1","engines":{"arangodb":"^3.0.0"}}},"_key":"mumu","name":"mumu"},{"author":"Jan Steemann","description":"This a demo application providing a REST interface to return a random Aztec god name.","license":"Apache-2.0","keywords":["service","demo"],"versions":{"1.2.0":{"type":"github","location":"arangodb-foxx/demo-itzpapalotl","tag":"v1.2.0","engines":{"arangodb":"^3.0.0"}},"1.1.0":{"type":"github","location":"arangodb-foxx/demo-itzpapalotl","tag":"v1.1.0","engines":{"arangodb":"^2.7.0"}},"0.0.5":{"type":"github","location":"arangodb-foxx/demo-itzpapalotl","tag":"v0.0.5","engines":{"arangodb":"2 - 2.6"}}},"_key":"itzpapalotl","name":"itzpapalotl"},{"author":"Michael Hackstein","description":"A foxx library to manage create, manage and keep track of API key given out for your foxx APIs.","license":"Apache-2.0","keywords":["util","auth"],"versions":{"1.0.0":{"type":"github","location":"arangodb-foxx/util-api-keys","tag":"master","engines":{"arangodb":"^2.6.0"},"provides":{"@foxx/api-keys":"1.0.0"}}},"_key":"api-keys","name":"api-keys"},{"author":"ArangoDB GmbH","description":"Password-based authentication for Foxx based on Eric Elliott's credential module.","license":"Apache-2.0","keywords":["util","auth"],"versions":{"1.1.0":{"type":"github","location":"arangodb-foxx/util-credential-auth","tag":"v1.1.0","engines":{"arangodb":"^2.6.0"},"provides":{"@foxx/auth":"1.0.0"}},"1.0.1":{"type":"github","location":"arangodb-foxx/util-credential-auth","tag":"v1.0.1","engines":{"arangodb":"^2.6.0"},"provides":{"@foxx/auth":"1.0.0"}},"1.0.0":{"type":"github","location":"arangodb-foxx/util-credential-auth","tag":"v1.0.0","engines":{"arangodb":"^2.6.0"},"provides":{"@foxx/auth":"1.0.0"}}},"_key":"credential-auth","name":"credential-auth"},{"author":"ArangoDB GmbH","description":"Logger job type for Foxx using BugSnag.","license":"Apache-2.0","keywords":["util","logging"],"versions":{"2.0.0":{"type":"github","location":"arangodb-foxx/util-logger-bugsnag","tag":"v2.0.0","engines":{"arangodb":"^2.6.0"},"provides":{"@foxx/bugsnag":"2.0.0"}},"1.0.0":{"type":"github","location":"arangodb-foxx/util-logger-bugsnag","tag":"v1.0.0","engines":{"arangodb":"2.4 - 2.5"},"provides":{"@foxx/bugsnag":"1.0.0"}}},"_key":"logger-bugsnag","name":"logger-bugsnag"},{"aliases":["session-service"],"author":"ArangoDB GmbH","description":"Session-Service for ArangoDB.","license":"Apache-2.0","keywords":["service","sessions"],"versions":{"1.3.0":{"type":"github","location":"arangodb-foxx/service-sessions","tag":"v1.3.0","engines":{"arangodb":"^2.7.0"}},"1.2.1":{"type":"github","location":"arangodb-foxx/service-sessions","tag":"v1.2.1","engines":{"arangodb":"^2.7.0"}},"1.0.0":{"type":"github","location":"arangodb-foxx/service-sessions","tag":"v1.0.0","engines":{"arangodb":"~2.6.0"}}},"_key":"sessions","name":"sessions"},{"author":"ArangoDB GmbH","description":"Mailer job type for Foxx using Postmark.","license":"Apache-2.0","keywords":["util","mail"],"versions":{"2.0.0":{"type":"github","location":"arangodb-foxx/util-mailer-postmark","tag":"v2.0.0","engines":{"arangodb":"^2.6.0"},"provides":{"@foxx/postmark":"2.0.0"}},"1.0.0":{"type":"github","location":"arangodb-foxx/util-mailer-postmark","tag":"v1.0.0","engines":{"arangodb":"2.4 - 2.5"},"provides":{"@foxx/postmark":"1.0.0"}}},"_key":"mailer-postmark","name":"mailer-postmark"},{"author":"ArangoDB GmbH","description":"Example application for Foxx sessions with password auth and OAuth2.","license":"Apache-2.0","keywords":["demo","service","auth"],"versions":{"1.3.0":{"type":"github","location":"arangodb-foxx/demo-sessions","tag":"v1.3.0","engines":{"arangodb":"^2.7.0"}},"1.2.0":{"type":"github","location":"arangodb-foxx/demo-sessions","tag":"v1.2.0","engines":{"arangodb":"2 - 2.6"}}},"_key":"demo-sessions","name":"demo-sessions"},{"author":"ArangoDB GmbH","description":"JWT-based session storage for Foxx.","license":"Apache-2.0","keywords":["util","sessions"],"versions":{"1.0.2":{"type":"github","location":"arangodb-foxx/util-sessions-jwt","tag":"v1.0.2","engines":{"arangodb":"^2.6.0"},"provides":{"@foxx/sessions":"2.0.0"}},"1.0.1":{"type":"github","location":"arangodb-foxx/util-sessions-jwt","tag":"v1.0.1","engines":{"arangodb":"^2.6.0"},"provides":{"@foxx/sessions":"2.0.0"}},"1.0.0":{"type":"github","location":"arangodb-foxx/util-sessions-jwt","tag":"v1.0.0","engines":{"arangodb":"^2.6.0"},"provides":{"@foxx/sessions":"2.0.0"}}},"_key":"sessions-jwt","name":"sessions-jwt"},{"author":"ArangoDB GmbH","description":"OAuth2 authentication for Foxx.","license":"Apache-2.0","keywords":["util","auth"],"versions":{"2.0.0":{"type":"github","location":"arangodb-foxx/oauth2","tag":"v2.0.0","engines":{"arangodb":"^2.7.0"},"provides":{"@foxx/oauth2":"2.0.0"}},"1.2.0":{"type":"github","location":"arangodb-foxx/oauth2","tag":"v1.2.0","engines":{"arangodb":"~2.6.0"},"provides":{"@foxx/oauth2":"1.0.0"}},"1.1.0":{"type":"github","location":"arangodb-foxx/oauth2","tag":"v1.1.0","engines":{"arangodb":"2.3 - 2.5"},"provides":{"@foxx/oauth2":"1.0.0"}},"1.0.0":{"type":"github","location":"arangodb-foxx/oauth2","tag":"v1.0.0","engines":{"arangodb":"2.3 - 2.5"},"provides":{"@foxx/oauth2":"1.0.0"}}},"_key":"oauth2","name":"oauth2"},{"author":"ArangoDB GmbH","description":"Example Foxx service for using GraphQL.","license":"Apache-2.0","keywords":["demo","service","graphql"],"versions":{"2.0.2":{"type":"github","location":"arangodb-foxx/demo-graphql","tag":"v2.0.2","engines":{"arangodb":"^3.1.0"}},"2.0.1":{"type":"github","location":"arangodb-foxx/demo-graphql","tag":"v2.0.1","engines":{"arangodb":"^3.1.0"}},"2.0.0":{"type":"github","location":"arangodb-foxx/demo-graphql","tag":"v2.0.0","engines":{"arangodb":"^3.1.0"}},"1.2.2":{"type":"github","location":"arangodb-foxx/demo-graphql","tag":"v1.2.2","engines":{"arangodb":"^2.8.0"}},"1.2.1":{"type":"github","location":"arangodb-foxx/demo-graphql","tag":"v1.2.1","engines":{"arangodb":"^2.8.0"}},"1.2.0":{"type":"github","location":"arangodb-foxx/demo-graphql","tag":"v1.2.0","engines":{"arangodb":"^2.8.0"}},"1.1.0":{"type":"github","location":"arangodb-foxx/demo-graphql","tag":"v1.1.0","engines":{"arangodb":"^2.8.0"}},"1.0.0":{"type":"github","location":"arangodb-foxx/demo-graphql","tag":"v1.0.0","engines":{"arangodb":"^2.8.0"}}},"_key":"demo-graphql","name":"demo-graphql"},{"author":"ArangoDB GmbH","description":"Mailer job type for Foxx using PostageApp.","license":"Apache-2.0","keywords":["util","mail"],"versions":{"2.0.0":{"type":"github","location":"arangodb-foxx/util-mailer-postageapp","tag":"v2.0.0","engines":{"arangodb":"^2.6.0"},"provides":{"@foxx/postageapp":"2.0.0"}},"1.0.0":{"type":"github","location":"arangodb-foxx/util-mailer-postageapp","tag":"v1.0.0","engines":{"arangodb":"2.4 - 2.5"},"provides":{"@foxx/postageapp":"1.0.0"}}},"_key":"mailer-postageapp","name":"mailer-postageapp"},{"author":"ArangoDB GmbH","description":"Collection-based session storage for Foxx.","license":"Apache-2.0","keywords":["util","sessions"],"versions":{"2.0.2":{"type":"github","location":"arangodb-foxx/util-sessions-local","tag":"v2.0.2","engines":{"arangodb":"^2.6.0"},"provides":{"@foxx/sessions":"2.0.0"}},"2.0.1":{"type":"github","location":"arangodb-foxx/util-sessions-local","tag":"v2.0.1","engines":{"arangodb":"^2.6.0"},"provides":{"@foxx/sessions":"2.0.0"}},"2.0.0":{"type":"github","location":"arangodb-foxx/util-sessions-local","tag":"v2.0.0","engines":{"arangodb":"^2.6.0"},"provides":{"@foxx/sessions":"2.0.0"}},"1.1.0":{"type":"github","location":"arangodb-foxx/util-sessions-local","tag":"v1.1.0","engines":{"arangodb":"2.3 - 2.5"},"provides":{"@foxx/sessions":"1.0.0"}},"1.0.0":{"type":"github","location":"arangodb-foxx/util-sessions-local","tag":"v1.0.0","engines":{"arangodb":"2.3 - 2.5"},"provides":{"@foxx/sessions":"1.0.0"}}},"_key":"sessions-local","name":"sessions-local"},{"author":"Manuel Pöter","description":"A simple Foxx service for ArangoDB that provides the internal statistics from ArangoDB in the Prometheus format.","license":"MIT","keywords":["prometheus","monitoring"],"versions":{"0.1.0":{"type":"github","location":"Lean5/arango-prometheus-exporter","tag":"master","engines":{"arangodb":"^3.0.0"}}},"_key":"prometheus-exporter","name":"prometheus-exporter"},{"author":"ArangoDB GmbH","contributors":["Heiko Kernbach <heiko@arangodb.com>","Alan Plum <me@pluma.io>"],"description":"Example Qlik REST connector for ArangoDB.","license":"Apache-2.0","keywords":["connector","service"],"versions":{"1.0.0":{"type":"github","location":"arangodb-foxx/qlik-connector","tag":"v1.0.0","engines":{"arangodb":"^3.0.0"}}},"_key":"qlik-connector","name":"qlik-connector"},{"author":"ArangoDB GmbH","description":"Simple password-based authentication for Foxx.","license":"Apache-2.0","keywords":["util","auth"],"versions":{"2.0.0":{"type":"github","location":"arangodb-foxx/util-simple-auth","tag":"v2.0.0","engines":{"arangodb":"^2.7.0"},"provides":{"@foxx/auth":"2.0.0"}},"1.1.0":{"type":"github","location":"arangodb-foxx/util-simple-auth","tag":"v1.1.0","engines":{"arangodb":"^2.4.0"},"provides":{"@foxx/auth":"1.0.0"}},"1.0.0":{"type":"github","location":"arangodb-foxx/util-simple-auth","tag":"v1.0.0","engines":{"arangodb":"^2.4.0"},"provides":{"@foxx/auth":"1.0.0"}}},"_key":"simple-auth","name":"simple-auth"},{"author":"ArangoDB GmbH","contributors":["Heiko Kernbach <heiko@arangodb.com>","Alan Plum <me@pluma.io>"],"description":"Example Power BI Web connector for ArangoDB.","license":"Apache-2.0","keywords":["connector","service"],"versions":{"1.0.0":{"type":"github","location":"arangodb-foxx/powerbi-connector","tag":"v1.0.0","engines":{"arangodb":"^3.0.0"}}},"_key":"powerbi-connector","name":"powerbi-connector"},{"author":"ArangoDB GmbH","contributors":["Alan Plum <me@pluma.io>"],"description":"Example Tableau Web Data Connector for ArangoDB.","license":"Apache-2.0","keywords":["connector","service"],"versions":{"1.0.2":{"type":"github","location":"arangodb-foxx/tableau-connector","tag":"v1.0.2","engines":{"arangodb":"^3.0.0"}},"1.0.1":{"type":"github","location":"arangodb-foxx/tableau-connector","tag":"v1.0.1","engines":{"arangodb":"^3.0.0"}},"1.0.0":{"type":"github","location":"arangodb-foxx/tableau-connector","tag":"v1.0.0","engines":{"arangodb":"^3.0.0"}}},"_key":"tableau-connector","name":"tableau-connector"},{"author":"ArangoDB GmbH","description":"Username-based user storage for Foxx.","license":"Apache-2.0","keywords":["util","users"],"versions":{"3.0.0":{"type":"github","location":"arangodb-foxx/util-users-local","tag":"v3.0.0","engines":{"arangodb":"^2.7.0"},"provides":{"@foxx/users":"3.0.0"}},"2.0.2":{"type":"github","location":"arangodb-foxx/util-users-local","tag":"v2.0.2","engines":{"arangodb":"^2.6.0"},"provides":{"@foxx/users":"2.0.0"}},"2.0.1":{"type":"github","location":"arangodb-foxx/util-users-local","tag":"v2.0.1","engines":{"arangodb":"^2.6.0"},"provides":{"@foxx/users":"2.0.0"}},"2.0.0":{"type":"github","location":"arangodb-foxx/util-users-local","tag":"v2.0.0","engines":{"arangodb":"^2.6.0"},"provides":{"@foxx/users":"2.0.0"}},"1.1.0":{"type":"github","location":"arangodb-foxx/util-users-local","tag":"v1.1.0","engines":{"arangodb":"^2.3.0"},"provides":{"@foxx/users":"1.0.0"}},"1.0.0":{"type":"github","location":"arangodb-foxx/util-users-local","tag":"v1.0.0","engines":{"arangodb":"^2.3.0"},"provides":{"@foxx/users":"1.0.0"}}},"_key":"users-local","name":"users-local"},{"author":"ArangoDB GmbH","description":"Job types for Foxx using segment.io.","license":"Apache-2.0","keywords":["util","analytics"],"versions":{"2.0.0":{"type":"github","location":"arangodb-foxx/util-segment-io","tag":"v2.0.0","engines":{"arangodb":"^2.6.0"},"provides":{"@foxx/segment-io":"2.0.0"}},"1.0.0":{"type":"github","location":"arangodb-foxx/util-segment-io","tag":"v1.0.0","engines":{"arangodb":"2.4 - 2.5"},"provides":{"@foxx/segment-io":"1.0.0"}}},"_key":"segment-io","name":"segment-io"},{"author":"ArangoDB GmbH","description":"API documentation browser app for Foxx services.","license":"Apache-2.0","keywords":["service","documentation"],"versions":{"1.0.3":{"type":"github","location":"arangodb-foxx/service-swagger","tag":"v1.0.3","engines":{"arangodb":"^2.5.0"}},"1.0.2":{"type":"github","location":"arangodb-foxx/service-swagger","tag":"v1.0.2","engines":{"arangodb":"^2.5.0"}},"1.0.1":{"type":"github","location":"arangodb-foxx/service-swagger","tag":"v1.0.1","engines":{"arangodb":"^2.5.0"}},"1.0.0":{"type":"github","location":"arangodb-foxx/service-swagger","tag":"v1.0.0","engines":{"arangodb":"^2.5.0"}}},"_key":"swagger","name":"swagger"}];

  return {
    setUp : function () {
      global.FISHBOWL_SET([]);
    },

    tearDown : function () {
      global.FISHBOWL_SET([]);
    },

    testFishbowlSetGet : function () {
      global.FISHBOWL_SET([1, 2, 3]);
      assertEqual([1, 2, 3], global.FISHBOWL_GET());
      
      global.FISHBOWL_SET([]);
      assertEqual([], global.FISHBOWL_GET());
    },

    testStoreAvailableJsonEmptyStore: function () {
      global.FISHBOWL_SET([]);

      let res = store.availableJson();
      assertEqual([], res);
    },
    
    testStoreAvailableJson: function () {
      global.FISHBOWL_SET(apps);

      let res = store.availableJson();
      assertEqual(35, res.length);

      assertEqual({
        "name" : "nonce",
        "description" : "Nonces in a box.",
        "author" : "Frank Celler",
        "latestVersion" : "1.0.0",
        "legacy" : true,
        "location" : "arangodb-foxx/service-nonce",
        "license" : "Apache-2.0",
        "categories" : [
          "service"
        ]
      }, res[0]);
      
      assertEqual({  
        "name" : "mailer-sendgrid", 
        "description" : "Mailer job type for Foxx using SendGrid.", 
        "author" : "ArangoDB GmbH", 
        "latestVersion" : "2.0.0", 
        "legacy" : true, 
        "location" : "arangodb-foxx/util-mailer-sendgrid", 
        "license" : "Apache-2.0", 
        "categories" : [ 
          "util", 
          "mail" 
        ] 
      }, res[1]);
    },
        //assertEqual(ERRORS.ERROR_QUERY_PARSE.code, e.errorNum);
    
    testStoreSearchWithFilter: function () {
      global.FISHBOWL_SET(apps);

      let res = store.searchJson("mail").map((res) => res.name);
      res.sort();

      assertEqual([
        "mailer-mailgun",
        "mailer-postageapp",
        "mailer-postmark",
        "mailer-sendgrid",
      ], res);
      
      res = store.searchJson("foxx").map((res) => res.name);
      res.sort();

      assertEqual([ 
        "api-keys", 
        "credential-auth", 
        "demo-geo-s2", 
        "demo-graphql", 
        "demo-sessions", 
        "foxx-heartbeat", 
        "hello-foxx", 
        "logger-bugsnag", 
        "mailer-mailgun", 
        "mailer-postageapp", 
        "mailer-postmark", 
        "mailer-sendgrid", 
        "marvel-universe", 
        "oauth2", 
        "prometheus-exporter", 
        "segment-io", 
        "sessions-jwt", 
        "sessions-local", 
        "sessions-remote", 
        "simple-auth", 
        "swagger", 
        "users-local" 
      ], res);
    },
    
    testStoreSearchNoFilter: function () {
      global.FISHBOWL_SET(apps);

      let res = store.searchJson("").map((res) => res.name);
      res.sort();

      assertEqual([
        "api-keys",
        "aye-aye",
        "credential-auth",
        "demo-geo-s2",
        "demo-graphql",
        "demo-sessions",
        "foxx-heartbeat",
        "fugu",
        "grafana-connector",
        "hello-foxx",
        "itzpapalotl",
        "logger-bugsnag",
        "mailer-mailgun",
        "mailer-postageapp",
        "mailer-postmark",
        "mailer-sendgrid",
        "marvel-universe",
        "mumu",
        "nonce",
        "oauth2",
        "powerbi-connector",
        "profile",
        "prometheus-exporter",
        "pronto-move-shard",
        "qlik-connector",
        "segment-io",
        "sessions",
        "sessions-jwt",
        "sessions-local",
        "sessions-remote",
        "simple-auth",
        "swagger",
        "tableau-connector",
        "users",
        "users-local"
      ], res);
    },

    testStoreSearchOneResult: function () {
      global.FISHBOWL_SET(apps);

      let res = store.searchJson("heartbeat");
      assertEqual([ 
        { 
          "author" : "Christian Pekeler", 
          "description" : "Gives ArangoDB a heartbeat for external monitoring.", 
          "license" : "Apache-2.0", 
          "keywords" : [ 
            "service" 
          ], 
          "versions" : { 
            "1.0.2" : { 
              "type" : "github", 
              "location" : "pekeler/foxx-heartbeat", 
              "tag" : "v1.0.2", 
              "engines" : { 
                "arangodb" : "^2.6.0" 
              } 
            } 
          }, 
          "_key" : "foxx-heartbeat", 
          "name" : "foxx-heartbeat" 
        } 
      ], res);
    },
    
    testStoreSearchNoResults: function () {
      global.FISHBOWL_SET(apps);

      let res = store.searchJson("piffpaff");
      assertEqual([], res);
    },
    
    testStoreSearchJson: function () {
      global.FISHBOWL_SET(apps);

      let res = store.searchJson("itzpapalotl");
      assertEqual([ 
        { 
          "author" : "Jan Steemann", 
          "description" : "This a demo application providing a REST interface to return a random Aztec god name.", 
          "license" : "Apache-2.0", 
          "keywords" : [ 
            "service", 
            "demo" 
          ], 
          "versions" : { 
            "1.2.0" : { 
              "type" : "github", 
              "location" : "arangodb-foxx/demo-itzpapalotl", 
              "tag" : "v1.2.0", 
              "engines" : { 
                "arangodb" : "^3.0.0" 
              } 
            }, 
            "1.1.0" : { 
              "type" : "github", 
              "location" : "arangodb-foxx/demo-itzpapalotl", 
              "tag" : "v1.1.0", 
              "engines" : { 
                "arangodb" : "^2.7.0" 
              } 
            }, 
            "0.0.5" : { 
              "type" : "github", 
              "location" : "arangodb-foxx/demo-itzpapalotl", 
              "tag" : "v0.0.5", 
              "engines" : { 
                "arangodb" : "2 - 2.6" 
              } 
            } 
          }, 
          "_key" : "itzpapalotl", 
          "name" : "itzpapalotl" 
        } 
      ], res);
    },
  
  };
}

jsunity.run(FoxxManagerSuite);
return jsunity.done();
