import { Button } from "@chakra-ui/react";
import { Form, Formik } from "formik";
import React from "react";
import * as Yup from "yup";
import {
  Modal,
  ModalBody,
  ModalFooter,
  ModalHeader
} from "../../../components/modal";
import { AddNewViewForm } from "./AddNewViewForm";

export const AddNewViewModal = ({
  onClose,
  isOpen
}: {
  onClose: () => void;
  isOpen: boolean;
}) => {
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
              direction: ""
            }
          ],
          storedValues: [
            {
              fields: [],
              compression: "lz4"
            }
          ],
          writebufferIdle: "",
          writebufferActive: "",
          writebufferSizeMax: ""
        }}
        validationSchema={Yup.object({
          name: Yup.string().required("Name is required")
        })}
        onSubmit={(values, { setSubmitting }) => {
          setTimeout(() => {
            alert(JSON.stringify(values, null, 2));
            setSubmitting(false);
          }, 400);
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
