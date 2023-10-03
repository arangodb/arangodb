import { ArrowBackIcon, DeleteIcon } from "@chakra-ui/icons";
import {
  Box,
  Button,
  Flex,
  Heading,
  IconButton,
  Stack,
  Tag
} from "@chakra-ui/react";
import React from "react";
import { useRouteMatch } from "react-router-dom";
import { ControlledJSONEditor } from "../../../../components/jsonEditor/ControlledJSONEditor";
import { useCollectionIndicesContext } from "../CollectionIndicesContext";
import { TYPE_TO_LABEL_MAP } from "../CollectionIndicesHelpers";
import { DeleteIndexModal } from "../DeleteIndexModal";

export const CollectionIndexDetails = () => {
  const { params } = useRouteMatch<{
    indexId: string;
  }>();
  const { collectionIndices, readOnly } = useCollectionIndicesContext();
  const { indexId } = params;
  // need to use winodw.location.hash here instead of useRouteMatch because
  // useRouteMatch does not work with hash router
  const fromUrl = window.location.hash.split("#cIndices/")[1];
  const collectionNameFromUrl = fromUrl.split("/")[0];
  const decodedCollectionNameFromUrl = decodeURIComponent(
    collectionNameFromUrl
  );
  const [showDeleteModal, setShowDeleteModal] = React.useState(false);
  const currentIndex = collectionIndices?.find(collectionIndex => {
    return collectionIndex.id === `${decodedCollectionNameFromUrl}/${indexId}`;
  });
  if (!currentIndex) {
    return null;
  }
  const isSystem =
    currentIndex.type === "primary" || (currentIndex.type as string) === "edge";
  // TODO arangojs does not currently support "edge" index type
  const showDeleteButton = !isSystem && !readOnly;

  return (
    <Stack padding="6" width="100%" height="calc(100vh - 120px)">
      <Flex direction="row" alignItems="center">
        <IconButton
          aria-label="Back"
          variant="ghost"
          size="sm"
          icon={<ArrowBackIcon width="24px" height="24px" />}
          onClick={() => {
            window.location.href = `#cIndices/${collectionNameFromUrl}`;
          }}
        />
        <Heading marginLeft="2" as="h1" size="lg">
          Index: {decodedCollectionNameFromUrl}
        </Heading>
        {showDeleteButton && (
          <Button
            size="xs"
            flexShrink={0}
            marginLeft="auto"
            leftIcon={<DeleteIcon />}
            colorScheme="red"
            onClick={() => {
              setShowDeleteModal(true);
            }}
          >
            Delete
          </Button>
        )}
      </Flex>
      <Box>
        <Tag>{TYPE_TO_LABEL_MAP[currentIndex.type]}</Tag>
      </Box>
      <ControlledJSONEditor
        mode="code"
        value={currentIndex}
        htmlElementProps={{
          style: {
            height: "calc(100%)",
            width: "100%"
          }
        }}
      />
      {showDeleteModal && (
        <DeleteIndexModal
          index={currentIndex}
          onSuccess={() => {
            window.location.href = `#cIndices/${collectionNameFromUrl}`;
          }}
          onClose={() => {
            setShowDeleteModal(false);
          }}
        />
      )}
    </Stack>
  );
};
