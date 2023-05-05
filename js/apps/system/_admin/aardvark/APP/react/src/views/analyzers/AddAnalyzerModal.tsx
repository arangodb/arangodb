import { Button, Flex, Heading, Stack } from "@chakra-ui/react";
import { AnalyzerDescription } from "arangojs/analyzer";
import { Form, Formik } from "formik";
import React from "react";
import { mutate } from "swr";
import * as Yup from "yup";
import {
  Modal,
  ModalBody,
  ModalFooter,
  ModalHeader
} from "../../components/modal";
import { getCurrentDB } from "../../utils/arangoClient";
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
  const initialFocusRef = React.useRef<HTMLInputElement>(null);
  return (
    <Modal
      initialFocusRef={initialFocusRef}
      size="6xl"
      isOpen={isOpen}
      onClose={onClose}
    >
      <Formik
        enableReinitialize
        initialValues={initialValues}
        validationSchema={Yup.object({
          name: Yup.string().required("Name is required")
        })}
        onSubmit={async (values: AnalyzerDescription) => {
          const currentDB = getCurrentDB();
          await currentDB.analyzer(values.name).create(values);
          mutate("/analyzers");
          onClose();
        }}
      >
        {({ isSubmitting }) => (
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
              {showJSONForm ? (
                <AnalyzerJSONForm />
              ) : (
                <AddAnalyzerForm initialFocusRef={initialFocusRef} />
              )}
            </ModalBody>
            <ModalFooter>
              <Stack direction="row" spacing={4} align="center">
                <Button colorScheme="gray" onClick={onClose}>
                  Close
                </Button>
                <Button
                  isLoading={isSubmitting}
                  colorScheme="blue"
                  type="submit"
                >
                  Create
                </Button>
              </Stack>
            </ModalFooter>
          </Form>
        )}
      </Formik>
    </Modal>
  );
};
