import { AddIcon } from "@chakra-ui/icons";
import { Box, Button, useDisclosure } from "@chakra-ui/react";
import React, { useEffect } from "react";
import { AddIndex } from "./AddIndex";
import { CollectionIndicesList } from "./CollectionIndicesList";
import { useFetchIndices } from "./useFetchIndices";

export const CollectionIndicesTab = ({
  collectionName
}: {
  collectionName: string;
  collection: any;
}) => {
  useSteupNav({ collectionName });
  const { indices } = useFetchIndices({ collectionName });
  const { onOpen, onClose, isOpen } = useDisclosure();
  if (isOpen) {
    return <AddIndex onClose={onClose} />;
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
          onClick={onOpen}
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

const useSteupNav = ({ collectionName }: { collectionName: string }) => {
  useEffect(() =>
    window.arangoHelper.buildCollectionSubNav(collectionName, "Indexes")
  );
};
