import * as Yup from "yup";

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
      "Index waits at least this many commits between removing unused files in its data directory."
  },
  commitIntervalMsec: {
    label: "Commit Interval (msec)",
    name: "commitIntervalMsec",
    type: "number",
    group: "general",
    tooltip:
      "Wait at least this many milliseconds between committing data store changes and making documents visible to queries."
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
  storedValues: {
    label: "Stored Values",
    name: "storedValues",
    type: "custom",
    group: "storedValues"
  }
};

const arangoSearchFields = [
  arangoSearchFieldsMap.links,
  {
    label: "Name",
    name: "name",
    type: "text",
    tooltip:
      "Index name. If left blank, the name will be auto-generated. Example: byValue",
    initialValue: "",
    group: "name"
  },
  arangoSearchFieldsMap.primarySort,
  arangoSearchFieldsMap.storedValues,
  arangoSearchFieldsMap.writebufferIdle,
  arangoSearchFieldsMap.writebufferActive,
  arangoSearchFieldsMap.writebufferSizeMax,
  arangoSearchFieldsMap.cleanupIntervalStep,
  arangoSearchFieldsMap.commitIntervalMsec,
  arangoSearchFieldsMap.consolidationIntervalMsec,
  arangoSearchFieldsMap.consolidationPolicy
];

// <FormField
//           field={{
//             name: "consolidationPolicy.segmentsMin",
//             label: "Segments Min",
//             type: "number",
//             isDisabled: field.isDisabled,
//             tooltip:
//               "The minimum number of segments that will be evaluated as candidates for consolidation."
//           }}
//         />
//         <FormField
//           field={{
//             name: "consolidationPolicy.segmentsMax",
//             label: "Segments Max",
//             type: "number",
//             isDisabled: field.isDisabled,
//             tooltip:
//               "The maximum number of segments that will be evaluated as candidates for consolidation."
//           }}
//         />
//         <FormField
//           field={{
//             name: "consolidationPolicy.segmentsBytesMax",
//             label: "Segments Bytes Max",
//             type: "number",
//             isDisabled: field.isDisabled,
//             tooltip:
//               "Maximum allowed size of all consolidated segments in bytes."
//           }}
//         />
//         <FormField
//           field={{
//             name: "consolidationPolicy.segmentsBytesFloor",
//             label: "Segments Bytes Floor",
//             type: "number",
//             isDisabled: field.isDisabled,
//             tooltip:
//               "Defines the value (in bytes) to treat all smaller segments as equal for consolidation selection."
//           }}
//         />
// ^ these should be stored in an array
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
const fieldSchema: any = Yup.object().shape({
  name: Yup.string().required(),
  inBackground: Yup.boolean(),
  analyzer: Yup.string(),
  features: Yup.array().of(Yup.string().required()),
  includeAllFields: Yup.boolean(),
  trackListPositions: Yup.boolean(),
  searchField: Yup.boolean(),
  nested: Yup.array().of(Yup.lazy(() => fieldSchema.default(undefined)))
});

export const useArangoSearchFieldsData = () => {
  return {
    fields: arangoSearchFields,
    tierConsolidationPolicyFields,
    bytesAccumConsolidationPolicyFields
  };
};
