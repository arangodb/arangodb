
export const commonFieldsMap = {
  fields: {
    label: "Fields",
    name: "fields",
    type: "text",
    isRequired: true,
    tooltip: "A comma-separated list of attribute paths.",
    initialValue: ""
  },
  name: {
    label: "Name",
    name: "name",
    type: "text",
    tooltip: "Index name. If left blank, a name will be auto-generated. Example: byValue",
    initialValue: ""
  },
  inBackground: {
    label: "Create in background",
    name: "inBackground",
    type: "boolean",
    initialValue: true,
    tooltip: "Create the index in background."
  }
};
const commonPersistentIndexFieldsMap = {
  unique: {
    label: "Unique",
    name: "unique",
    type: "boolean",
    tooltip: "If true, then create a unique index.",
    initialValue: false
  },
  sparse: {
    label: "Sparse",
    name: "sparse",
    type: "boolean",
    tooltip: "If true, then create a sparse index. Sparse indexes do not include documents for which the index attributes do not exist or have a value of null. Sparse indexes will exclude any such documents, so they can save space, but are less versatile than non-sparse indexes. For example, sparse indexes may not be usable for sort operations, range or point lookup queries if the query optimizer cannot prove that the index query range excludes null values.",
    initialValue: false
  },
  deduplicate: {
    label: "Deduplicate array values",
    name: "deduplicate",
    type: "boolean",
    tooltip: "Duplicate index values from the same document into a unique array index will lead to an error or not.",
    initialValue: false
  },
  estimates: {
    label: "Maintain index selectivity estimates",
    name: "estimates",
    type: "boolean",
    tooltip: "Maintain index selectivity estimates for this index. This option can be turned off for non-unique-indexes to improve efficiency of write operations, but will lead to the optimizer being unaware of the data distribution inside this index.",
    initialValue: true
  },
  cacheEnabled: {
    label: "Enable in-memory cache for index lookups",
    name: "cacheEnabled",
    type: "boolean",
    tooltip: "Cache index lookup values in memory for faster lookups of the same values. Note that the cache will be initially empty and be populated lazily during lookups. The cache can only be used for equality lookups on all index attributes. It cannot be used for range scans, partial lookups or sorting.",
    initialValue: false
  }
};


export const persistentIndexInitialValues = {
  fields: commonFieldsMap.fields.initialValue,
  name: commonFieldsMap.fields.initialValue,
  unique: commonPersistentIndexFieldsMap.unique.initialValue,
  sparse: commonPersistentIndexFieldsMap.sparse.initialValue,
  deduplicate: commonPersistentIndexFieldsMap.deduplicate.initialValue,
  estimates: commonPersistentIndexFieldsMap.estimates.initialValue,
  cacheEnabled: commonPersistentIndexFieldsMap.cacheEnabled.initialValue,
  inBackground: commonFieldsMap.inBackground.initialValue
};

export const persistentIndexFields = [
  commonFieldsMap.fields,
  commonFieldsMap.name,
  commonPersistentIndexFieldsMap.unique,
  commonPersistentIndexFieldsMap.sparse,
  commonPersistentIndexFieldsMap.deduplicate,
  commonPersistentIndexFieldsMap.estimates,
  commonPersistentIndexFieldsMap.cacheEnabled,
  commonFieldsMap.inBackground
];
