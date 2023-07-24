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
import { ClusterFields } from "./ClusterFields";
import { GeneralGraphCreateValues } from "./CreateGraph.types";
import { EdgeDefinitionsField } from "./EdgeDefinitionsField";
import { FieldsGrid } from "../../../components/form/FieldsGrid";
import { GraphModalFooter } from "./GraphModalFooter";
import { useCollectionOptions } from "./useEdgeCollectionOptions";

const generalGraphFieldsMap = {
  name: GENERAL_GRAPH_FIELDS_MAP.name,
  orphanCollections: GENERAL_GRAPH_FIELDS_MAP.orphanCollections
};

const INITIAL_VALUES: GeneralGraphCreateValues = {
  name: "",
  edgeDefinitions: [
    {
      from: [],
      to: [],
      collection: ""
    }
  ],
  orphanCollections: []
};

export const GeneralGraphForm = ({ onClose }: { onClose: () => void }) => {
  const { initialGraph, mode } = useGraphsModeContext();
  const { documentCollectionOptions } = useCollectionOptions();
  const handleSubmit = async (values: GeneralGraphCreateValues) => {
    const info = await createGraph({
      values,
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
          <FieldsGrid maxWidth="full">
            <FormField
              field={{
                ...generalGraphFieldsMap.name,
                isDisabled: mode === "edit"
              }}
            />
            {window.frontendConfig.isCluster && (
              <ClusterFields isShardsRequired={false} />
            )}
            <EdgeDefinitionsField
              noOptionsMessage={() => "No collections found"}
            />
            <FormField
              field={{
                ...generalGraphFieldsMap.orphanCollections,
                options: documentCollectionOptions,
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
