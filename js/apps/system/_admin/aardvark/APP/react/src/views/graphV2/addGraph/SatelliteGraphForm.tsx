import { VStack } from "@chakra-ui/react";
import { Form, Formik } from "formik";
import React from "react";
import { mutate } from "swr";
import { FormField } from "../../../components/form/FormField";
import {
  createGraph,
  GENERAL_GRAPH_FIELDS_MAP,
  GRAPH_VALIDATION_SCHEMA
} from "../listGraphs/graphListHelpers";
import { useGraphsModeContext } from "../listGraphs/GraphsModeContext";
import { SatelliteGraphCreateValues } from "./CreateGraph.types";
import { EdgeDefinitionsField } from "./EdgeDefinitionsField";
import { FieldsGrid } from "./FieldsGrid";
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
    const info = await createGraph({
      values: {
        name: values.name,
        edgeDefinitions: values.edgeDefinitions,
        orphanCollections: values.orphanCollections,
        options: {
          replicationFactor: "satellite"
        }
      },
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
    </Formik>
  );
};
