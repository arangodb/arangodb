import { VStack } from "@chakra-ui/react";
import { Form, Formik } from "formik";
import React from "react";
import { mutate } from "swr";
import * as Yup from "yup";
import { FormField } from "../../../components/form/FormField";
import { getCurrentDB } from "../../../utils/arangoClient";
import { FieldsGrid } from "../FieldsGrid";
import { useGraphsModeContext } from "../GraphsModeContext";
import { CollectionNameInfo } from "./CollectionNameInfo";
import { EnterpriseGraphCreateValues } from "./CreateGraph.types";
import { EdgeDefinitionsField } from "./EdgeDefinitionsField";
import { GraphModalFooter } from "./GraphModalFooter";

const enterpriseGraphFieldsMap = {
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
  const handleSubmit = async (values: EnterpriseGraphCreateValues) => {
    const currentDB = getCurrentDB();
    const graph = currentDB.graph(values.name);
    try {
      const options = {
        orphanCollections: values.orphanCollections,
        numberOfShards: Number(values.numberOfShards),
        replicationFactor: Number(values.replicationFactor),
        minReplicationFactor: Number(values.minReplicationFactor),
        isDisjoint: false,
        isSmart: true
      };
      const info = await graph.create(values.edgeDefinitions, options);
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
            <CollectionNameInfo />
            <FieldsGrid maxWidth="full">
              <FormField
                field={{
                  ...enterpriseGraphFieldsMap.name,
                  isDisabled: mode === "edit"
                }}
              />
              <FormField
                field={{
                  ...enterpriseGraphFieldsMap.numberOfShards,
                  isDisabled: mode === "edit"
                }}
              />
              <FormField
                field={{
                  ...enterpriseGraphFieldsMap.replicationFactor,
                  isDisabled: mode === "edit"
                }}
              />
              <FormField
                field={{
                  ...enterpriseGraphFieldsMap.minReplicationFactor,
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
                  ...enterpriseGraphFieldsMap.orphanCollections,
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
