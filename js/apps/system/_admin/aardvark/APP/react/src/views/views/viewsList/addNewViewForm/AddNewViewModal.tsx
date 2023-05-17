import { Button, Stack } from "@chakra-ui/react";
import { Form, Formik } from "formik";
import React from "react";
import * as Yup from "yup";
import {
  Modal,
  ModalBody,
  ModalFooter,
  ModalHeader
} from "../../../../components/modal";
import { AddNewViewForm } from "./AddNewViewForm";
import { useAddView } from "./useAddView";

const initialValues = {
  name: "",
  type: "arangosearch",
  primarySortCompression: "lz4",
  primarySort: [
    {
      field: "",
      direction: "asc"
    }
  ],
  storedValues: [
    {
      fields: [],
      compression: "lz4"
    }
  ],
  writebufferIdle: 64,
  writebufferActive: 0,
  writebufferSizeMax: 33554432,
  indexes: [
    {
      collection: "",
      index: ""
    }
  ]
};

export const AddNewViewModal = ({
  onClose,
  isOpen
}: {
  onClose: () => void;
  isOpen: boolean;
}) => {
  const getFinalValues = (values: typeof initialValues) => {
    if (values.type === "arangosearch") {
      return {
        ...values,
        name: values.name.normalize(),
        primarySort: values.primarySort.filter(item => item.field),
        indexes: undefined
      };
    }
    const finalIndexes = values.indexes
      .filter(indexItem => indexItem.collection && indexItem.index)
      .filter(Boolean);
    return {
      type: values.type,
      name: values.name.normalize(),
      indexes: finalIndexes
    };
  };
  const { onAdd } = useAddView();
  return (
    <Modal size="6xl" onClose={onClose} isOpen={isOpen}>
      <ModalHeader>Create New View</ModalHeader>
      <Formik
        initialValues={initialValues}
        validationSchema={Yup.object({
          name: Yup.string().required("Name is required"),
          writebufferIdle: Yup.number()
            .moreThan(-1)
            .required("Write Buffer Idle is required"),
          writebufferActive: Yup.number()
            .moreThan(-1)
            .required("Write Buffer Active is required"),
          writebufferSizeMax: Yup.number()
            .moreThan(-1)
            .required("Write Buffer Size Max is required")
        })}
        onSubmit={(values, { setSubmitting }) => {
          const finalValues = getFinalValues(values);
          onAdd(finalValues).then(() => {
            setSubmitting(false);
            onClose();
          });
        }}
        isInitialValid={false}
      >
        {({ isSubmitting, isValid }) => (
          <Form>
            <ModalBody>
              <AddNewViewForm />
            </ModalBody>
            <ModalFooter>
              <Stack direction="row">
                <Button onClick={onClose} colorScheme={"gray"}>
                  Close
                </Button>
                <Button
                  isLoading={isSubmitting}
                  colorScheme={"green"}
                  type="submit"
                  isDisabled={!isValid}
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
