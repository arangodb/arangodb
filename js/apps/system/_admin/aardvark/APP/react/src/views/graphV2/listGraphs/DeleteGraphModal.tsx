import { Modal, ModalBody, ModalHeader } from "../../../components/modal";
import { GraphInfo } from "arangojs/graph";
import React from "react";
import { GraphsModeProvider, useGraphsModeContext } from "../GraphsModeContext";
import {
  Button,
  Flex,
  Heading,
  ModalFooter,
  Stack,
  VStack
} from "@chakra-ui/react";
import { FormField } from "../../../components/form/FormField";
import { Form, Formik } from "formik";
import { FieldsGrid } from "../FieldsGrid";
import { getCurrentDB } from "../../../utils/arangoClient";

export const DeleteGraphModal = ({
  graph,
  isOpen,
  onClose
}: {
  graph?: GraphInfo;
  isOpen: boolean;
  onClose: () => void;
}) => {
    console.log("graph in DeleteGraphMOdal: ", graph);
  const { initialGraph } = useGraphsModeContext();

  const INITIAL_VALUES: any = {
    name: ""
  };

  const dropCollectionsField = {
    name: "dropCollections",
    type: "boolean",
    label: "Also drop collections"
  };

  const handleSubmit = async (values: any) => {
    console.log("values: ", values);
    console.log("values.name: ", values.name);

    const currentDB = getCurrentDB();
    const graph = currentDB.graph(values.name);
    console.log("graph in DeleteModalGraph: ", graph);
    /*
      const info = await graph.create(values.edgeDefinitions, {
        orphanCollections: values.orphanCollections
      });
      */
    /*
    try {
      const info = await graph.drop();
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
    */
  };

  return (
    <Formik
      initialValues={initialGraph || INITIAL_VALUES}
      onSubmit={handleSubmit}
    >
      <GraphsModeProvider mode="edit" initialGraph={graph}>
        <Modal size="6xl" isOpen={isOpen} onClose={onClose}>
          <ModalHeader fontSize="sm" fontWeight="normal">
            <Flex direction="row" alignItems="center">
              <Heading marginRight="4" size="md">
                Really delete graph "{graph?.name}"
              </Heading>
            </Flex>
          </ModalHeader>
          <ModalBody>
            <Form>
              <VStack spacing={4} align="stretch">
                <FieldsGrid maxWidth="full">
                  <FormField
                    field={{
                      ...dropCollectionsField
                    }}
                  />
                </FieldsGrid>
              </VStack>
            </Form>
          </ModalBody>
          <ModalFooter>
            <Stack direction="row" spacing={4} align="center">
              <Button onClick={onClose} colorScheme="gray">
                Cancel
              </Button>
              <Button
                colorScheme="red"
                type="submit"
                //isLoading={isSubmitting}
              >
                Delete
              </Button>
            </Stack>
          </ModalFooter>
        </Modal>
      </GraphsModeProvider>
    </Formik>
  );
};
