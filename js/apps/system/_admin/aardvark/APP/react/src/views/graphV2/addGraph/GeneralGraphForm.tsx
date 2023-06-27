import { Button, Stack, VStack } from "@chakra-ui/react";
import { Form, Formik } from "formik";
import React from "react";
import * as Yup from "yup";
import { FormField } from "../../../components/form/FormField";
import { ModalFooter } from "../../../components/modal";
import { getCurrentDB } from "../../../utils/arangoClient";
import { FieldsGrid } from "../FieldsGrid";
import { GeneralGraphCreateValues } from "./CreateGraph.types";
import { EdgeDefinitionsField } from "./EdgeDefinitionsField";

const generalGraphFieldsMap = {
  name: {
    name: "name",
    type: "string",
    label: "Name",
    tooltip:
      "String value. The name to identify the graph. Has to be unique and must follow the Document Keys naming conventions.",
    isRequired: true
  },
  orphanCollections: {
    name: "orphanCollections",
    type: "custom",
    label: "Orphan collections",
    tooltip:
      "Collections that are part of a graph but not used in an edge definition.",
    isRequired: true
  }
};

const INITIAL_VALUES: GeneralGraphCreateValues = {
  name: "",
  edgeDefinitions: [
    {
      from: "",
      to: "",
      collection: ""
    }
  ],
  orphanCollections: []
};

export const GeneralGraphForm = ({ onClose }: { onClose: () => void }) => {
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
              <FormField field={generalGraphFieldsMap.name} />
              <EdgeDefinitionsField />
              <FormField field={generalGraphFieldsMap.orphanCollections} />
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
