import { AddIcon } from "@chakra-ui/icons";
import { Box, Button } from "@chakra-ui/react";
import React from "react";
import { AddIndex } from "./AddIndex";
import { useCollectionIndicesContext } from "./CollectionIndicesContext";
import { CollectionIndicesList } from "./CollectionIndicesList";
import { useFetchIndices } from "./useFetchIndices";
import { useSetupNav } from "./useSetupNav";

export const CollectionIndicesTab = () => {
  const {
    collectionName,
    onOpenForm,
    onCloseForm,
    isFormOpen
  } = useCollectionIndicesContext();
  useSetupNav({ collectionName });
  const { indices } = useFetchIndices({ collectionName });
  if (isFormOpen) {
    return <AddIndex onClose={onCloseForm} />;
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
        <CollectionIndicesList indices={indices} />
        <Button
          marginTop="2"
          marginRight="4"
          size="sm"
          onClick={onOpenForm}
          alignSelf="flex-end"
          colorScheme="green"
          variant="ghost"
          leftIcon={<AddIcon />}
        >
          Add Index
        </Button>
      </Box>
    </Box>
  );
};
