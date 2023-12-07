import { VStack } from "@chakra-ui/react";
import { Form, Formik } from "formik";
import React from "react";
import { mutate } from "swr";
import { FieldsGrid } from "../../../components/form/FieldsGrid";
import { FormField } from "../../../components/form/FormField";
import {
  CLUSTER_GRAPH_FIELDS_MAP,
  createGraph,
  GENERAL_GRAPH_FIELDS_MAP,
  GRAPH_VALIDATION_SCHEMA,
  updateGraph
} from "../listGraphs/graphListHelpers";
import { useGraphsModeContext } from "../listGraphs/GraphsModeContext";
import { ClusterFields } from "./ClusterFields";
import { EnterpriseGraphCreateValues } from "./CreateGraph.types";
import { EdgeDefinitionsField } from "./EdgeDefinitionsField";
import { GraphModalFooter } from "./GraphModalFooter";
import { GraphWarnings } from "./GraphWarnings";

const enterpriseGraphFieldsMap = {
  name: GENERAL_GRAPH_FIELDS_MAP.name,
  orphanCollections: GENERAL_GRAPH_FIELDS_MAP.orphanCollections
};

const INITIAL_VALUES: EnterpriseGraphCreateValues = {
  name: "",
  edgeDefinitions: [
    {
      from: [],
      to: [],
      collection: ""
    }
  ],
  orphanCollections: [],
  isSmart: true
};

export const EnterpriseGraphForm = ({ onClose }: { onClose: () => void }) => {
  const { initialGraph, mode } = useGraphsModeContext();
  const isEditMode = mode === "edit";

  const handleSubmit = async (values: EnterpriseGraphCreateValues) => {
    const sanitizedValues = {
      name: values.name,
      orphanCollections: values.orphanCollections,
      edgeDefinitions: values.edgeDefinitions,
      options: {
        isSmart: true,
        numberOfShards: Number(values.numberOfShards),
        replicationFactor: Number(values.replicationFactor),
        writeConcern: Number(values.writeConcern),
        satellites: values.satellites
      }
    };
    if (isEditMode) {
      const info = await updateGraph({
        values: sanitizedValues,
        initialGraph,
        onSuccess: () => {
          mutate("/graphs");
          onClose();
        }
      });
      return info;
    }
    const info = await createGraph({
      values: sanitizedValues,
      onSuccess: () => {
        mutate("/graphs");
        onClose();
      }
    });
    return info;
  };
  return (
    <Formik
      initialValues={initialGraph || INITIAL_VALUES}
      validationSchema={GRAPH_VALIDATION_SCHEMA}
      onSubmit={handleSubmit}
    >
      <Form>
        <VStack spacing={4} align="stretch">
          <GraphWarnings />
          <FieldsGrid maxWidth="full">
            <FormField
              field={{
                ...enterpriseGraphFieldsMap.name,
                isDisabled: isEditMode
              }}
            />
            <ClusterFields isShardsRequired />
            {(window.frontendConfig.isCluster || true) && (
              <FormField
                field={{
                  ...CLUSTER_GRAPH_FIELDS_MAP.satellites,
                  label: isEditMode
                    ? "New satellite collections"
                    : CLUSTER_GRAPH_FIELDS_MAP.satellites.label,
                  noOptionsMessage: () =>
                    "Please enter a new and valid collection name"
                }}
              />
            )}
            <EdgeDefinitionsField
              noOptionsMessage={() =>
                "Please enter a new and valid collection name"
              }
              allowExistingCollections={false}
            />
            <FormField field={enterpriseGraphFieldsMap.orphanCollections} />
          </FieldsGrid>
          <GraphModalFooter onClose={onClose} />
        </VStack>
      </Form>
    </Formik>
  );
};
