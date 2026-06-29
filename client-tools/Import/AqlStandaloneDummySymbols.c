////////////////////////////////////////////////////////////////////////////////
/// Dummy typeinfo / vtable symbols for classes whose destructors cannot be
/// defined in C++ without pulling in massive destructor cascades.
///
/// These symbols are referenced by vtables emitted in AqlStandaloneStubs.cpp
/// but are never used at runtime (the corresponding classes are never
/// constructed in client tools).  They exist solely to satisfy the ELF loader.
///
/// DO NOT use dynamic_cast or typeid on ClusterFeature, DatabaseFeature,
/// or ReplicationApplier in client tool code paths.
////////////////////////////////////////////////////////////////////////////////

/* typeinfo for arangodb::ClusterFeature  (__si_class_type_info: 3 pointers) */
__attribute__((used)) void* _ZTIN8arangodb14ClusterFeatureE[3] = {0, 0, 0};

/* typeinfo for arangodb::DatabaseFeature */
__attribute__((used)) void* _ZTIN8arangodb15DatabaseFeatureE[3] = {0, 0, 0};

/* typeinfo for arangodb::ReplicationApplier (__class_type_info: 2 pointers) */
__attribute__((used)) void* _ZTIN8arangodb18ReplicationApplierE[2] = {0, 0};

/* vtable for arangodb::ReplicationApplier (offset-to-top + typeinfo + slots) */
__attribute__((used)) void* _ZTVN8arangodb18ReplicationApplierE[32] = {0};
