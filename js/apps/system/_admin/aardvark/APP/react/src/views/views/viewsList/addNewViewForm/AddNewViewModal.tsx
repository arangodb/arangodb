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

export const AddNewViewModal = ({
  onClose,
  isOpen
}: {
  onClose: () => void;
  isOpen: boolean;
}) => {
  const { onAdd } = useAddView();
  return (
    <Modal size="6xl" onClose={onClose} isOpen={isOpen}>
      <ModalHeader>Create New View</ModalHeader>
      <Formik
        initialValues={{
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
        }}
        validationSchema={Yup.object({
          name: Yup.string().required("Name is required")
        })}
        onSubmit={(values, { setSubmitting }) => {
          const finalValues =
            values.type === "arangosearch"
              ? { ...values, indexes: undefined }
              : {
                  type: values.type,
                  name: values.name,
                  indexes: values.indexes
                };
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
