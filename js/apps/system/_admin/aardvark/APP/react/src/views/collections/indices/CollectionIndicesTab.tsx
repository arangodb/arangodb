import { AddIcon } from "@chakra-ui/icons";
import { Button, Stack } from "@chakra-ui/react";
import React, { useEffect } from "react";
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
  return (
    <Stack backgroundColor="white" height="full">
      <CollectionIndicesList indices={indices} />
      <Button colorScheme="blue" variant="ghost" leftIcon={<AddIcon />}>
        Add Index
      </Button>
    </Stack>
  );
};

const useSteupNav = ({ collectionName }: { collectionName: string }) => {
  useEffect(() =>
    window.arangoHelper.buildCollectionSubNav(collectionName, "Indexes")
  );
};
