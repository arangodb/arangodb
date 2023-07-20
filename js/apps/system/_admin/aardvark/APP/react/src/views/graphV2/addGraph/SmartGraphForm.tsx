import { VStack } from "@chakra-ui/react";
import { Form, Formik } from "formik";
import React from "react";
import { mutate } from "swr";
import { FormField } from "../../../components/form/FormField";
import {
  CLUSTER_GRAPH_FIELDS_MAP,
  createGraph,
  GENERAL_GRAPH_FIELDS_MAP,
  GRAPH_VALIDATION_SCHEMA,
  SMART_GRAPH_FIELDS_MAP
} from "../listGraphs/graphListHelpers";
import { useGraphsModeContext } from "../listGraphs/GraphsModeContext";
import { ClusterFields } from "./ClusterFields";
import { SmartGraphCreateValues } from "./CreateGraph.types";
import { EdgeDefinitionsField } from "./EdgeDefinitionsField";
import { FieldsGrid } from "./FieldsGrid";
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
  const handleSubmit = async (values: SmartGraphCreateValues) => {
    try {
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
      const info = await createGraph({
        values: sanitizedValues,
        onSuccess: () => {
          mutate("/graphs");
          onClose();
        }
      });
      return info;
    } catch (e: any) {
      const errorMessage = e.response.body.errorMessage;
      window.arangoHelper.arangoError("Could not add graph", errorMessage);
    }
  };
  const isDisabled = mode === "edit";
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
                isDisabled
              }}
            />
            <ClusterFields isShardsRequired />
            {window.frontendConfig.isCluster && (
              <FormField
                field={{
                  ...CLUSTER_GRAPH_FIELDS_MAP.satellites,
                  isDisabled: mode === "edit",
                  noOptionsMessage: () =>
                    "Please enter a new and valid collection name"
                }}
              />
            )}
            <FormField
              field={{
                ...smartGraphFieldsMap.isDisjoint,
                isDisabled
              }}
            />
            <FormField
              field={{
                ...smartGraphFieldsMap.smartGraphAttribute,
                isDisabled
              }}
            />
            <EdgeDefinitionsField
              noOptionsMessage={() =>
                "Please enter a new and valid collection name"
              }
              allowExistingCollections={false}
            />
            <FormField
              field={{
                ...smartGraphFieldsMap.orphanCollections,
                isDisabled
              }}
            />
          </FieldsGrid>
          <GraphModalFooter onClose={onClose} />
        </VStack>
      </Form>
    </Formik>
  );
};
