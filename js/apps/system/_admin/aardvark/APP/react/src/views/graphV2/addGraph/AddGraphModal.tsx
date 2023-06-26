import {
  Alert,
  AlertIcon,
  Box,
  Button,
  Flex,
  Heading,
  HStack,
  Stack,
  Tabs,
  TabList,
  Tab,
  TabPanels,
  TabPanel,
  Text,
  VStack
} from "@chakra-ui/react";
import { FieldsGrid } from "../FieldsGrid";
import { Form, Formik } from "formik";
import { FormField } from "../../../components/form/FormField";
import React from "react";
import { InfoTooltip } from "../../../components/tooltip/InfoTooltip";
import { InputControl } from "../../../components/form/InputControl";
import * as Yup from "yup";
import {
  Modal,
  ModalBody,
  ModalFooter,
  ModalHeader
} from "../../../components/modal";
import { getCurrentDB } from "../../../utils/arangoClient";

export const AddGraphModal = ({
  isOpen,
  onClose
}: {
  isOpen: boolean;
  onClose: () => void;
}) => {
  const initialFocusRef = React.useRef<HTMLInputElement>(null);
  const handleSubmit = async (values: any) => {
    const currentDB = getCurrentDB();
    const graph = currentDB.graph(values.name);
    const info = await graph.create([
      {
        collection: "edges",
        from: ["start-vertices"],
        to: ["end-vertices"]
      }
    ]);
    return info;
    /*
    const collection = "edges"
    const newCollectionObject = {
        "name": "entergraph",
        "edgeDefinitions": [
            {
                "collection": "edges",
                "from": [
                    "nodes"
                ],
                "to": [
                    "nodes"
                ]
            }
        ],
        "orphanCollections": [],
        "isSmart": true,
        "options": {
            "numberOfShards": 1,
            "replicationFactor": null,
            "minReplicationFactor": null,
            "isDisjoint": false,
            "isSmart": true
        }
    };
    */
    /*
    const currentDB = getCurrentDB();
    try {
      await currentDB.analyzer(values.name).create(values);
      window.arangoHelper.arangoNotification(
        `The analyzer: ${values.name} was successfully created`
      );
      mutate("/analyzers");
      onClose();
    } catch (error: any) {
      const errorMessage = error?.response?.body?.errorMessage;
      if (errorMessage) {
        window.arangoHelper.arangoError("Analyzer", errorMessage);
      }
    }
    */
  };
  return (
    <Modal
      initialFocusRef={initialFocusRef}
      size="6xl"
      isOpen={isOpen}
      onClose={onClose}
    >
      <Formik
        initialValues={{}}
        validationSchema={Yup.object({
          name: Yup.string().required("Name is required")
        })}
        onSubmit={handleSubmit}
      >
        {({ isSubmitting }) => (
          <AddGraphModalInner
            initialFocusRef={initialFocusRef}
            onClose={onClose}
            isSubmitting={isSubmitting}
          />
        )}
      </Formik>
    </Modal>
  );
};

export const GeneralGraphForm = () => {
  return <GeneralGraphFormInner />;
};

export const GeneralGraphFormInner = () => {
  const generalGraphFields = [
    {
      name: "name",
      type: "string",
      label: "Name",
      tooltip: "Name of the graph.",
      isRequired: true
    }
  ];
  const generalGraphRelationFields = [
    {
      name: "edgeDefinition",
      type: "string",
      label: "edgeDefinition",
      tooltip: "An edge definition defines a relation of the graph.",
      isRequired: true
    },
    {
      name: "fromCollections",
      type: "custom",
      label: "fromCollections",
      tooltip:
        "The collections that contain the start vertices of the relation.",
      isRequired: true
    },
    {
      name: "toCollections",
      type: "custom",
      label: "toCollections",
      tooltip: "The collections that contain the end vertices of the relation.",
      isRequired: true
    }
  ];
  return (
    <VStack spacing={4} align="stretch">
      <FieldsGrid>
        {generalGraphFields?.map(field => {
          return (
            <FormField
              field={{
                ...field
              }}
              key={field.name}
            />
          );
        })}
      </FieldsGrid>
      <HStack>
        <Text>Relation</Text>
        <Button colorScheme="blue" type="submit">
          Add relation
        </Button>
      </HStack>
      <FieldsGrid backgroundColor="green.100" verticalPadding="4">
        {generalGraphRelationFields.map(field => {
          return (
            <FormField
              field={{
                ...field
              }}
              key={field.name}
            />
          );
        })}
      </FieldsGrid>
    </VStack>
  );
};

const AddGraphModalInner = ({
  initialFocusRef,
  onClose,
  isSubmitting
}: {
  initialFocusRef: React.RefObject<HTMLInputElement>;
  onClose: () => void;
  isSubmitting: boolean;
}) => {
  console.log("initialFocusRef: ", initialFocusRef);
  return (
    <Form>
      <ModalHeader fontSize="sm" fontWeight="normal">
        <Flex direction="row" alignItems="center">
          <Heading marginRight="4" size="md">
            Create Graph
          </Heading>
        </Flex>
      </ModalHeader>

      <ModalBody>
        <Tabs size="md" variant="enclosed" defaultIndex={4}>
          <TabList>
            <Tab>Examples</Tab>
            <Tab>SatelliteGraph</Tab>
            <Tab>GeneralGraph</Tab>
            <Tab>SmartGraph</Tab>
            <Tab>EnterpriseGraph</Tab>
          </TabList>
          <TabPanels>
            <TabPanel>
              <p>Examples!</p>
            </TabPanel>
            <TabPanel>
              <p>SatelliteGraph!</p>
            </TabPanel>
            <TabPanel>
              <GeneralGraphForm />
            </TabPanel>
            <TabPanel>
              <p>SmartGraph!</p>
            </TabPanel>
            <TabPanel>
              <VStack spacing={4} align="stretch">
                <Alert status="info">
                  <AlertIcon />
                  Only use non-existent collection names. They are automatically
                  created during the graph setup.
                </Alert>
                <HStack>
                  <InputControl
                    ref={initialFocusRef}
                    name="name"
                    label="Name"
                  />
                  <InfoTooltip
                    label={
                      "String value. The name to identify the graph. Has to be unique and must follow the Document Keys naming conventions."
                    }
                  />
                </HStack>
                <HStack>
                  <InputControl name="shards" label="Shards" />
                  <InfoTooltip
                    label={
                      "Numeric value. Must be at least 1. Number of shards the graph is using."
                    }
                  />
                </HStack>
                <HStack>
                  <InputControl
                    name="replicationFactor"
                    label="Replication factor"
                  />
                  <InfoTooltip
                    label={
                      "Numeric value. Must be at least 1. Total number of copies of the data in the cluster.If not given, the system default for new collections will be used."
                    }
                  />
                </HStack>
                <HStack>
                  <InputControl name="writeConcern" label="Write concern" />
                  <InfoTooltip
                    label={
                      "Numeric value. Must be at least 1. Must be smaller or equal compared to the replication factor. Total number of copies of the data in the cluster that are required for each write operation. If we get below this value, the collection will be read-only until enough copies are created. If not given, the system default for new collections will be used."
                    }
                  />
                </HStack>
                <HStack>
                  <Text>Relation</Text>
                  <Button
                    isLoading={isSubmitting}
                    colorScheme="blue"
                    type="submit"
                  >
                    Add relation
                  </Button>
                </HStack>
                <Box bg="green.100" w="100%" p={4}>
                  <VStack spacing={4} align="stretch">
                    <HStack>
                      <InputControl
                        name="edgeDefinition"
                        label="Edge definition"
                      />
                      <InfoTooltip
                        label={
                          "An edge definition defines a relation of the graph."
                        }
                      />
                    </HStack>
                    <HStack>
                      <InputControl
                        name="fromCollections"
                        label="fromCollections"
                      />
                      <InfoTooltip
                        label={
                          "The collections that contain the start vertices of the relation."
                        }
                      />
                    </HStack>
                    <HStack>
                      <InputControl
                        name="toCollections"
                        label="toCollections"
                      />
                      <InfoTooltip
                        label={
                          "The collections that contain the end vertices of the relation."
                        }
                      />
                    </HStack>
                  </VStack>
                </Box>
                <HStack>
                  <InputControl
                    name="orphanCollections"
                    label="Orphan collections"
                  />
                  <InfoTooltip
                    label={
                      "Collections that are part of a graph but not used in an edge definition."
                    }
                  />
                </HStack>
              </VStack>
            </TabPanel>
          </TabPanels>
        </Tabs>
      </ModalBody>
      <ModalFooter>
        <Stack direction="row" spacing={4} align="center">
          <Button colorScheme="gray" onClick={onClose}>
            Cancel
          </Button>
          <Button isLoading={isSubmitting} colorScheme="blue" type="submit">
            Create
          </Button>
        </Stack>
      </ModalFooter>
    </Form>
  );
};
