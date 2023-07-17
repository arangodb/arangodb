import { VStack } from "@chakra-ui/react";
import { Form, Formik } from "formik";
import React from "react";
import { mutate } from "swr";
import * as Yup from "yup";
import { FormField } from "../../../components/form/FormField";
import { FieldsGrid } from "../FieldsGrid";
import {
  createGraph,
  CLUSTER_GRAPH_FIELDS_MAP,
  GENERAL_GRAPH_FIELDS_MAP,
  SMART_GRAPH_FIELDS_MAP
} from "../GraphsHelpers";
import { useGraphsModeContext } from "../GraphsModeContext";
import { SmartGraphCreateValues } from "./CreateGraph.types";
import { EdgeDefinitionsField } from "./EdgeDefinitionsField";
import { GraphModalFooter } from "./GraphModalFooter";
import { GraphWarnings } from "./GraphWarnings";

const smartGraphFieldsMap = {
  name: GENERAL_GRAPH_FIELDS_MAP.name,
  numberOfShards: CLUSTER_GRAPH_FIELDS_MAP.numberOfShards,
  replicationFactor: CLUSTER_GRAPH_FIELDS_MAP.replicationFactor,
  minReplicationFactor: CLUSTER_GRAPH_FIELDS_MAP.minReplicationFactor,
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
      const options = {
        orphanCollections: values.orphanCollections,
        numberOfShards: Number(values.numberOfShards),
        replicationFactor: Number(values.replicationFactor),
        minReplicationFactor: Number(values.minReplicationFactor),
        isDisjoint: values.isDisjoint,
        isSmart: true,
        smartGraphAttribute: values.smartGraphAttribute,
        name: values.name,
        edgeDefinitions: values.edgeDefinitions
      };
      const info = await createGraph(options);

      window.arangoHelper.arangoNotification(
        "Graph",
        `Successfully created the graph ${values.name}`
      );
      mutate("/graphs");
      onClose();
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
      validationSchema={Yup.object({
        name: Yup.string().required("Name is required")
      })}
      onSubmit={handleSubmit}
    >
      {() => (
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
              <FormField
                field={{
                  ...smartGraphFieldsMap.numberOfShards,
                  isDisabled
                }}
              />
              <FormField
                field={{
                  ...smartGraphFieldsMap.replicationFactor,
                  isDisabled
                }}
              />
              <FormField
                field={{
                  ...smartGraphFieldsMap.minReplicationFactor,
                  isDisabled
                }}
              />
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
      )}
    </Formik>
  );
};
