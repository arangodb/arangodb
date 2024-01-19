import { Button, Stack } from "@chakra-ui/react";
import { Form, Formik } from "formik";
import React from "react";
import * as Yup from "yup";
import { InputControl } from "../../../components/form/InputControl";
import {
  Modal,
  ModalBody,
  ModalFooter,
  ModalHeader
} from "../../../components/modal";
import { useQueryContext } from "../QueryContextProvider";
import { QueryType } from "./useFetchUserSavedQueries";

const getQueryExists = ({
  savedQueries,
  queryName
}: {
  savedQueries: QueryType[] | undefined;
  queryName: string;
}) => {
  return !!savedQueries?.some(query => query.name === queryName);
};
export const SaveAsModal = () => {
  const {
    onSaveAs,
    onSave,
    setCurrentQueryName,
    savedQueries,
    isSaveAsModalOpen,
    onCloseSaveAsModal
  } = useQueryContext();
  const handleSave = async (newQueryName: string) => {
    const queryExists = getQueryExists({
      savedQueries,
      queryName: newQueryName
    });
    if (queryExists) {
      await onSave(newQueryName);
      setCurrentQueryName(newQueryName);
      onCloseSaveAsModal();
      return;
    }
    await onSaveAs(newQueryName);
    setCurrentQueryName(newQueryName);
    onCloseSaveAsModal();
  };
  return (
    <Modal isOpen={isSaveAsModalOpen} onClose={onCloseSaveAsModal}>
      <Formik
        onSubmit={values => handleSave(values.newQueryName)}
        initialValues={{
          newQueryName: ""
        }}
        validationSchema={Yup.object().shape({
          newQueryName: Yup.string()
            .required("Name is required")
            .matches(
              /^(\s*[a-zA-Z0-9\-._]+\s*)+$/,
              'Only characters, numbers and ".", "_", "-" symbols are allowed'
            )
        })}
      >
        {({ values: { newQueryName } }) => {
          const queryExists = getQueryExists({
            savedQueries,
            queryName: newQueryName
          });
          return (
            <Form>
              <ModalHeader>Save Query</ModalHeader>
              <ModalBody>
                <InputControl name="newQueryName" />
              </ModalBody>
              <ModalFooter>
                <Stack direction="row">
                  <Button
                    colorScheme="gray"
                    onClick={() => onCloseSaveAsModal()}
                  >
                    Cancel
                  </Button>
                  <Button
                    type="submit"
                    isDisabled={newQueryName === ""}
                    colorScheme="green"
                  >
                    {queryExists ? "Update" : "Save"}
                  </Button>
                </Stack>
              </ModalFooter>
            </Form>
          );
        }}
      </Formik>
    </Modal>
  );
};
