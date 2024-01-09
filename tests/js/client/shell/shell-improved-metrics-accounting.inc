/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global fail, assertEqual, assertNotEqual, assertTrue */

////////////////////////////////////////////////////////////////////////////////
/// Copyright 2023 ArangoDB GmbH, Cologne, Germany
///
/// The Programs (which include both the software and documentation) contain
/// proprietary information of ArangoDB GmbH; they are provided under a license
/// agreement containing restrictions on use and disclosure and are also
/// protected by copyright, patent and other intellectual and industrial
/// property laws. Reverse engineering, disassembly or decompilation of the
/// Programs, except to the extent required to obtain interoperability with
/// other independently created software or as specified by law, is prohibited.
///
/// It shall be the licensee's responsibility to take all appropriate fail-safe,
/// backup, redundancy, and other measures to ensure the safe use of
/// applications if the Programs are used for purposes such as nuclear,
/// aviation, mass transit, medical, or other inherently dangerous applications,
/// and ArangoDB GmbH disclaims liability for any damages caused by such use of
/// the Programs.
///
/// This software is the confidential and proprietary information of ArangoDB
/// GmbH. You shall not disclose such confidential and proprietary information
/// and shall use it only in accordance with the terms of the license agreement
/// you entered into with ArangoDB GmbH.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Alexey Bakharew
////////////////////////////////////////////////////////////////////////////////
(function () {

  'use strict';
  const db = require('@arangodb').db;
  const { getAllMetricsFromEndpoints, getMetricSingle, getCompleteMetricsValues } = require('@arangodb/test-helper');
  const isCluster = require("internal").isCluster();

  return function (dbName, databaseCreationOptions, collectionCreationOptions, shardingEnabled = false) {
    const n = 10000;

    let insertDocuments = (collName) => {
      let c = db._collection(collName);
      let docs = [];
      for (let i = 0; i < n; ++i) {
        docs.push({value: i, value1: i, value2: i});
        if (docs.length === 5000) {
          c.insert(docs);
          docs = [];
        }
      }
      if (docs.length > 0) {
        c.insert(docs);
        docs = [];
      }
    };   

    return {
      setUpAll: function () {
        if (databaseCreationOptions) {
          db._createDatabase(dbName, databaseCreationOptions);
        } else {
          db._createDatabase(dbName);
        }
        db._useDatabase(dbName);
      },

      tearDownAll: function () {
        db._useDatabase('_system');
        db._dropDatabase(dbName);
      },

      testAgencyNodeAndInternalClusterInfoMemoryUsage: function () {
        let [initial_cluster_info_metric, initial_agency_metric] = getCompleteMetricsValues([
          "arangodb_internal_cluster_info_memory_usage",
          "arangodb_agency_node_memory_usage"]
        );
        if (isCluster) {
          assertTrue(initial_cluster_info_metric > 0, initial_cluster_info_metric);
          assertTrue(initial_agency_metric > 0, initial_agency_metric);
        } else {
          // on some platforms, the empty containers used in ClusterInfo
          // can make an initial allocation even though no data is contained
          // in them.
          assertTrue(initial_cluster_info_metric <= 1000, initial_cluster_info_metric);
          assertTrue(initial_agency_metric <= 1000, initial_agency_metric);
        }

        let cluster_info_metric_before = initial_cluster_info_metric;
        let agency_metric_before = initial_agency_metric;

        let cluster_info_metric_after;
        let agency_metric_metric_after;

        for(let i = 0; i < 2; i++) {
          let cn = `c${i}`;
          db._create(cn, collectionCreationOptions);
          [cluster_info_metric_after, agency_metric_metric_after] = getCompleteMetricsValues([
            "arangodb_internal_cluster_info_memory_usage",
            "arangodb_agency_node_memory_usage"]
          );
          if (isCluster) {
            assertTrue(cluster_info_metric_after > cluster_info_metric_before, {cluster_info_metric_after, cluster_info_metric_before});
            assertTrue(agency_metric_metric_after > agency_metric_before, {agency_metric_metric_after, agency_metric_before});
          } else {
            assertEqual(cluster_info_metric_before, cluster_info_metric_after);
            assertEqual(agency_metric_before, agency_metric_metric_after);
          }

          cluster_info_metric_before = cluster_info_metric_after;
          agency_metric_before = agency_metric_metric_after;

          insertDocuments(cn);

          // create few indexes
          let coll = db._collection(cn);

          coll.ensureIndex({type: "inverted", name: "inverted", fields: ["value"]});
          coll.ensureIndex({type: "inverted", name: "inverted1", fields: ["value1"]});
          coll.ensureIndex({type: "inverted", name: "inverted2", fields: ["value2"]});

          coll.ensureIndex({ type: "persistent", fields: ["value"], estimates: true });
          coll.ensureIndex({ type: "persistent", fields: ["value1"], estimates: true });
          coll.ensureIndex({ type: "persistent", fields: ["value2"], estimates: true });

          [cluster_info_metric_after, agency_metric_metric_after] = getCompleteMetricsValues([
            "arangodb_internal_cluster_info_memory_usage",
            "arangodb_agency_node_memory_usage"]
          );
          if (isCluster) {
            assertTrue(cluster_info_metric_after > cluster_info_metric_before, {cluster_info_metric_after, cluster_info_metric_before});
            assertTrue(agency_metric_metric_after > agency_metric_before, {agency_metric_metric_after, agency_metric_before});
          } else {
            assertEqual(cluster_info_metric_before, cluster_info_metric_after);
            assertEqual(agency_metric_before, agency_metric_metric_after);
          }
          cluster_info_metric_before = cluster_info_metric_after;
          agency_metric_before = agency_metric_metric_after;

          // create few arangosearch views
          db._createView(`view0${i}`, 'arangosearch', { links: { [cn]: {value: {}} } });
          db._createView(`view1${i}`, 'arangosearch', { links: { [cn]: {value1: {}} } });
          db._createView(`view2${i}`, 'arangosearch', { links: { [cn]: {value2: {}} } });

          [cluster_info_metric_after, agency_metric_metric_after] = getCompleteMetricsValues([
            "arangodb_internal_cluster_info_memory_usage",
            "arangodb_agency_node_memory_usage"]
          );
          if (isCluster) {
            assertTrue(cluster_info_metric_after > cluster_info_metric_before, {cluster_info_metric_after, cluster_info_metric_before});
            assertTrue(agency_metric_metric_after > agency_metric_before, {agency_metric_metric_after, agency_metric_before});
          } else {
            assertEqual(cluster_info_metric_before, cluster_info_metric_after);
            assertEqual(agency_metric_before, agency_metric_metric_after);
          }
          cluster_info_metric_before = cluster_info_metric_after;
          agency_metric_before = agency_metric_metric_after;

          // create few search-alias views
          db._createView(`viewSa0${i}`, 'search-alias', { indexes: [ {collection: `${cn}`, index: "inverted"}]} );
          db._createView(`viewSa1${i}`, 'search-alias', { indexes: [ {collection: `${cn}`, index: "inverted1"}]});
          db._createView(`viewSa2${i}`, 'search-alias', { indexes: [ {collection: `${cn}`, index: "inverted2"}]});

          [cluster_info_metric_after, agency_metric_metric_after] = getCompleteMetricsValues([
            "arangodb_internal_cluster_info_memory_usage",
            "arangodb_agency_node_memory_usage"]
          );
          if (isCluster) {
            assertTrue(cluster_info_metric_after > cluster_info_metric_before, {cluster_info_metric_after, cluster_info_metric_before});
            assertTrue(agency_metric_metric_after > agency_metric_before, {agency_metric_metric_after, agency_metric_before});
          } else {
            assertEqual(cluster_info_metric_before, cluster_info_metric_after);
            assertEqual(agency_metric_before, agency_metric_metric_after);
          }
          cluster_info_metric_before = cluster_info_metric_after;
          agency_metric_before = agency_metric_metric_after;
        }

        for (let i = 0; i < 2; i++) {
          let [cluster_info_metric_before, agency_metric_before] = getCompleteMetricsValues([
            "arangodb_internal_cluster_info_memory_usage",
            "arangodb_agency_node_memory_usage"]
          );
          db._drop(`c${i}`);
          let [cluster_info_metric_after, agency_metric_after] = getCompleteMetricsValues([
            "arangodb_internal_cluster_info_memory_usage",
            "arangodb_agency_node_memory_usage"]
          );
          if (isCluster) {
            assertTrue(cluster_info_metric_after < cluster_info_metric_before);
            assertTrue(agency_metric_after < agency_metric_before);

          } else {
            // on some platforms, the empty containers used in ClusterInfo
            // can make an initial allocation even though no data is contained
            // in them.
            assertTrue(cluster_info_metric_before <= 1000, cluster_info_metric_before);
            assertTrue(cluster_info_metric_after <= 1000, cluster_info_metric_after);
            assertTrue(agency_metric_before <= 1000, agency_metric_before);
            assertTrue(agency_metric_after <= 1000, agency_metric_after);
          }
        }
      },

      testRequestMemoryUsage: function () {
        let metrics;
        if (isCluster) {
          metrics = getAllMetricsFromEndpoints("arangodb_requests_memory_usage", "all");
          metrics.forEach(value => {
              assertTrue(value > 0);
          });
        } else {
          metrics = getMetricSingle("arangodb_requests_memory_usage");
          assertTrue(metrics > 0);
        }    
      }
    };
  };
}());
