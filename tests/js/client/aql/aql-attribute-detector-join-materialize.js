/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Julia Puget
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const internal = require("internal");
const db = require("@arangodb").db;

function attributeDetectorJoinMaterializeTestSuite() {
  const cnOrders = "UnitTestsAttrDetOrders";
  const cnProducts = "UnitTestsAttrDetProducts";
  const cnLarge = "UnitTestsAttrDetLarge";
  let ordersCollection;
  let productsCollection;
  let largeCollection;

  const explainAbac = function (query, bindVars, options) {
    const explainRes = db._createStatement({
      query: query,
      bindVars: bindVars || {},
      options: options || {}
    }).explain();
    return {
      accesses: explainRes.abacAccesses || [],
      plan: explainRes.plan
    };
  };

  const containsReadAttr = function (access, attrPath) {
    const expected = Array.isArray(attrPath) ? attrPath : [attrPath];
    const attrs = (access && access.read && access.read.attributes) || [];
    return attrs.some(a =>
      Array.isArray(a) &&
      a.length === expected.length &&
      a.every((seg, i) => seg === expected[i])
    );
  };

  const findNodeInPlan = function (plan, nodeType) {
    const nodes = plan.nodes || [];
    for (let i = 0; i < nodes.length; i++) {
      if (nodes[i].type === nodeType) {
        return nodes[i];
      }
    }
    return null;
  };

  const hasNodeInPlan = function (plan, nodeType) {
    return findNodeInPlan(plan, nodeType) !== null;
  };

  return {
    setUp: function () {
      internal.db._drop(cnOrders);
      internal.db._drop(cnProducts);
      internal.db._drop(cnLarge);

      ordersCollection = internal.db._create(cnOrders);
      productsCollection = internal.db._create(cnProducts);
      largeCollection = internal.db._create(cnLarge);

      // Sorted indexes required for JoinNode (merge-join needs sorted data)
      ordersCollection.ensureIndex({ type: "persistent", fields: ["productId"], storedValues: ["customerId", "orderDate"] });
      productsCollection.ensureIndex({ type: "persistent", fields: ["_key"], storedValues: ["name", "category", "price"] });

      // Index with stored values for late materialization
      largeCollection.ensureIndex({
        type: "persistent",
        fields: ["sortKey"],
        storedValues: ["name", "category"]
      });

      for (let i = 0; i < 20; ++i) {
        ordersCollection.insert({
          _key: `order${i}`,
          productId: `prod${i % 5}`,
          customerId: `cust${i % 3}`,
          quantity: i + 1,
          orderDate: `2026-01-${(i % 28) + 1}`
        });
      }

      for (let i = 0; i < 10; ++i) {
        productsCollection.insert({
          _key: `prod${i}`,
          name: `Product ${i}`,
          category: `cat${i % 3}`,
          price: (i + 1) * 10.5,
          stock: 100 - i * 5
        });
      }

      for (let i = 0; i < 100; ++i) {
        largeCollection.insert({
          _key: `large${i}`,
          sortKey: i,
          name: `Item ${i}`,
          category: `cat${i % 5}`,
          payload: { nested: i, extra: `data${i}` }
        });
      }
    },

    tearDown: function () {
      internal.db._drop(cnOrders);
      internal.db._drop(cnProducts);
      internal.db._drop(cnLarge);
    },

    // JoinNode tests - merge-join of two index scans
    testJoin_TwoCollectionsMergeJoin: function () {
      const query = `
        FOR o IN ${cnOrders}
          FOR p IN ${cnProducts}
            FILTER o.productId == p._key
            RETURN {orderId: o._key, productName: p.name}
      `;
      const { accesses, plan } = explainAbac(query);

      // Verify JoinNode was created
      assertTrue(hasNodeInPlan(plan, "JoinNode"),
        "Expected JoinNode in plan. Got nodes: " + plan.nodes.map(n => n.type).join(", "));

      assertEqual(2, accesses.length);

      let orderAccess = accesses.find(a => a.collection === cnOrders);
      let productAccess = accesses.find(a => a.collection === cnProducts);

      assertTrue(orderAccess !== undefined);
      assertTrue(productAccess !== undefined);

      assertTrue(containsReadAttr(orderAccess, "productId"));
      assertTrue(containsReadAttr(orderAccess, "_key"));
      assertFalse(orderAccess.read.requiresAll);
      assertFalse(orderAccess.write.requiresAll);

      assertTrue(containsReadAttr(productAccess, "_key"));
      assertTrue(containsReadAttr(productAccess, "name"));
      assertFalse(productAccess.read.requiresAll);
      assertFalse(productAccess.write.requiresAll);
    },

    testJoin_ReturnFullDocuments: function () {
      const query = `
        FOR o IN ${cnOrders}
          FOR p IN ${cnProducts}
            FILTER o.productId == p._key
            RETURN {productName: p.name, order: o}
      `;

      const { accesses, plan } = explainAbac(query);

      assertTrue(hasNodeInPlan(plan, "JoinNode"),
        "Expected JoinNode. Got: " + plan.nodes.map(n => n.type).join(", "));

      assertEqual(2, accesses.length);
      const orderAccess = accesses.find(a => a.collection === cnOrders);
      const productAccess = accesses.find(a => a.collection === cnProducts);

      assertTrue(!!orderAccess);
      assertTrue(!!productAccess);

      assertTrue(containsReadAttr(productAccess, "name"));
      assertTrue(containsReadAttr(productAccess, "_key"));

      assertTrue(orderAccess.read.requiresAll);
      assertFalse(productAccess.read.requiresAll);
    },

    testJoin_MultipleAttributesFromBothSides: function () {
      const query = `
        FOR o IN ${cnOrders}
          FOR p IN ${cnProducts}
            FILTER o.productId == p._key
            RETURN {
              orderId: o._key,
              qty: o.quantity,
              productName: p.name,
              productPrice: p.price
            }
      `;
      const { accesses, plan } = explainAbac(query);

      assertTrue(hasNodeInPlan(plan, "JoinNode"),
        "Expected JoinNode. Got: " + plan.nodes.map(n => n.type).join(", "));

      assertEqual(2, accesses.length);

      let orderAccess = accesses.find(a => a.collection === cnOrders);
      let productAccess = accesses.find(a => a.collection === cnProducts);

      assertTrue(containsReadAttr(orderAccess, "productId"));
      assertTrue(containsReadAttr(orderAccess, "_key"));
      assertTrue(containsReadAttr(orderAccess, "quantity"));

      assertTrue(containsReadAttr(productAccess, "_key"));
      assertTrue(containsReadAttr(productAccess, "name"));
      assertTrue(containsReadAttr(productAccess, "price"));

      assertFalse(orderAccess.read.requiresAll);
      assertFalse(productAccess.read.requiresAll);
    },

    testJoin_WithAdditionalFilter: function () {
      const query = `
        FOR o IN ${cnOrders}
          FOR p IN ${cnProducts}
            FILTER o.productId == p._key
            FILTER p.category == "cat1"
            RETURN {orderId: o._key, productName: p.name}
      `;
      const { accesses, plan } = explainAbac(query);

      assertTrue(hasNodeInPlan(plan, "JoinNode"),
        "Expected JoinNode. Got: " + plan.nodes.map(n => n.type).join(", "));

      assertEqual(2, accesses.length);

      let orderAccess = accesses.find(a => a.collection === cnOrders);
      let productAccess = accesses.find(a => a.collection === cnProducts);

      assertTrue(containsReadAttr(orderAccess, "productId"));
      assertTrue(containsReadAttr(orderAccess, "_key"));
      assertTrue(containsReadAttr(productAccess, "_key"));
      assertTrue(containsReadAttr(productAccess, "category"));
      assertTrue(containsReadAttr(productAccess, "name"));
      assertFalse(productAccess.read.requiresAll);
      assertFalse(orderAccess.read.requiresAll);
    },

    testJoin_WithSort: function () {
      const query = `
        FOR o IN ${cnOrders}
          FOR p IN ${cnProducts}
            FILTER o.productId == p._key
            SORT p.price DESC
            RETURN {orderId: o._key, price: p.price}
      `;
      const { accesses, plan } = explainAbac(query);

      assertTrue(hasNodeInPlan(plan, "JoinNode"),
        "Expected JoinNode. Got: " + plan.nodes.map(n => n.type).join(", "));

      assertEqual(2, accesses.length);

      let orderAccess = accesses.find(a => a.collection === cnOrders);
      let productAccess = accesses.find(a => a.collection === cnProducts);

      assertTrue(containsReadAttr(orderAccess, "productId"));
      assertTrue(containsReadAttr(orderAccess, "_key"));
      assertTrue(containsReadAttr(productAccess, "_key"));
      assertTrue(containsReadAttr(productAccess, "price"));
      assertFalse(orderAccess.read.requiresAll);
      assertFalse(productAccess.read.requiresAll);
    },

    testJoin_WithCollect: function () {
      const query = `
        FOR o IN ${cnOrders}
          FOR p IN ${cnProducts}
            FILTER o.productId == p._key
            COLLECT category = p.category WITH COUNT INTO cnt
            RETURN {category, cnt}
      `;
      const { accesses, plan } = explainAbac(query);

      assertTrue(hasNodeInPlan(plan, "JoinNode"),
        "Expected JoinNode. Got: " + plan.nodes.map(n => n.type).join(", "));

      assertEqual(2, accesses.length);

      let orderAccess = accesses.find(a => a.collection === cnOrders);
      let productAccess = accesses.find(a => a.collection === cnProducts);

      assertTrue(containsReadAttr(orderAccess, "productId"));
      assertTrue(containsReadAttr(productAccess, "_key"));
      assertTrue(containsReadAttr(productAccess, "category"));
      assertFalse(orderAccess.read.requiresAll);
      assertFalse(productAccess.read.requiresAll);
    },

    testJoin_SelfJoin: function () {
      const query = `
        FOR o1 IN ${cnOrders}
          FOR o2 IN ${cnOrders}
            FILTER o1.productId == o2.productId
            SORT o1.productId
            RETURN {c: o1.customerId, k1: o1._key, k2: o2._key}
      `;
      const { accesses, plan } = explainAbac(query);

      assertTrue(hasNodeInPlan(plan, "JoinNode"),
        "Expected JoinNode. Got: " + plan.nodes.map(n => n.type).join(", "));

      assertEqual(1, accesses.length);
      assertEqual(cnOrders, accesses[0].collection);

      assertTrue(containsReadAttr(accesses[0], "productId"));
      assertTrue(containsReadAttr(accesses[0], "_key"));
      assertTrue(containsReadAttr(accesses[0], "customerId"));
      assertFalse(accesses[0].read.requiresAll);
    },

    // MaterializeNode tests - late document materialization
    testMaterialize_SortLimitPattern: function () {
      // Pattern: INDEX scan -> SORT -> LIMIT triggers late materialization
      // Without a FILTER using the index, optimizer may do full collection scan
      const query = `
        FOR doc IN ${cnLarge}
          FILTER doc.sortKey >= 0
          SORT doc.sortKey
          LIMIT 10
          RETURN doc.name
      `;
      const { accesses, plan } = explainAbac(query);

      // Verify MaterializeNode was created - this is required to test the MaterializeNode handler
      assertTrue(hasNodeInPlan(plan, "MaterializeNode"),
        "Expected MaterializeNode in plan. Got nodes: " + plan.nodes.map(n => n.type).join(", "));

      assertEqual(1, accesses.length);
      assertEqual(cnLarge, accesses[0].collection);

      assertTrue(containsReadAttr(accesses[0], "sortKey"));
      assertTrue(containsReadAttr(accesses[0], "name"));
      assertFalse(accesses[0].read.requiresAll);
    },

    testMaterialize_SortLimitReturnFullDoc: function () {
      const query = `
        FOR doc IN ${cnLarge}
          SORT doc.sortKey
          LIMIT 10
          RETURN doc
      `;
      const { accesses } = explainAbac(query);

      assertEqual(1, accesses.length);
      assertTrue(accesses[0].read.requiresAll);
    },

    testMaterialize_SortLimitMultipleAttributes: function () {
      // Add FILTER to trigger index usage
      const query = `
        FOR doc IN ${cnLarge}
          FILTER doc.sortKey >= 0
          SORT doc.sortKey DESC
          LIMIT 5
          RETURN {name: doc.name, category: doc.category, key: doc._key}
      `;
      const { accesses } = explainAbac(query);

      assertEqual(1, accesses.length);
      assertTrue(containsReadAttr(accesses[0], "sortKey"));
      assertTrue(containsReadAttr(accesses[0], "name"));
      assertTrue(containsReadAttr(accesses[0], "category"));
      assertTrue(containsReadAttr(accesses[0], "_key"));
      assertFalse(accesses[0].read.requiresAll);
    },

    testMaterialize_WithFilter: function () {
      const query = `
        FOR doc IN ${cnLarge}
          FILTER doc.sortKey >= 10
          SORT doc.sortKey
          LIMIT 5
          RETURN doc.name
      `;
      const { accesses } = explainAbac(query);

      assertEqual(1, accesses.length);
      assertTrue(containsReadAttr(accesses[0], "sortKey"));
      assertTrue(containsReadAttr(accesses[0], "name"));
      assertFalse(accesses[0].read.requiresAll);
    },

    testMaterialize_NestedAttributeAccess: function () {
      // Add FILTER to trigger index usage
      const query = `
        FOR doc IN ${cnLarge}
          FILTER doc.sortKey >= 0
          SORT doc.sortKey
          LIMIT 10
          RETURN doc.payload.nested
      `;
      const { accesses } = explainAbac(query);

      assertEqual(1, accesses.length);
      assertTrue(containsReadAttr(accesses[0], "sortKey"));
      assertTrue(containsReadAttr(accesses[0], ["payload", "nested"]));
      assertFalse(accesses[0].read.requiresAll);
    },

    testMaterialize_WithCalculation: function () {
      const query = `
        FOR doc IN ${cnLarge}
          SORT doc.sortKey
          LIMIT 10
          LET doubled = doc.sortKey * 2
          RETURN {name: doc.name, doubled}
      `;
      const { accesses } = explainAbac(query);

      assertEqual(1, accesses.length);
      assertTrue(containsReadAttr(accesses[0], "sortKey"));
      assertTrue(containsReadAttr(accesses[0], "name"));
      assertFalse(accesses[0].read.requiresAll);
    },

    // Combined patterns
    testJoinWithMaterialize_JoinThenSortLimit: function () {
      const query = `
        FOR o IN ${cnOrders}
          FOR p IN ${cnProducts}
            FILTER o.productId == p._key
            SORT p.price DESC
            LIMIT 5
            RETURN {orderId: o._key, productName: p.name, price: p.price}
      `;
      const { accesses } = explainAbac(query);

      assertEqual(2, accesses.length);

      let orderAccess = accesses.find(a => a.collection === cnOrders);
      let productAccess = accesses.find(a => a.collection === cnProducts);

      assertTrue(containsReadAttr(orderAccess, "productId"));
      assertTrue(containsReadAttr(orderAccess, "_key"));

      assertTrue(containsReadAttr(productAccess, "_key"));
      assertTrue(containsReadAttr(productAccess, "name"));
      assertTrue(containsReadAttr(productAccess, "price"));

      assertFalse(orderAccess.read.requiresAll);
      assertFalse(productAccess.read.requiresAll);
    },

    testJoinWithCollectAggregate: function () {
      const query = `
        FOR o IN ${cnOrders}
          FOR p IN ${cnProducts}
            FILTER o.productId == p._key
            COLLECT category = p.category
            AGGREGATE totalQty = SUM(o.quantity), avgPrice = AVG(p.price)
            RETURN {category, totalQty, avgPrice}
      `;
      const { accesses } = explainAbac(query);

      assertEqual(2, accesses.length);

      let orderAccess = accesses.find(a => a.collection === cnOrders);
      let productAccess = accesses.find(a => a.collection === cnProducts);

      assertTrue(containsReadAttr(orderAccess, "productId"));
      assertTrue(containsReadAttr(orderAccess, "quantity"));

      assertTrue(containsReadAttr(productAccess, "_key"));
      assertTrue(containsReadAttr(productAccess, "category"));
      assertTrue(containsReadAttr(productAccess, "price"));

      assertFalse(orderAccess.read.requiresAll);
      assertFalse(productAccess.read.requiresAll);
    },

    // Edge cases
    testJoin_ThreeWayJoin: function () {
      // Third collection for three-way join
      const cnCustomers = "UnitTestsAttrDetCustomers";
      try {
        let customersCollection = internal.db._create(cnCustomers);
        customersCollection.ensureIndex({ type: "persistent", fields: ["_key"] });

        for (let i = 0; i < 5; ++i) {
          customersCollection.insert({
            _key: `cust${i}`,
            name: `Customer ${i}`,
            region: `region${i % 2}`
          });
        }

        const query = `
          FOR o IN ${cnOrders}
            FOR p IN ${cnProducts}
              FOR c IN ${cnCustomers}
                FILTER o.productId == p._key
                FILTER o.customerId == c._key
                RETURN {
                  orderId: o._key,
                  productName: p.name,
                  customerName: c.name
                }
        `;
        const { accesses } = explainAbac(query);

        assertEqual(3, accesses.length);

        let orderAccess = accesses.find(a => a.collection === cnOrders);
        let productAccess = accesses.find(a => a.collection === cnProducts);
        let customerAccess = accesses.find(a => a.collection === cnCustomers);

        assertTrue(orderAccess !== undefined);
        assertTrue(productAccess !== undefined);
        assertTrue(customerAccess !== undefined);

        assertTrue(containsReadAttr(orderAccess, "productId"));
        assertTrue(containsReadAttr(orderAccess, "customerId"));
        assertTrue(containsReadAttr(orderAccess, "_key"));

        assertTrue(containsReadAttr(productAccess, "_key"));
        assertTrue(containsReadAttr(productAccess, "name"));

        assertTrue(containsReadAttr(customerAccess, "_key"));
        assertTrue(containsReadAttr(customerAccess, "name"));

        assertFalse(orderAccess.read.requiresAll);
        assertFalse(productAccess.read.requiresAll);
        assertFalse(customerAccess.read.requiresAll);
      } finally {
        internal.db._drop(cnCustomers);
      }
    },

    testMaterialize_LargeOffset: function () {
      // Add FILTER to trigger index usage
      const query = `
        FOR doc IN ${cnLarge}
          FILTER doc.sortKey >= 0
          SORT doc.sortKey
          LIMIT 50, 10
          RETURN doc.name
      `;
      const { accesses } = explainAbac(query);

      assertEqual(1, accesses.length);
      assertTrue(containsReadAttr(accesses[0], "sortKey"));
      assertTrue(containsReadAttr(accesses[0], "name"));
      assertFalse(accesses[0].read.requiresAll);
    },

    testJoin_WithSubquery: function () {
      // Subquery pattern - in cluster mode, inner query may need full scan
      const query = `
        FOR p IN ${cnProducts}
          LET orderCount = (
            FOR o IN ${cnOrders}
              FILTER o.productId == p._key
              RETURN 1
          )
          RETURN {productName: p.name, orders: LENGTH(orderCount)}
      `;
      const { accesses } = explainAbac(query);

      assertEqual(2, accesses.length);

      let orderAccess = accesses.find(a => a.collection === cnOrders);
      let productAccess = accesses.find(a => a.collection === cnProducts);

      assertTrue(orderAccess !== undefined);
      assertTrue(productAccess !== undefined);

      // Orders collection needs productId for filter
      if (!orderAccess.read.requiresAll) {
        assertTrue(containsReadAttr(orderAccess, "productId"));
      }
      // Products collection needs _key and name
      assertTrue(containsReadAttr(productAccess, "_key") || productAccess.read.requiresAll);
      assertTrue(containsReadAttr(productAccess, "name") || productAccess.read.requiresAll);
    },

    testJoin_OptionalMatch: function () {
      // Left outer join pattern
      const query = `
        FOR p IN ${cnProducts}
          LET orders = (
            FOR o IN ${cnOrders}
              FILTER o.productId == p._key
              RETURN o.quantity
          )
          RETURN {product: p.name, quantities: orders}
      `;
      const { accesses } = explainAbac(query);

      assertEqual(2, accesses.length);

      let orderAccess = accesses.find(a => a.collection === cnOrders);
      let productAccess = accesses.find(a => a.collection === cnProducts);

      assertTrue(containsReadAttr(orderAccess, "productId"));
      assertTrue(containsReadAttr(orderAccess, "quantity"));
      assertTrue(containsReadAttr(productAccess, "_key"));
      assertTrue(containsReadAttr(productAccess, "name"));
    },

    // Verify node type detection in plan - these tests REQUIRE the specific nodes
    testVerifyJoinNodeInPlan: function () {
      const query = `
        FOR o IN ${cnOrders}
          FOR p IN ${cnProducts}
            FILTER o.productId == p._key
            RETURN {o: o._key, p: p._key}
      `;
      const { plan } = explainAbac(query);

      // JoinNode MUST be present - this test verifies the optimizer creates it
      const nodes = plan.nodes.map(n => n.type);
      assertTrue(nodes.includes("JoinNode"),
        "Expected JoinNode in plan. Got nodes: " + nodes.join(", "));
    },

    testVerifyMaterializeNodeInPlan: function () {
      const query = `
        FOR doc IN ${cnLarge}
          FILTER doc.sortKey >= 0
          SORT doc.sortKey
          LIMIT 10
          RETURN doc.name
      `;
      const { plan } = explainAbac(query);

      // MaterializeNode MUST be present - this test verifies the optimizer creates it
      const nodes = plan.nodes.map(n => n.type);
      assertTrue(nodes.includes("MaterializeNode"),
        "Expected MaterializeNode in plan. Got nodes: " + nodes.join(", "));
    }
  };
}

jsunity.run(attributeDetectorJoinMaterializeTestSuite);

return jsunity.done();
