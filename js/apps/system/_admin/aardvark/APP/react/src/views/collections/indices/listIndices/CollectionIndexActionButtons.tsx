import { DeleteIcon, LockIcon } from "@chakra-ui/icons";
import { Box, IconButton, Tooltip } from "@chakra-ui/react";
import { Index } from "arangojs/indexes";
import React from "react";
import { useHistory } from "react-router-dom";
import { encodeHelper } from "../../../../utils/encodeHelper";
import { useCollectionIndicesContext } from "../CollectionIndicesContext";
import { DeleteIndexModal } from "../DeleteIndexModal";

export function CollectionIndexActionButtons({ index }: { index: Index }) {
  const { readOnly } = useCollectionIndicesContext();
  const [showDeleteModal, setShowDeleteModal] = React.useState(false);
  const collectionName = index.id.slice(0, index.id.indexOf("/"));
  const { encoded: encodedCollectionName } = encodeHelper(collectionName);
  const history = useHistory();

  const isSystem =
    index.type === "primary" ||
    /**
     * @todo
     * typecast required because arangojs
     * doesn't support "edge" index types yet
     */

    (index.type as string) === "edge";

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
          index={index}
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
