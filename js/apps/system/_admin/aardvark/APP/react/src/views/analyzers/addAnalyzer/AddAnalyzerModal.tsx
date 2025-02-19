import { Button, Flex, Heading, Stack } from "@chakra-ui/react";
import { CreateAnalyzerOptions } from "arangojs/analyzer";
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
import { notifySuccess } from "../../../utils/notifications";
import { AddAnalyzerForm } from "./AddAnalyzerForm";
import { AnalyzerJSONForm } from "./AnalyzerJSONForm";
import { CopyAnalyzerDropdown } from "./CopyAnalyzerDropdown";

const INITIAL_VALUES = {
  name: "",
  type: "identity",
  features: [],
  properties: {}
} as CreateAnalyzerOptions & { name: string };

export const AddAnalyzerModal = ({
  isOpen,
  onClose
}: {
  isOpen: boolean;
  onClose: () => void;
}) => {
  const initialFocusRef = React.useRef<HTMLInputElement>(null);
  const handleSubmit = async (
    values: CreateAnalyzerOptions & { name: string }
  ) => {
    const currentDB = getCurrentDB();
    try {
      await currentDB.analyzer(values.name).create(values);
      notifySuccess(`The analyzer: ${values.name} was successfully created`);
      mutate("/analyzers");
      onClose();
    } catch (error: any) {
      const errorMessage = error?.response?.parsedBody?.errorMessage;
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
          <Button isLoading={isSubmitting} colorScheme="green" type="submit">
            Create
          </Button>
        </Stack>
      </ModalFooter>
    </Form>
  );
};
