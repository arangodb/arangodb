import {
  Button,
  Grid,
  GridItem,
  Stack,
  Text,
  VStack
} from "@chakra-ui/react";
import { Form, Formik } from "formik";
import React from "react";
import * as Yup from "yup";
import { ModalFooter } from "../../../components/modal";
import { getCurrentDB } from "../../../utils/arangoClient";
import { ExampleGraphCreateValues } from "./CreateGraph.types";

const exampleGraphsMap = [
  {
    label: "Knows Graph"
  },
  {
    label: "Traversal Graph"
  },
  {
    label: "k Shortest Paths Graph"
  },
  {
    label: "Mps Graph"
  },
  {
    label: "World Graph"
  },
  {
    label: "Social Graph"
  },
  {
    label: "City Graph"
  },
  {
    label: "Connected Components Graph"
  }
];

const INITIAL_VALUES: ExampleGraphCreateValues = {
  name: "",
  edgeDefinitions: [
    {
      from: [],
      to: [],
      collection: ""
    }
  ],
  orphanCollections: []
};

export const ExampleGraphForm = ({ onClose }: { onClose: () => void }) => {
  const handleSubmit = async (values: ExampleGraphCreateValues) => {
    const currentDB = getCurrentDB();
    const graph = currentDB.graph(values.name);
    try {
      const info = await graph.create(values.edgeDefinitions, {
        orphanCollections: values.orphanCollections
      });
      window.arangoHelper.arangoNotification(
        "Graph",
        `Successfully created the graph: ${values.name}`
      );
      onClose();
      return info;
    } catch (e: any) {
      const errorMessage = e.response.body.errorMessage;
      window.arangoHelper.arangoError("Could not add graph", errorMessage);
    }
  };
  return (
    <Formik
      initialValues={INITIAL_VALUES}
      validationSchema={Yup.object({
        name: Yup.string().required("Name is required")
      })}
      onSubmit={handleSubmit}
    >
      {({ isSubmitting }) => (
        <Form>
          <VStack spacing={4} align="stretch">
            <Grid templateColumns="repeat(5, 1fr)" gap={4}>
              {exampleGraphsMap.map(exampleGraphField => {
                return (
                  <>
                    <GridItem h="10" bg="tomato">
                      <Text>{exampleGraphField.label}</Text>
                    </GridItem>
                    <GridItem bg="papayawhip">
                      <Button
                        colorScheme="blue"
                        type="submit"
                        isLoading={isSubmitting}
                      >
                        Create
                      </Button>
                    </GridItem>
                  </>
                );
              })}
            </Grid>
            <ModalFooter>
              <Stack direction="row" spacing={4} align="center">
                <Button onClick={onClose} colorScheme="gray">
                  Cancel
                </Button>
                <Button
                  colorScheme="blue"
                  type="submit"
                  isLoading={isSubmitting}
                >
                  Create
                </Button>
              </Stack>
            </ModalFooter>
          </VStack>
        </Form>
      )}
    </Formik>
  );
};
