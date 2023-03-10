import { Button } from "@chakra-ui/react";
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
      return { ...values, indexes: undefined };
    }
    const finalIndexes = values.indexes
      .filter(indexItem => indexItem.collection && indexItem.index)
      .filter(Boolean);
    return {
      type: values.type,
      name: values.name,
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
          name: Yup.string().required("Name is required")
        })}
        onSubmit={(values, { setSubmitting }) => {
          const finalValues = getFinalValues(values);
          onAdd(finalValues).then(() => {
            setSubmitting(false);
            onClose();
          });
        }}
      >
        {({ isSubmitting }) => (
          <Form>
            <ModalBody>
              <AddNewViewForm />
            </ModalBody>
            <ModalFooter>
              <Button
                isLoading={isSubmitting}
                colorScheme={"blue"}
                type="submit"
              >
                Create
              </Button>
            </ModalFooter>
          </Form>
        )}
      </Formik>
    </Modal>
  );
};
