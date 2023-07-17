import React from "react";
import { FormField } from "../../../components/form/FormField";
import { CLUSTER_GRAPH_FIELDS_MAP } from "../GraphsHelpers";
import { useGraphsModeContext } from "../GraphsModeContext";

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
          ...CLUSTER_GRAPH_FIELDS_MAP.minReplicationFactor,
          isDisabled
        }}
      />
    </>
  );
};
