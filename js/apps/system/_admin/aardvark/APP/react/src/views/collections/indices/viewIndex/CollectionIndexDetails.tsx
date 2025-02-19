import {
  Box,
  Button,
  Flex,
  Heading,
  ModalCloseButton,
  Stack,
  Tag
} from "@chakra-ui/react";
import React from "react";
import { useRouteMatch } from "react-router-dom";
import { ControlledJSONEditor } from "../../../../components/jsonEditor/ControlledJSONEditor";
import { Modal, ModalBody, ModalFooter } from "../../../../components/modal";
import { useCustomHashMatch } from "../../../../utils/useCustomHashMatch";
import { useCollectionIndicesContext } from "../CollectionIndicesContext";
import { TYPE_TO_LABEL_MAP } from "../CollectionIndicesHelpers";
import { DeleteIndexModal } from "../DeleteIndexModal";
import { IndexStats } from "./IndexStats";

export const CollectionIndexDetails = () => {
  const match = useRouteMatch<{
    indexId: string;
  }>("/cIndices/:collectionName/:indexId");
  const { collectionIndices, readOnly } = useCollectionIndicesContext();
  const { indexId } = match?.params || {};
  // need to use winodw.location.hash here instead of useRouteMatch because
  // useRouteMatch does not work with hash router
  const fromUrl = useCustomHashMatch("#cIndices/");
  const collectionNameFromUrl = fromUrl.split("/")[0];
  const decodedCollectionNameFromUrl = decodeURIComponent(
    collectionNameFromUrl
  );
  const [showDeleteModal, setShowDeleteModal] = React.useState(false);
  const foundCollectionIndex = collectionIndices?.find(collectionIndex => {
    return collectionIndex.id === `${decodedCollectionNameFromUrl}/${indexId}`;
  });
  if (!foundCollectionIndex) {
    return null;
  }
  const { type } = foundCollectionIndex;
  // TODO arangojs does not currently support "edge" index type
  const isSystem = type === "primary" || (type as string) === "edge";
  const showDeleteButton = !isSystem && !readOnly;
  return (
    <Modal
      isOpen
      size="6xl"
      modalContentProps={{
        marginBottom: 0
      }}
      onClose={() => {
        window.location.href = `#cIndices/${collectionNameFromUrl}`;
      }}
    >
      <Flex paddingX="6" paddingTop="4">
        <Flex gap="2" direction="column">
          <Heading size="md" marginRight="auto">
            Index: {decodedCollectionNameFromUrl}
          </Heading>
          <Box>
            <Tag>{TYPE_TO_LABEL_MAP[type]}</Tag>
          </Box>
        </Flex>
        <ModalCloseButton />
      </Flex>

      <ModalBody>
        <Stack width="100%" height="calc(100vh - 300px)">
          <IndexStats foundCollectionIndex={foundCollectionIndex} />
          <ControlledJSONEditor
            mode="code"
            value={foundCollectionIndex}
            htmlElementProps={{
              style: {
                height: "100%",
                width: "100%"
              }
            }}
          />
          {showDeleteModal && (
            <DeleteIndexModal
              foundCollectionIndex={foundCollectionIndex}
              onSuccess={() => {
                window.location.href = `#cIndices/${collectionNameFromUrl}`;
              }}
              onClose={() => {
                setShowDeleteModal(false);
              }}
            />
          )}
        </Stack>
      </ModalBody>
      <ModalFooter>
        {showDeleteButton && (
          <Button
            flexShrink={0}
            variant="outline"
            marginRight="auto"
            colorScheme="red"
            onClick={() => {
              setShowDeleteModal(true);
            }}
          >
            Delete
          </Button>
        )}
        <Button
          aria-label="Back"
          variant="solid"
          onClick={() => {
            window.location.href = `#cIndices/${collectionNameFromUrl}`;
          }}
        >
          Close
        </Button>
      </ModalFooter>
    </Modal>
  );
};
