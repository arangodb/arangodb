// Tests for the /_admin/wal/* endpoints.
//
// The /_admin/wal prefix is handled by two different implementations
// depending on the storage engine / deployment mode:
//
//   RocksDBRestWalHandler   (single-server and DB-server; RocksDB engine)
//     Registered in RocksDBEngine/RocksDBRestHandlers.cpp
//   ClusterRestWalHandler   (coordinator; registered in ClusterRestHandlers.cpp)
//
// Both handlers extend RestBaseHandler (no mandatory _system DB context).
// Whether the general routing layer enforces _system access for /_admin/*
// RestBaseHandler routes depends on server internals; in practice AU and
// AN may reach the handler rather than being rejected with 401.
//
// Per-operation auth checks
// ──────────────────────────
// GET  /properties         – no in-handler auth check; returns 501 NOT_IMPL
//                            (RocksDB does not expose legacy WAL properties)
// PUT  /properties         – same; returns 501 NOT_IMPL
// GET  /transactions       – no in-handler auth check; returns 501 NOT_IMPL
//                            (running transaction count is in the response body
//                             of the 501, but the HTTP status is 501)
// PUT  /flush              – no in-handler auth check
//                            Single/DBserver: flushes RocksDB WAL → 200
//                            Coordinator:     delegates flush to DB-servers → 200
// PUT  /wait_for_estimator_sync
//   Production build:   !isSuperuser → 403
//   Maintainer build:   canUseAdmin(WalAccess) → without RBAC: RW on _system
//
// Expected (production build, single-server or coordinator)
// ──────────────────────────────────────────────────────────
//  GET  /properties              AU→401, AN→401, AR→501, AW→501, SU→501
//  PUT  /properties              AU→401, AN→401, AR→501, AW→501, SU→501
//  GET  /transactions            AU→401, AN→401, AR→501, AW→501, SU→501
//  PUT  /flush                   AU→401, AN→401, AR→200, AW→200, SU→200
//  PUT  /wait_for_estimator_sync AU→401, AN→401, AR→403, AW→403, SU→200

export default [

  // ── GET /_admin/wal/properties ────────────────────────────────────────────
  // No in-handler auth check.  RocksDB does not support the legacy WAL
  // properties format; both RocksDB and cluster handlers return 501.
  // Expected (prod, single/coord): AU→401, AN→401, AR→501, AW→501, SU→501
  {
    name: "Get WAL properties (GET /_admin/wal/properties)",
    type: "admin",
    method: "GET",
    path: "/_admin/wal/properties",
  },

  // ── PUT /_admin/wal/properties ────────────────────────────────────────────
  // No in-handler auth check.  Returns 501 (same reason as GET).
  // Expected: AU→401, AN→401, AR→501, AW→501, SU→501
  {
    name: "Set WAL properties (PUT /_admin/wal/properties)",
    type: "admin",
    method: "PUT",
    path: "/_admin/wal/properties",
    body: {},
  },

  // ── GET /_admin/wal/transactions ──────────────────────────────────────────
  // No in-handler auth check.  Returns the active transaction count wrapped
  // in HTTP 501 (the cluster handler marks this as NOT_IMPLEMENTED).
  // Expected: AU→401, AN→401, AR→501, AW→501, SU→501
  {
    name: "Get WAL transactions (GET /_admin/wal/transactions)",
    type: "admin",
    method: "GET",
    path: "/_admin/wal/transactions",
  },

  // ── PUT /_admin/wal/flush ─────────────────────────────────────────────────
  // No in-handler auth check.
  // Single/DBserver: flushes the RocksDB WAL → 200 {}.
  // Coordinator:     delegates to all DB servers → 200 {}.
  // Expected: AU→401, AN→401, AR→200, AW→200, SU→200
  {
    name: "Flush WAL (PUT /_admin/wal/flush)",
    type: "admin",
    method: "PUT",
    path: "/_admin/wal/flush",
    body: {},
  },

  // ── PUT /_admin/wal/wait_for_estimator_sync ───────────────────────────────
  // Production build: !isSuperuser → 403 (all named users regardless of role)
  // Maintainer build: canUseAdmin(WalAccess) → without RBAC: RW on _system
  //   → AR→403, AW→200, SU→200
  // Expected (prod): AU→401, AN→401, AR→403, AW→403, SU→200
  // Expected (maint): AU→401, AN→401, AR→403, AW→403, SU→200
  {
    name: "Wait for estimator sync (PUT /_admin/wal/wait_for_estimator_sync)",
    type: "admin",
    method: "PUT",
    path: "/_admin/wal/wait_for_estimator_sync",
  },

];
