const arangoSearchFieldsMap = {
  links: {
    label: "Links",
    name: "links",
    isRequired: false,
    type: "custom",
    group: "links"
  },
  writebufferIdle: {
    isDisabled: true,
    label: "Writebuffer Idle",
    name: "writebufferIdle",
    type: "number",
    group: "general",
    tooltip:
      "Maximum number of writers (segments) cached in the pool (default: 64, use 0 to disable, immutable)."
  },
  writebufferActive: {
    isDisabled: true,
    label: "Writebuffer Active",
    name: "writebufferActive",
    type: "number",
    group: "general",
    tooltip:
      "Maximum number of concurrent active writers (segments) that perform a transaction. Other writers (segments) wait till current active writers (segments) finish (default: 0, use 0 to disable, immutable)."
  },
  writebufferSizeMax: {
    isDisabled: true,
    label: "Writebuffer Size Max",
    name: "writebufferSizeMax",
    type: "number",
    group: "general",
    tooltip:
      "Maximum memory byte size per writer (segment) before a writer (segment) flush is triggered. 0 value turns off this limit for any writer (buffer) and data will be flushed periodically based on the value defined for the flush thread (ArangoDB server startup option). 0 value should be used carefully due to high potential memory consumption (default: 33554432, use 0 to disable, immutable)."
  },
  cleanupIntervalStep: {
    label: "Cleanup Interval Step",
    name: "cleanupIntervalStep",
    type: "number",
    group: "general",
    tooltip:
      "ArangoSearch waits at least this many commits between removing unused files in its data directory."
  },
  commitIntervalMsec: {
    label: "Commit Interval (msec)",
    name: "commitIntervalMsec",
    type: "number",
    group: "general",
    tooltip:
      "Wait at least this many milliseconds between committing View data store changes and making documents visible to queries."
  },
  consolidationIntervalMsec: {
    label: "Consolidation Interval (msec)",
    name: "consolidationIntervalMsec",
    type: "number",
    group: "general",
    tooltip:
      "Wait at least this many milliseconds between index segments consolidations."
  },
  consolidationPolicy: {
    label: "Consolidation Policy",
    name: "consolidationPolicy",
    type: "custom",
    group: "consolidationPolicy"
  },
  primarySort: {
    label: "Primary Sort",
    name: "primarySort",
    type: "custom",
    group: "primarySort"
  },
  primaryKeyCache: {
    isDisabled: true,
    label: "Primary Key Cache",
    name: "primaryKeyCache",
    type: "boolean",
    group: "general",
    tooltip: "Always cache primary key columns in memory."
  },
  storedValues: {
    label: "Stored Values",
    name: "storedValues",
    type: "custom",
    group: "storedValues"
  }
};

type FieldType = {
  label: string;
  name: string;
  type: string;
  initialValue?: string | number | boolean;
  group?: string;
  tooltip?: string;
  isDisabled?: boolean;
  isRequired?: boolean;
};

const arangoSearchFields = [
  arangoSearchFieldsMap.links,
  {
    label: "Name",
    name: "name",
    type: "text",
    initialValue: "",
    group: "name"
  },
  arangoSearchFieldsMap.primarySort,
  arangoSearchFieldsMap.storedValues,
  arangoSearchFieldsMap.primaryKeyCache,
  arangoSearchFieldsMap.writebufferIdle,
  arangoSearchFieldsMap.writebufferActive,
  arangoSearchFieldsMap.writebufferSizeMax,
  arangoSearchFieldsMap.cleanupIntervalStep,
  arangoSearchFieldsMap.commitIntervalMsec,
  arangoSearchFieldsMap.consolidationIntervalMsec,
  arangoSearchFieldsMap.consolidationPolicy
] as FieldType[];

const tierConsolidationPolicyFields = [
  {
    name: "consolidationPolicy.segmentsMin",
    label: "Segments Min",
    type: "number",
    tooltip:
      "The minimum number of segments that will be evaluated as candidates for consolidation."
  },
  {
    name: "consolidationPolicy.segmentsMax",
    label: "Segments Max",
    type: "number",
    tooltip:
      "The maximum number of segments that will be evaluated as candidates for consolidation."
  },
  {
    name: "consolidationPolicy.segmentsBytesMax",
    label: "Segments Bytes Max",
    type: "number",
    tooltip: "Maximum allowed size of all consolidated segments in bytes."
  },
  {
    name: "consolidationPolicy.segmentsBytesFloor",
    label: "Segments Bytes Floor",
    type: "number",
    tooltip:
      "Defines the value (in bytes) to treat all smaller segments as equal for consolidation selection."
  }
];
const bytesAccumConsolidationPolicyFields = [
  {
    name: "consolidationPolicy.threshold",
    label: "Threshold",
    type: "number",
    tooltip:
      "Consolidation is performed on segments which accumulated size in bytes is less than all segments’ byte size multiplied by the threshold."
  }
];

export const useArangoSearchFieldsData = () => {
  return {
    fields: arangoSearchFields,
    tierConsolidationPolicyFields,
    bytesAccumConsolidationPolicyFields
  };
};
