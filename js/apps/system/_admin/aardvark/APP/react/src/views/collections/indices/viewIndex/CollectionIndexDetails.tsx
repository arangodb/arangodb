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
import { useHistory, useRouteMatch } from "react-router-dom";
import { ControlledJSONEditor } from "../../../../components/jsonEditor/ControlledJSONEditor";
import { encodeHelper } from "../../../../utils/encodeHelper";
import { useCollectionIndicesContext } from "../CollectionIndicesContext";
import { TYPE_TO_LABEL_MAP } from "../CollectionIndicesHelpers";
import { DeleteIndexModal } from "../DeleteIndexModal";

export const CollectionIndexDetails = () => {
  const { params } = useRouteMatch<{
    collectionName: string;
    indexId: string;
  }>();
  const { collectionIndices, readOnly } = useCollectionIndicesContext();
  const { collectionName, indexId } = params;
  const { encoded: encodedCollectionName } = encodeHelper(collectionName);
  const [showDeleteModal, setShowDeleteModal] = React.useState(false);
  const currentIndex = collectionIndices?.find(
    a => a.id === `${collectionName}/${indexId}`
  );
  const history = useHistory();
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
            history.push(`/cIndices/${encodedCollectionName}`);
          }}
        />
        <Heading marginLeft="2" as="h1" size="lg">
          Index: {collectionName}
        </Heading>
        {showDeleteButton && (
          <Button
            size="xs"
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
            history.push(`/cIndices/${encodedCollectionName}`);
          }}
          onClose={() => {
            setShowDeleteModal(false);
          }}
        />
      )}
    </Stack>
  );
};
