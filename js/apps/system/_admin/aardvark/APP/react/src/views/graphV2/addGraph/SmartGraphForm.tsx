import { Button, Stack, VStack } from "@chakra-ui/react";
import { Form, Formik } from "formik";
import React from "react";
import * as Yup from "yup";
import { FormField } from "../../../components/form/FormField";
import { ModalFooter } from "../../../components/modal";
import { getCurrentDB } from "../../../utils/arangoClient";
import { FieldsGrid } from "../FieldsGrid";
import { SmartGraphCreateValues } from "./CreateGraph.types";
import { EdgeDefinitionsField } from "./EdgeDefinitionsField";

const smartGraphFieldsMap = {
  name: {
    name: "name",
    type: "string",
    label: "Name",
    tooltip:
      "String value. The name to identify the graph. Has to be unique and must follow the Document Keys naming conventions.",
    isRequired: true
  },
  numberOfShards: {
    name: "numberOfShards",
    type: "string",
    label: "Shards",
    tooltip:
      "Numeric value. Must be at least 1. Number of shards the graph is using.",
    isRequired: true
  },
  replicationFactor: {
    name: "replicationFactor",
    type: "string",
    label: "Replication factor",
    tooltip:
      "Numeric value. Must be at least 1. Total number of copies of the data in the cluster.If not given, the system default for new collections will be used.",
    isRequired: false
  },
  minReplicationFactor: {
    name: "minReplicationFactor",
    type: "string",
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
    type: "string",
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
    noOptionsMessage: "Please enter a new and valid collection name"
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
  const handleSubmit = async (values: SmartGraphCreateValues) => {
    const currentDB = getCurrentDB();
    const graph = currentDB.graph(values.name);
    try {
      const options = {
        orphanCollections: values.orphanCollections,
        numberOfShards: Number(values.numberOfShards),
        replicationFactor: Number(values.replicationFactor),
        minReplicationFactor: Number(values.minReplicationFactor),
        isDisjoint: values.isDisjoint,
        isSmart: true,
        smartGraphAttribute: values.smartGraphAttribute
      };
      const info = await graph.create(values.edgeDefinitions, options);
      window.arangoHelper.arangoNotification(
        "Graph",
        `Successfully created the graph ${values.name}`
      );
      onClose();
      return info;
    } catch (e: any) {
      const errorMessage = e.response.body.errorMessage;
      window.arangoHelper.arangoError("Could not add graph", errorMessage);
    }
  };
  return (
    <Formik
      initialValues={INITIAL_VALUES}
      validationSchema={Yup.object({
        name: Yup.string().required("Name is required")
      })}
      onSubmit={handleSubmit}
    >
      {({ isSubmitting }) => (
        <Form>
          <VStack spacing={4} align="stretch">
            <FieldsGrid maxWidth="full">
              <FormField field={smartGraphFieldsMap.name} />
              <FormField field={smartGraphFieldsMap.numberOfShards} />
              <FormField field={smartGraphFieldsMap.replicationFactor} />
              <FormField field={smartGraphFieldsMap.minReplicationFactor} />
              <FormField field={smartGraphFieldsMap.isDisjoint} />
              <FormField field={smartGraphFieldsMap.smartGraphAttribute} />
              <EdgeDefinitionsField
                noOptionsMessage="Please enter a new and valid collection name"
                allowExistingCollections={false}
              />
              <FormField field={smartGraphFieldsMap.orphanCollections} />
            </FieldsGrid>
            <ModalFooter>
              <Stack direction="row" spacing={4} align="center">
                <Button onClick={onClose} colorScheme="gray">
                  Cancel
                </Button>
                <Button
                  colorScheme="blue"
                  type="submit"
                  isLoading={isSubmitting}
                >
                  Create
                </Button>
              </Stack>
            </ModalFooter>
          </VStack>
        </Form>
      )}
    </Formik>
  );
};
