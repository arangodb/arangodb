import React from "react";
import { FormField } from "../../../components/form/FormField";
import { CLUSTER_GRAPH_FIELDS_MAP } from "../listGraphs/GraphsHelpers";
import { useGraphsModeContext } from "../listGraphs/GraphsModeContext";

export const ClusterFields = ({
  isShardsRequired
}: {
  isShardsRequired: boolean;
}) => {
  const { mode } = useGraphsModeContext();
  const isDisabled = mode === "edit";
  return (
    <>
      <FormField
        field={{
          ...CLUSTER_GRAPH_FIELDS_MAP.numberOfShards,
          isDisabled,
          isRequired: isShardsRequired
        }}
      />
      <FormField
        field={{
          ...CLUSTER_GRAPH_FIELDS_MAP.replicationFactor,
          isDisabled
        }}
      />
      <FormField
        field={{
          ...CLUSTER_GRAPH_FIELDS_MAP.writeConcern,
          isDisabled
        }}
      />
    </>
  );
};
