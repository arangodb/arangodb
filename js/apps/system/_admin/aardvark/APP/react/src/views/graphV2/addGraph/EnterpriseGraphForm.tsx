import { VStack } from "@chakra-ui/react";
import { Form, Formik } from "formik";
import React from "react";
import { mutate } from "swr";
import * as Yup from "yup";
import { FormField } from "../../../components/form/FormField";
import { FieldsGrid } from "../FieldsGrid";
import {
  CLUSTER_GRAPH_FIELDS_MAP,
  createGraph,
  GENERAL_GRAPH_FIELDS_MAP
} from "../GraphsHelpers";
import { useGraphsModeContext } from "../GraphsModeContext";
import { ClusterFields } from "./ClusterFields";
import { EnterpriseGraphCreateValues } from "./CreateGraph.types";
import { EdgeDefinitionsField } from "./EdgeDefinitionsField";
import { GraphModalFooter } from "./GraphModalFooter";
import { GraphWarnings } from "./GraphWarnings";

const enterpriseGraphFieldsMap = {
  name: GENERAL_GRAPH_FIELDS_MAP.name,
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
    const sanitizedValues = {
      name: values.name,
      orphanCollections: values.orphanCollections,
      edgeDefinitions: values.edgeDefinitions,
      options: {
        isSmart: true,
        numberOfShards: Number(values.numberOfShards),
        replicationFactor: Number(values.replicationFactor),
        writeConcern: Number(values.writeConcern),
        satellites: values.satellites
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
              <ClusterFields isShardsRequired />
              {(window.frontendConfig.isCluster || true) && (
                <FormField
                  field={{
                    ...CLUSTER_GRAPH_FIELDS_MAP.satellites,
                    isDisabled: mode === "edit",
                    noOptionsMessage: () =>
                      "Please enter a new and valid collection name"
                  }}
                />
              )}
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
