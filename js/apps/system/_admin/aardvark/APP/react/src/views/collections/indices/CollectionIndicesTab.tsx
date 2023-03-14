import { AddIcon } from "@chakra-ui/icons";
import { Box, Button } from "@chakra-ui/react";
import React from "react";
import { AddIndexForm } from "./addIndex/AddIndexForm";
import { useCollectionIndicesContext } from "./CollectionIndicesContext";
import { CollectionIndicesTable } from "./CollectionIndicesTable";
import { useFetchIndices } from "./useFetchIndices";
import { useSetupNav } from "./useSetupNav";

export const CollectionIndicesTab = () => {
  const {
    collectionName,
    onOpenForm,
    onCloseForm,
    isFormOpen,
    readOnly
  } = useCollectionIndicesContext();
  useSetupNav({ collectionName });
  const { indices } = useFetchIndices({ collectionName });
  if (isFormOpen) {
    return <AddIndexForm onClose={onCloseForm} />;
  }
  return (
    <Box padding="4" height="full" width="full">
      <Box
        display="flex"
        flexDirection="column"
        backgroundColor="white"
        width="full"
        paddingBottom="2"
      >
        <CollectionIndicesTable indices={indices} />
        <Button
          marginTop="2"
          marginRight="4"
          size="sm"
          onClick={onOpenForm}
          alignSelf="flex-end"
          colorScheme="green"
          variant="ghost"
          leftIcon={<AddIcon />}
          isDisabled={readOnly}
        >
          Add Index
        </Button>
      </Box>
    </Box>
  );
};
