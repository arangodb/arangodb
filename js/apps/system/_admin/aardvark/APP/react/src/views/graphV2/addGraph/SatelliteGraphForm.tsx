import { VStack } from "@chakra-ui/react";
import { Form, Formik } from "formik";
import React from "react";
import { mutate } from "swr";
import * as Yup from "yup";
import { FormField } from "../../../components/form/FormField";
import { getCurrentDB } from "../../../utils/arangoClient";
import { FieldsGrid } from "../FieldsGrid";
import { GENERAL_GRAPH_FIELDS_MAP } from "../GraphsHelpers";
import { useGraphsModeContext } from "../GraphsModeContext";
import { SatelliteGraphCreateValues } from "./CreateGraph.types";
import { EdgeDefinitionsField } from "./EdgeDefinitionsField";
import { GraphModalFooter } from "./GraphModalFooter";
import { GraphWarnings } from "./GraphWarnings";

const satelliteGraphFieldsMap = {
  name: GENERAL_GRAPH_FIELDS_MAP.name,
  orphanCollections: GENERAL_GRAPH_FIELDS_MAP.orphanCollections
};

const INITIAL_VALUES: SatelliteGraphCreateValues = {
  name: "",
  edgeDefinitions: [
    {
      from: [],
      to: [],
      collection: ""
    }
  ],
  orphanCollections: [],
  replicationFactor: "satellite"
};

export const SatelliteGraphForm = ({ onClose }: { onClose: () => void }) => {
  const { initialGraph, mode } = useGraphsModeContext();
  const handleSubmit = async (values: SatelliteGraphCreateValues) => {
    const currentDB = getCurrentDB();
    const graph = currentDB.graph(values.name);
    try {
      const info = await graph.create(values.edgeDefinitions, {
        orphanCollections: values.orphanCollections,
        replicationFactor: "satellite"
      });
      window.arangoHelper.arangoNotification(
        "Graph",
        `Successfully created the graph: ${values.name}`
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
            <GraphWarnings showOneShardWarning={false} />
            <FieldsGrid maxWidth="full">
              <FormField
                field={{
                  ...satelliteGraphFieldsMap.name,
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
                  ...satelliteGraphFieldsMap.orphanCollections,
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
