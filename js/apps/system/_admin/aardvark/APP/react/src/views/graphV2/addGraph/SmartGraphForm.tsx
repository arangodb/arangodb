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
  SMART_GRAPH_FIELDS_MAP,
  updateGraph
} from "../listGraphs/graphListHelpers";
import { useGraphsModeContext } from "../listGraphs/GraphsModeContext";
import { ClusterFields } from "./ClusterFields";
import { SmartGraphCreateValues } from "./CreateGraph.types";
import { EdgeDefinitionsField } from "./EdgeDefinitionsField";
import { GraphModalFooter } from "./GraphModalFooter";
import { GraphWarnings } from "./GraphWarnings";

const smartGraphFieldsMap = {
  name: GENERAL_GRAPH_FIELDS_MAP.name,
  isDisjoint: SMART_GRAPH_FIELDS_MAP.isDisjoint,
  smartGraphAttribute: SMART_GRAPH_FIELDS_MAP.smartGraphAttribute,
  orphanCollections: GENERAL_GRAPH_FIELDS_MAP.orphanCollections
};

const INITIAL_VALUES: SmartGraphCreateValues = {
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
export const SmartGraphForm = ({ onClose }: { onClose: () => void }) => {
  const { initialGraph, mode } = useGraphsModeContext();
  const isEditMode = mode === "edit";
  const handleSubmit = async (values: SmartGraphCreateValues) => {
    const sanitizedValues = {
      name: values.name,
      edgeDefinitions: values.edgeDefinitions,
      orphanCollections: values.orphanCollections,
      options: {
        isDisjoint: values.isDisjoint,
        isSmart: true,
        numberOfShards: Number(values.numberOfShards),
        replicationFactor: Number(values.replicationFactor),
        writeConcern: Number(values.writeConcern),
        satellites: values.satellites,
        smartGraphAttribute: values.smartGraphAttribute
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
                ...smartGraphFieldsMap.name,
                isDisabled: isEditMode
              }}
            />
            <ClusterFields isShardsRequired />
            {window.frontendConfig.isCluster && (
              <FormField
                field={{
                  ...CLUSTER_GRAPH_FIELDS_MAP.satellites,
                  noOptionsMessage: () =>
                    "Please enter a new and valid collection name"
                }}
              />
            )}
            <FormField
              field={{
                ...smartGraphFieldsMap.isDisjoint,
                isDisabled: isEditMode
              }}
            />
            <FormField
              field={{
                ...smartGraphFieldsMap.smartGraphAttribute,
                isDisabled: isEditMode
              }}
            />
            <EdgeDefinitionsField
              noOptionsMessage={() =>
                "Please enter a new and valid collection name"
              }
              allowExistingCollections={false}
            />
            <FormField field={smartGraphFieldsMap.orphanCollections} />
          </FieldsGrid>
          <GraphModalFooter onClose={onClose} />
        </VStack>
      </Form>
    </Formik>
  );
};
