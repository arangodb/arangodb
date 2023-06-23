import { Button, Flex, Heading, Stack } from "@chakra-ui/react";
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
import { AddDatabaseForm } from "./AddDatabaseForm";
import { DatabaseDescription } from "../Database.types";
import { useFetchInitialValues } from "../useFetchInitialValues";

export const AddDatabaseModal = ({
  isOpen,
  onClose
}: {
  isOpen: boolean;
  onClose: () => void;
}) => {
  const initialFocusRef = React.useRef<HTMLInputElement>(null);
  const { defaults: INITIAL_VALUES } = useFetchInitialValues();
  const handleSubmit = async ({
    name,
    users: usernames,
    isOneShard,
    isSatellite,
    replicationFactor,
    writeConcern
  }: DatabaseDescription & { users: string[] }) => {
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
              writeConcern
            }
          : { users }
      );
      window.arangoHelper.arangoNotification(
        `The database: ${name} was successfully created`
      );
      mutate("/databases");
      onClose();
    } catch (error: any) {
      const errorMessage = error?.response?.body?.errorMessage;
      if (errorMessage) {
        window.arangoHelper.arangoError("Database", errorMessage);
      }
    }
  };
  if (!INITIAL_VALUES) return null;
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
          <AddDatabaseModalInner
            initialFocusRef={initialFocusRef}
            onClose={onClose}
            isSubmitting={isSubmitting}
          />
        )}
      </Formik>
    </Modal>
  );
};

const AddDatabaseModalInner = ({
  initialFocusRef,
  onClose,
  isSubmitting
}: {
  initialFocusRef: React.RefObject<HTMLInputElement>;
  onClose: () => void;
  isSubmitting: boolean;
}) => {
  return (
    <Form>
      <ModalHeader fontSize="sm" fontWeight="normal">
        <Flex direction="row" alignItems="center">
          <Heading marginRight="4" size="md">
            Create Database
          </Heading>
        </Flex>
      </ModalHeader>

      <ModalBody>
        <AddDatabaseForm initialFocusRef={initialFocusRef} />
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
