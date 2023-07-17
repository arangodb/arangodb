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
import { ClusterFields } from "./ClusterFields";
import { GeneralGraphCreateValues } from "./CreateGraph.types";
import { EdgeDefinitionsField } from "./EdgeDefinitionsField";
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
    const currentDB = getCurrentDB();
    const graph = currentDB.graph(values.name);
    try {
      const info = await graph.create(values.edgeDefinitions, {
        orphanCollections: values.orphanCollections
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
      )}
    </Formik>
  );
};
