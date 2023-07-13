import { Button, Flex, Heading, Stack } from "@chakra-ui/react";
import { CreateCollectionOptions } from "arangojs/collection";
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
import { AddCollectionForm } from "./AddCollectionForm";

const INITIAL_VALUES = {
  name: "",
  type: "document" as "document" | "edge",
  waitForSync: false,
  numberOfShards: 1,
  shardKeys: ["_key"],
  replicationFactor: window.frontendConfig.defaultReplicationFactor,
  writeConcern: 1,
  distributeShardsLike: "",
  isSatellite: false,
  smartJoinAttribute: ""
} satisfies Partial<CreateCollectionOptions> & Record<string, any>;

export const AddCollectionModal = ({
  isOpen,
  onClose
}: {
  isOpen: boolean;
  onClose: () => void;
}) => {
  const handleSubmit = async (values: typeof INITIAL_VALUES) => {
    const currentDB = getCurrentDB();
    const {
      name,
      type,
      waitForSync,
      distributeShardsLike,
      numberOfShards,
      replicationFactor,
      writeConcern,
      isSatellite,
      ...extra
    } = values;
    const clusterOptions: Partial<CreateCollectionOptions> = window.App
      .isCluster
      ? distributeShardsLike
        ? { distributeShardsLike, ...extra }
        : {
            numberOfShards,
            replicationFactor: (isSatellite
              ? "satellite"
              : replicationFactor) as any, // TODO workaround for arangojs 7
            writeConcern,
            ...extra
          }
      : {};
    try {
      await currentDB.collection(name).create({
        type: type === "edge" ? 3 : 2,
        waitForSync,
        ...clusterOptions
      });
      window.arangoHelper.arangoNotification(
        `The collection: ${values.name} was successfully created`
      );
      mutate("/collections");
      onClose();
    } catch (error: any) {
      const errorMessage = error?.response?.body?.errorMessage;
      if (errorMessage) {
        window.arangoHelper.arangoError("Collection", errorMessage);
      }
    }
  };
  return (
    <Modal size="6xl" isOpen={isOpen} onClose={onClose}>
      <Formik
        initialValues={INITIAL_VALUES}
        validationSchema={Yup.object({
          name: Yup.string().required("Name is required")
        })}
        validate={values => {
          const errors = {} as Record<keyof typeof values, string>;
          if (window.App.isCluster) {
            if (!values.distributeShardsLike) {
              if ((values.numberOfShards as any) === "") {
                errors.numberOfShards = "Value must be a number";
              }
              if (!values.isSatellite) {
                if (values.writeConcern > values.replicationFactor) {
                  errors.writeConcern =
                    "Write concern cannot be greater than replication factor";
                }
                if (typeof values.writeConcern !== "number") {
                  errors.writeConcern = "Value must be a number";
                }
                if (typeof values.replicationFactor !== "number") {
                  errors.replicationFactor = "Value must be a number";
                }
              }
            }
          }
          return errors;
        }}
        onSubmit={handleSubmit}
      >
        {({ isSubmitting }) => (
          <AddCollectionModalInner
            onClose={onClose}
            isSubmitting={isSubmitting}
          />
        )}
      </Formik>
    </Modal>
  );
};

const AddCollectionModalInner = ({
  onClose,
  isSubmitting
}: {
  onClose: () => void;
  isSubmitting: boolean;
}) => {
  return (
    <Form>
      <ModalHeader fontSize="sm" fontWeight="normal">
        <Flex direction="row" alignItems="center">
          <Heading marginRight="4" size="md">
            Create Collection
          </Heading>
        </Flex>
      </ModalHeader>

      <ModalBody>
        <AddCollectionForm />
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
