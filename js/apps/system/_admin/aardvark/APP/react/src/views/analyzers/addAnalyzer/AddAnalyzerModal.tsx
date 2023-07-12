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
} from "../../../components/modal";
import { getCurrentDB } from "../../../utils/arangoClient";
import { useReinitializeForm } from "../useReinitializeForm";
import { AddAnalyzerForm } from "./AddAnalyzerForm";
import { AnalyzerJSONForm } from "./AnalyzerJSONForm";
import { CopyAnalyzerDropdown } from "./CopyAnalyzerDropdown";

const INITIAL_VALUES = {
  name: "",
  type: "identity",
  features: []
} as AnalyzerDescription;

export const AddAnalyzerModal = ({
  isOpen,
  onClose
}: {
  isOpen: boolean;
  onClose: () => void;
}) => {
  const initialFocusRef = React.useRef<HTMLInputElement>(null);
  const handleSubmit = async (values: AnalyzerDescription) => {
    const currentDB = getCurrentDB();
    try {
      await currentDB.analyzer(values.name).create(values);
      window.arangoHelper.arangoNotification(
        `The analyzer: ${values.name} was successfully created`
      );
      mutate("/analyzers");
      onClose();
    } catch (error: any) {
      const errorMessage = error?.response?.body?.errorMessage;
      if (errorMessage) {
        window.arangoHelper.arangoError("Analyzer", errorMessage);
      }
    }
  };
  return (
    <Modal
      initialFocusRef={initialFocusRef}
      size="6xl"
      isOpen={isOpen}
      onClose={onClose}
    >
      <Formik
        initialValues={INITIAL_VALUES}
        validationSchema={Yup.object({
          name: Yup.string().required("Name is required")
        })}
        onSubmit={handleSubmit}
      >
        {({ isSubmitting }) => (
          <AddAnalyzerModalInner
            initialFocusRef={initialFocusRef}
            onClose={onClose}
            isSubmitting={isSubmitting}
          />
        )}
      </Formik>
    </Modal>
  );
};

const AddAnalyzerModalInner = ({
  initialFocusRef,
  onClose,
  isSubmitting
}: {
  initialFocusRef: React.RefObject<HTMLInputElement>;
  onClose: () => void;
  isSubmitting: boolean;
}) => {
  const [showJSONForm, setShowJSONForm] = React.useState(false);
  useReinitializeForm();
  return (
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
          <Button isLoading={isSubmitting} colorScheme="blue" type="submit">
            Create
          </Button>
        </Stack>
      </ModalFooter>
    </Form>
  );
};
