import { VStack } from "@chakra-ui/react";
import { Form, Formik } from "formik";
import React from "react";
import { mutate } from "swr";
import * as Yup from "yup";
import { FormField } from "../../../components/form/FormField";
import { getCurrentDB } from "../../../utils/arangoClient";
import { FieldsGrid } from "../FieldsGrid";
import {
  CLUSTER_GRAPH_FIELDS_MAP,
  GENERAL_GRAPH_FIELDS_MAP
} from "../GraphsHelpers";
import { useGraphsModeContext } from "../GraphsModeContext";
import { EnterpriseGraphCreateValues } from "./CreateGraph.types";
import { EdgeDefinitionsField } from "./EdgeDefinitionsField";
import { GraphModalFooter } from "./GraphModalFooter";
import { GraphWarnings } from "./GraphWarnings";

const enterpriseGraphFieldsMap = {
  name: GENERAL_GRAPH_FIELDS_MAP.name,
  numberOfShards: CLUSTER_GRAPH_FIELDS_MAP.numberOfShards,
  replicationFactor: CLUSTER_GRAPH_FIELDS_MAP.replicationFactor,
  minReplicationFactor: CLUSTER_GRAPH_FIELDS_MAP.minReplicationFactor,
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
            <GraphWarnings />
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
