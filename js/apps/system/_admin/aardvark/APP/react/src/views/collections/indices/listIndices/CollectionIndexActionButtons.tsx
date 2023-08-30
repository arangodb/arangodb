import { DeleteIcon, LockIcon, ViewIcon } from "@chakra-ui/icons";
import { IconButton, Stack } from "@chakra-ui/react";
import { Index } from "arangojs/indexes";
import React from "react";
import { Link as RouterLink, useHistory } from "react-router-dom";
import { encodeHelper } from "../../../../utils/encodeHelper";
import { useCollectionIndicesContext } from "../CollectionIndicesContext";
import { DeleteIndexModal } from "../DeleteIndexModal";


export function CollectionIndexActionButtons({
  index
}: {
  index: Index;
}) {
  const { readOnly } = useCollectionIndicesContext();
  const [showDeleteModal, setShowDeleteModal] = React.useState(false);
  const collectionName = index.id.slice(0, index.id.indexOf("/"));
  const { encoded: encodedCollectionName } = encodeHelper(collectionName);
  const history = useHistory();
  return (
    <Stack direction="row" align="center">
      <IconButton
        colorScheme="gray"
        variant="ghost"
        size="sm"
        aria-label="View Index"
        icon={<ViewIcon />}
        as={RouterLink}
        to={`/cIndices/${index.id}`} />
      {/* TODO arangojs does not currently support "edge" index type */}
      {index.type === "primary" || (index.type as string) === "edge" ? (
        <LockIcon aria-label="System index can't be modified" />
      ) : (
        <IconButton
          colorScheme="red"
          variant="ghost"
          size="sm"
          aria-label="Delete Index"
          icon={<DeleteIcon />}
          disabled={readOnly}
          onClick={() => setShowDeleteModal(true)} />
      )}
      {showDeleteModal && (
        <DeleteIndexModal
          index={index}
          onSuccess={() => {
            history.push(`/cIndices/${encodedCollectionName}`);
          }}
          onClose={() => {
            setShowDeleteModal(false);
          }} />
      )}
    </Stack>
  );
}
