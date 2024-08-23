import { DeleteIcon, LockIcon } from "@chakra-ui/icons";
import { Box, IconButton, Tooltip } from "@chakra-ui/react";
import { Index } from "arangojs/indexes";
import React from "react";
import { useHistory } from "react-router-dom";
import { encodeHelper } from "../../../../utils/encodeHelper";
import { useCollectionIndicesContext } from "../CollectionIndicesContext";
import { DeleteIndexModal } from "../DeleteIndexModal";

export function CollectionIndexActionButtons({
  collectionIndex
}: {
  collectionIndex: Index & { progress?: number };
}) {
  const { readOnly } = useCollectionIndicesContext();
  const [showDeleteModal, setShowDeleteModal] = React.useState(false);
  const collectionName = collectionIndex.id.slice(
    0,
    collectionIndex.id.indexOf("/")
  );
  const { encoded: encodedCollectionName } = encodeHelper(collectionName);
  const history = useHistory();

  const isSystem =
    collectionIndex.type === "primary" ||
    /**
     * @todo
     * typecast required because arangojs
     * doesn't support "edge" index types yet
     */

    (collectionIndex.type as string) === "edge";

  if (isSystem) {
    const label = "System index can't be modified";
    return (
      <Box padding="2">
        <Tooltip hasArrow label={label} placement="top">
          <LockIcon aria-label={label} />
        </Tooltip>
      </Box>
    );
  }

  if (
    typeof collectionIndex.progress === "number" &&
    collectionIndex.progress < 100
  ) {
    const label = "Index creation in progress";
    return (
      <Box padding="2">
        <Tooltip hasArrow label={label} placement="top">
          <LockIcon aria-label={label} />
        </Tooltip>
      </Box>
    );
  }

  return (
    <>
      <IconButton
        colorScheme="red"
        variant="ghost"
        size="sm"
        aria-label="Delete Index"
        icon={<DeleteIcon />}
        disabled={readOnly}
        onClick={() => setShowDeleteModal(true)}
      />
      {showDeleteModal && (
        <DeleteIndexModal
          foundCollectionIndex={collectionIndex}
          onSuccess={() => {
            history.push(`/cIndices/${encodedCollectionName}`);
          }}
          onClose={() => {
            setShowDeleteModal(false);
          }}
        />
      )}
    </>
  );
}
