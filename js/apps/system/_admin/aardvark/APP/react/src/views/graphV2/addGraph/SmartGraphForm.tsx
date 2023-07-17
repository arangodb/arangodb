import { VStack } from "@chakra-ui/react";
import { Form, Formik } from "formik";
import React from "react";
import { mutate } from "swr";
import * as Yup from "yup";
import { FormField } from "../../../components/form/FormField";
import { FieldsGrid } from "../FieldsGrid";
import { createGraph } from "../GraphsHelpers";
import { useGraphsModeContext } from "../GraphsModeContext";
import { GraphWarnings } from "./GraphWarnings";
import { SmartGraphCreateValues } from "./CreateGraph.types";
import { EdgeDefinitionsField } from "./EdgeDefinitionsField";
import { GraphModalFooter } from "./GraphModalFooter";

const smartGraphFieldsMap = {
  name: {
    name: "name",
    type: "text",
    label: "Name",
    tooltip:
      "String value. The name to identify the graph. Has to be unique and must follow the Document Keys naming conventions.",
    isRequired: true
  },
  numberOfShards: {
    name: "numberOfShards",
    type: "number",
    label: "Shards",
    tooltip:
      "Numeric value. Must be at least 1. Number of shards the graph is using.",
    isRequired: true
  },
  replicationFactor: {
    name: "replicationFactor",
    type: "number",
    label: "Replication factor",
    tooltip:
      "Numeric value. Must be at least 1. Total number of copies of the data in the cluster.If not given, the system default for new collections will be used.",
    isRequired: false
  },
  minReplicationFactor: {
    name: "minReplicationFactor",
    type: "number",
    label: "Write concern",
    tooltip:
      "Numeric value. Must be at least 1. Must be smaller or equal compared to the replication factor. Total number of copies of the data in the cluster that are required for each write operation. If we get below this value, the collection will be read-only until enough copies are created. If not given, the system default for new collections will be used.",
    isRequired: false
  },
  isDisjoint: {
    name: "isDisjoint",
    type: "boolean",
    label: "Create disjoint graph",
    tooltip:
      "Creates a Disjoint SmartGraph. Creating edges between different SmartGraph components is not allowed.",
    isRequired: false
  },
  smartGraphAttribute: {
    name: "smartGraphAttribute",
    type: "text",
    label: "SmartGraph Attribute",
    tooltip:
      "String value. The attribute name that is used to smartly shard the vertices of a graph. Every vertex in this Graph has to have this attribute. Cannot be modified later.",
    isRequired: true
  },
  orphanCollections: {
    name: "orphanCollections",
    type: "creatableMultiSelect",
    label: "Orphan collections",
    tooltip:
      "Collections that are part of a graph but not used in an edge definition.",
    isRequired: true,
    noOptionsMessage: () => "Please enter a new and valid collection name"
  }
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
                  isDisabled: mode === "edit"
                }}
              />
              <FormField
                field={{
                  ...smartGraphFieldsMap.numberOfShards,
                  isDisabled: mode === "edit"
                }}
              />
              <FormField
                field={{
                  ...smartGraphFieldsMap.replicationFactor,
                  isDisabled: mode === "edit"
                }}
              />
              <FormField
                field={{
                  ...smartGraphFieldsMap.minReplicationFactor,
                  isDisabled: mode === "edit"
                }}
              />
              <FormField
                field={{
                  ...smartGraphFieldsMap.isDisjoint,
                  isDisabled: mode === "edit"
                }}
              />
              <FormField
                field={{
                  ...smartGraphFieldsMap.smartGraphAttribute,
                  isDisabled: mode === "edit"
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
                  isDisabled: mode === "edit"
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
