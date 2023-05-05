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
import { AnalyzerJSONForm } from "./AnalyzerJSONForm";
import { useAnalyzersContext } from "./AnalyzersContext";
import { CopyAnalyzerDropdown } from "./CopyAnalyzerDropdown";

export const AddAnalyzerModal = ({
  isOpen,
  onClose
}: {
  isOpen: boolean;
  onClose: () => void;
}) => {
  const { initialValues } = useAnalyzersContext();
  const [showJSONForm, setShowJSONForm] = React.useState(false);
  return (
    <Modal size="6xl" isOpen={isOpen} onClose={onClose}>
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
          <ModalHeader fontSize="sm" fontWeight="normal">
            <Flex direction="row" alignItems="center">
              <Heading marginRight="4" size="md">
                Create Analyzer
              </Heading>
              <CopyAnalyzerDropdown />
              <Button
                size="xs"
                colorScheme="gray"
                marginLeft="auto"
                onClick={() => {
                  setShowJSONForm(!showJSONForm);
                }}
              >
                {showJSONForm ? "Show form" : " Show JSON"}
              </Button>
            </Flex>
          </ModalHeader>

          <ModalBody>
            {showJSONForm ? <AnalyzerJSONForm /> : <AddAnalyzerForm />}
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
