import { Button, Heading, Stack } from "@chakra-ui/react";
import { CollectionType, CreateCollectionOptions } from "arangojs/collection";
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
import { getNormalizedByteLengthTest } from "../../../utils/yupHelper";
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
} as Partial<CreateCollectionOptions> & Record<string, any>;

const minReplicationFactor = window.frontendConfig.minReplicationFactor ?? 1;
const maxReplicationFactor = window.frontendConfig.maxReplicationFactor ?? 10;
const maxNumberOfShards = window.frontendConfig.maxNumberOfShards;

const legacyNameSchema = Yup.string()
  .matches(/^[a-zA-Z]/, "Collection name must always start with a letter.")
  .matches(/^[a-zA-Z0-9\-_]*$/, 'Only symbols, "_" and "-" are allowed.')
  .max(256, "Collection name max length is 256 bytes.");

const extendedNameSchema = Yup.string()
  .matches(
    /^(?![0-9._])/,
    "Collection name cannot start with a number, a dot (.), or an underscore (_)."
  )
  .matches(
    window.arangoValidationHelper.getControlCharactersRegex(),
    "Collection name cannot contain control characters (0-31)."
  )
  .matches(
    /^\S(.*\S)?$/,
    "Collection name cannot contain leading/trailing spaces."
  )
  .matches(/^(?!.*[/])/, "Collection name cannot contain a forward slash (/).")
  .test(
    "normalizedByteLength",
    "Collection name max length is 256 bytes.",
    getNormalizedByteLengthTest(256, "Collection name max length is 256 bytes.")
  );

const clusterFieldsSchema = {
  numberOfShards: Yup.number().when("distributeShardsLike", {
    is: (distributeShardsLike: any) => !distributeShardsLike,
    then: () =>
      Yup.number()
        .min(1, "Number of shards must be greater than or equal to 1.")
        .max(
          maxNumberOfShards,
          `Number of shards must be less than or equal to ${maxNumberOfShards}.`
        )
        .required("Number of shards is required.")
  }),
  replicationFactor: Yup.number().when(
    ["distributeShardsLike", "isSatellite"],
    {
      is: (distributeShardsLike: any, isSatellite: any) =>
        !distributeShardsLike && !isSatellite,
      then: () =>
        Yup.number()
          .min(
            minReplicationFactor,
            `Replication Factor must be greater than or equal to ${minReplicationFactor}.`
          )
          .max(
            maxReplicationFactor,
            `Replication Factor must be less than or equal to ${maxReplicationFactor}.`
          )
          .required("Replication Factor is required.")
    }
  ),
  writeConcern: Yup.number().when(["distributeShardsLike", "isSatellite"], {
    is: (distributeShardsLike: any, isSatellite: any) =>
      !distributeShardsLike && !isSatellite,
    then: () =>
      Yup.number()
        .min(
          minReplicationFactor,
          `Write Concern must be greater than or equal to ${minReplicationFactor}.`
        )
        .required("Write Concern is required.")
        .when("replicationFactor", ([replicationFactor], schema) =>
          schema.max(
            replicationFactor ??
              window.frontendConfig.maxReplicationFactor ??
              10,
            "Write Concern must be less than or equal to Replication Factor."
          )
        )
  })
};

const validationSchema = Yup.object({
  name: (window.frontendConfig.extendedNames
    ? extendedNameSchema
    : legacyNameSchema
  ).required("Collection name is required."),
  ...(window.App?.isCluster ? clusterFieldsSchema : {})
});

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
      smartJoinAttribute,
      writeConcern,
      isSatellite,
      ...extra
    } = values;
    const clusterOptions = window.App.isCluster
      ? distributeShardsLike
        ? { distributeShardsLike, ...extra }
        : {
            numberOfShards,
            replicationFactor: isSatellite ? "satellite" : replicationFactor,
            smartJoinAttribute: smartJoinAttribute || undefined,
            writeConcern,
            ...extra
          }
      : {};
    try {
      await currentDB.collection(name).create({
        type:
          type === "edge"
            ? CollectionType.EDGE_COLLECTION
            : CollectionType.DOCUMENT_COLLECTION,
        waitForSync,
        ...clusterOptions
      });
      window.arangoHelper.arangoNotification(
        `The collection: ${values.name} was successfully created`
      );
      mutate("/collections");
      onClose();
    } catch (error: any) {
      const errorMessage = error?.response?.parsedBody?.errorMessage;
      if (errorMessage) {
        window.arangoHelper.arangoError("Collection", errorMessage);
      }
    }
  };
  return (
    <Modal size="6xl" isOpen={isOpen} onClose={onClose}>
      <Formik
        initialValues={INITIAL_VALUES}
        validationSchema={validationSchema}
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
        <Heading marginRight="4" size="md">
          Create Collection
        </Heading>
      </ModalHeader>

      <ModalBody>
        <AddCollectionForm />
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
