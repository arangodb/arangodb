import { Button, Heading, Stack } from "@chakra-ui/react";
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
import { notifyError, notifySuccess } from "../../../utils/notifications";
import { DatabaseDescription } from "../Database.types";
import { useFetchInitialValues } from "../useFetchInitialValues";
import { AddDatabaseForm } from "./AddDatabaseForm";

export const AddDatabaseModal = ({
  isOpen,
  onClose
}: {
  isOpen: boolean;
  onClose: () => void;
}) => {
  const { defaults: initialValues } = useFetchInitialValues();
  const handleSubmit = async ({
    name,
    users: usernames,
    isOneShard,
    isSatellite,
    replicationFactor,
    writeConcern
  }: DatabaseDescription & { users: string[] }) => {
    name = name.normalize();
    const currentDB = getCurrentDB();
    try {
      const users = usernames.map(username => ({ username }));
      await currentDB.createDatabase(
        name,
        window.frontendConfig.isCluster
          ? {
              users,
              replicationFactor: isSatellite ? "satellite" : replicationFactor,
              sharding: isOneShard ? "single" : "",
              writeConcern: isSatellite ? undefined : writeConcern
            }
          : { users }
      );
      notifySuccess(`The database: ${name} was successfully created`);
      mutate("/databases");
      onClose();
    } catch (error: any) {
      const errorMessage = error?.response?.parsedBody?.errorMessage;
      if (errorMessage) {
        notifyError(`Database creation error: ${errorMessage}`);
      }
    }
  };
  if (!initialValues) return null;
  return (
    <Modal size="6xl" isOpen={isOpen} onClose={onClose}>
      <Formik
        initialValues={initialValues}
        validationSchema={Yup.object({
          name: Yup.string()
            .required("Name is required")
            .max(64, "Database name max length is 64 bytes.")
        })}
        onSubmit={handleSubmit}
      >
        {({ isSubmitting }) => (
          <AddDatabaseModalInner
            onClose={onClose}
            isSubmitting={isSubmitting}
          />
        )}
      </Formik>
    </Modal>
  );
};

const AddDatabaseModalInner = ({
  onClose,
  isSubmitting
}: {
  onClose: () => void;
  isSubmitting: boolean;
}) => {
  return (
    <Form>
      <ModalHeader>
        <Heading size="md">Create Database</Heading>
      </ModalHeader>

      <ModalBody>
        <AddDatabaseForm />
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
