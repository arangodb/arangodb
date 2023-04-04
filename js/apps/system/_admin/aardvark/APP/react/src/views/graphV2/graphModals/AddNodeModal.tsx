import {
  Button,
  FormControl,
  FormErrorMessage,
  FormLabel,
  HStack,
  Input,
  Select,
  Stack
} from "@chakra-ui/react";
import { DocumentMetadata } from "arangojs/documents";
import { JsonEditor } from "jsoneditor-react";
import React, { useState } from "react";
import {
  Modal,
  ModalBody,
  ModalFooter,
  ModalHeader
} from "../../../components/modal";
import { InfoTooltip } from "../../../components/tooltip/InfoTooltip";
import { getCurrentDB } from "../../../utils/arangoClient";
import { useGraph } from "../GraphContext";

type AddNodeResponse = DocumentMetadata & {
  new?: { _key: string; _id: string; _rev: string };
};

const useAddNodeAction = ({
  graphName,
  onSuccess,
  onFailure
}: {
  graphName: string;
  onSuccess: (response: AddNodeResponse) => void;
  onFailure: () => void;
}) => {
  const addNode = async ({
    data,
    collection
  }: {
    data: { [key: string]: string };
    collection: string;
  }) => {
    const db = getCurrentDB();
    const graphCollection = db.graph(graphName).vertexCollection(collection);
    try {
      const response = await graphCollection.save(data, { returnNew: true });
      window.arangoHelper.arangoNotification(
        `The node ${response.new._id} was successfully created`
      );
      onSuccess(response);
    } catch (error) {
      console.log("Error adding this node: ", error);
      window.arangoHelper.arangoError("Graph", "Could not add node.");
      onFailure();
    }
  };
  return { addNode };
};

const getHasSmartAttributeError = ({
  smartGraphAttribute,
  smartGraphAttributeValue
}: {
  smartGraphAttribute?: string;
  smartGraphAttributeValue?: string;
}) => {
  if (smartGraphAttribute && !smartGraphAttributeValue) {
    return true;
  }
};

export const AddNodeModal = () => {
  const { graphName, onClearAction, datasets, graphData, selectedAction } =
    useGraph();
  const { vertexCollections, smartGraphAttribute } = graphData?.settings || {};
  const { addNode } = useAddNodeAction({
    graphName,
    onSuccess: response => {
      if (!response.new) {
        window.arangoHelper.arangoError(
          "Graph",
          "Something went wrong while adding a node."
        );
        return;
      }
      const { _id: id, _key: label } = response.new;
      const { pointer } = selectedAction?.entity || {};
      const nodeModel = {
        id,
        label,
        shape: "dot",
        size: 20,
        x: Number(pointer?.canvas.x),
        y: Number(pointer?.canvas.y)
      };
      datasets?.nodes.add(nodeModel);
      onClearAction();
    },
    onFailure: onClearAction
  });

  const [json, setJson] = useState({});
  const [key, setKey] = useState("");
  const [hasError, setHasError] = useState(false);
  const [smartGraphAttributeValue, setSmartGraphAttributeValue] = useState("");

  const [collection, setCollection] = useState(
    vertexCollections?.[0].name || ""
  );

  return (
    <Modal isOpen onClose={onClearAction}>
      <ModalHeader>Add Node</ModalHeader>
      <ModalBody>
        <Stack spacing="4">
          <Stack spacing="2">
            <FormLabel htmlFor="_key">_key</FormLabel>
            <HStack>
              <Input
                id="_key"
                placeholder="Optional: Leave empty for autogenerated key"
                onChange={event => setKey(event.target.value)}
              />
              <InfoTooltip label="The node's unique key (optional attribute, leave empty for autogenerated key)" />
            </HStack>
          </Stack>
          {smartGraphAttribute && (
            <Stack spacing="2">
              <FormLabel htmlFor={smartGraphAttribute}>
                {smartGraphAttribute}
              </FormLabel>
              <HStack>
                <FormControl isInvalid={hasError}>
                  <Input
                    id={smartGraphAttribute}
                    placeholder="Required"
                    onChange={event =>
                      setSmartGraphAttributeValue(event.target.value)
                    }
                  />
                  <FormErrorMessage>
                    {smartGraphAttribute} is a required field
                  </FormErrorMessage>
                </FormControl>
                <InfoTooltip label="The node's smart graph attribute (required)" />
              </HStack>
            </Stack>
          )}
          <Stack spacing="2">
            <FormLabel htmlFor="vertexCollection">Vertex Collection</FormLabel>
            <HStack>
              <Select
                id="vertexCollection"
                onChange={event => setCollection(event.target.value)}
              >
                {vertexCollections?.map(vertexCollection => {
                  return (
                    <option
                      key={vertexCollection.name}
                      value={vertexCollection.name}
                    >
                      {vertexCollection.name}
                    </option>
                  );
                })}
              </Select>
              <InfoTooltip label="Please select the target collection for the new node." />
            </HStack>
          </Stack>
          <JsonEditor
            value={{}}
            onChange={value => {
              setJson(value);
            }}
            mode={"code"}
            history={true}
          />
        </Stack>
      </ModalBody>
      <ModalFooter>
        <HStack>
          <Button onClick={onClearAction}>Cancel</Button>
          <Button
            colorScheme="green"
            onClick={() => {
              if (
                getHasSmartAttributeError({
                  smartGraphAttribute,
                  smartGraphAttributeValue
                })
              ) {
                setHasError(true);
                return;
              }
              let data = json;
              if (key) {
                data = { ...json, _key: key };
              }
              if (smartGraphAttribute) {
                data = {
                  ...json,
                  [smartGraphAttribute]: smartGraphAttributeValue
                };
              }
              addNode({
                collection,
                data: data
              });
            }}
          >
            Create
          </Button>
        </HStack>
      </ModalFooter>
    </Modal>
  );
};
