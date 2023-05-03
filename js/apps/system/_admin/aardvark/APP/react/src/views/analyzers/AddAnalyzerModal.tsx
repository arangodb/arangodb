import { Button, Flex, Heading, Stack } from "@chakra-ui/react";
import { Form, Formik } from "formik";
import React from "react";
import * as Yup from "yup";
import {
  Modal,
  ModalBody,
  ModalFooter,
  ModalHeader
} from "../../components/modal";
import { AddAnalyzerForm } from "./AddAnalyzerForm";
import { useAnalyzersContext } from "./AnalyzersContext";
import { CopyAnalyzer } from "./CopyAnalyzer";

export const AddAnalyzerModal = ({
  isOpen,
  onClose
}: {
  isOpen: boolean;
  onClose: () => void;
}) => {
  const { initialValues } = useAnalyzersContext();
  return (
    <Modal size="6xl" isOpen={isOpen} onClose={onClose}>
      <ModalHeader fontSize="sm" fontWeight="normal">
        <Flex direction="row" alignItems="center">
          <Heading marginRight="4" size="md">
            Create Analyzer
          </Heading>
          <CopyAnalyzer />
          <Button size="sm" colorScheme="gray" marginLeft="auto">
            Show JSON Form
          </Button>
        </Flex>
      </ModalHeader>
      <Formik
        enableReinitialize
        initialValues={initialValues}
        validationSchema={Yup.object({
          name: Yup.string().required("Name is required")
        })}
        onSubmit={values => {
          console.log("submit", { values });
        }}
      >
        <Form>
          <ModalBody>
            <AddAnalyzerForm />
          </ModalBody>
          <ModalFooter>
            <Stack direction="row" spacing={4} align="center">
              <Button colorScheme="gray" onClick={onClose}>
                Close
              </Button>
              <Button colorScheme="blue" type="submit">
                Create
              </Button>
            </Stack>
          </ModalFooter>
        </Form>
      </Formik>
    </Modal>
  );
};
