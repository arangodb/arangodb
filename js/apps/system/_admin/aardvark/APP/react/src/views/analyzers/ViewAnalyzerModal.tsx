import { Button, Flex, Text } from "@chakra-ui/react";
import { Formik, Form } from "formik";
import React from "react";
import {
  Modal,
  ModalBody,
  ModalFooter,
  ModalHeader
} from "../../components/modal";
import { AddAnalyzerForm } from "./AddAnalyzerForm";
import { AnalyzerJSONForm } from "./AnalyzerJSONForm";
import { useAnalyzersContext } from "./AnalyzersContext";

export const ViewAnalyzerModal = () => {
  const { viewAnalyzerName, setViewAnalyzerName, analyzers } =
    useAnalyzersContext();
  const [showJSONForm, setShowJSONForm] = React.useState(false);
  const currentAnalyzer = analyzers?.find(a => a.name === viewAnalyzerName);
  if (!viewAnalyzerName || !currentAnalyzer) {
    return null;
  }
  return (
    <Modal
      size="6xl"
      isOpen
      onClose={() => {
        setViewAnalyzerName("");
      }}
    >
      <ModalHeader>
        <Flex>
          <Text>{viewAnalyzerName}</Text>
          <Button
            size="sm"
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
      <Formik
        initialValues={currentAnalyzer}
        onSubmit={() => {
          // ignore
        }}
      >
        <Form>
          <ModalBody>
            {showJSONForm ? <AnalyzerJSONForm /> : <AddAnalyzerForm />}
          </ModalBody>
        </Form>
      </Formik>
      <ModalFooter>
        <Button
          colorScheme={"gray"}
          onClick={() => {
            setViewAnalyzerName("");
          }}
        >
          Close
        </Button>
      </ModalFooter>
    </Modal>
  );
};
