import { Button, Flex, Heading, ModalFooter, Stack } from "@chakra-ui/react";
import { GraphInfo } from "arangojs/graph";
import { Form, Formik } from "formik";
import React from "react";
import { mutate } from "swr";
import { FieldsGrid } from "../../../components/form/FieldsGrid";
import { FormField } from "../../../components/form/FormField";
import { Modal, ModalBody, ModalHeader } from "../../../components/modal";
import { getCurrentDB } from "../../../utils/arangoClient";
import { notifyError, notifySuccess } from "../../../utils/notifications";

export const DeleteGraphModal = ({
  currentGraph,
  isOpen,
  onClose,
  onSuccess
}: {
  currentGraph?: GraphInfo;
  isOpen: boolean;
  onClose: () => void;
  onSuccess: () => void;
}) => {
  const INITIAL_VALUES = {
    dropCollections: false
  };

  const handleSubmit = async (values: { dropCollections: boolean }) => {
    if (!currentGraph) return;
    const currentDB = getCurrentDB();
    const graph = currentDB.graph(currentGraph.name);
    try {
      const info = await graph.drop(values.dropCollections);
      notifySuccess(`Graph: "${currentGraph.name}" successfully deleted`);
      mutate("/graphs");
      onClose();
      onSuccess();
      return info;
    } catch (e: any) {
      const errorMessage =
        e?.response?.parsedBody?.errorMessage || "Unknown error";
      notifyError(`Could not delete graph: ${errorMessage}`);
    }
  };

  return (
    <Formik initialValues={INITIAL_VALUES} onSubmit={handleSubmit}>
      <Modal size="lg" isOpen={isOpen} onClose={onClose}>
        <ModalHeader fontSize="sm" fontWeight="normal">
          <Flex direction="row" alignItems="center">
            <Heading marginRight="4" size="md">
              Really delete graph "{currentGraph?.name}"
            </Heading>
          </Flex>
        </ModalHeader>
        <Form>
          <ModalBody>
            <FieldsGrid maxWidth="full">
              <FormField
                field={{
                  name: "dropCollections",
                  type: "boolean",
                  label: "Also drop collections"
                }}
              />
            </FieldsGrid>
          </ModalBody>
          <ModalFooter>
            <Stack direction="row" spacing={4} align="center">
              <Button onClick={onClose} colorScheme="gray">
                Cancel
              </Button>
              <Button colorScheme="red" type="submit">
                Delete
              </Button>
            </Stack>
          </ModalFooter>
        </Form>
      </Modal>
    </Formik>
  );
};
