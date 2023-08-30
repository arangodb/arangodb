import { AddIcon } from "@chakra-ui/icons";
import {
  Box,
  Button,
  Flex,
  Heading,
  Spinner,
  Stack,
  Text
} from "@chakra-ui/react";
import React from "react";
import { AddIndexForm } from "./addIndex/AddIndexForm";
import { useCollectionIndicesContext } from "./CollectionIndicesContext";
import { CollectionIndicesTable } from "./listIndices/CollectionIndicesTable";

export const CollectionIndicesView = () => {
  const { isFormOpen, onOpenForm, onCloseForm } = useCollectionIndicesContext();
  return isFormOpen ? (
    <AddIndexForm onClose={onCloseForm} />
  ) : (
    <Box padding="4" width="100%">
      <IndexViewHeader onOpen={onOpenForm} />
      <CollectionIndicesTable />
    </Box>
  );
};

const IndexViewHeader = ({ onOpen }: { onOpen: () => void }) => {
  const { stats } = useCollectionIndicesContext();
  return (
    <Stack direction="row" marginBottom="4" alignItems="center" spacing="6">
      <Heading size="lg">Indexes</Heading>
      <Flex direction="column">
        <Text fontSize="2xl" fontWeight="bold">
          {stats ? stats.count : <Spinner size="sm" />}
        </Text>
        <Text fontSize="sm" marginTop="-1">
          Index count
        </Text>
      </Flex>
      <Flex direction="column">
        <Text fontSize="2xl" fontWeight="bold">
          {stats ? window.prettyBytes(stats.size) : <Spinner size="sm" />}
        </Text>
        <Text fontSize="sm" marginTop="-1">
          Estimated memory used
        </Text>
      </Flex>
      <Button
        size="sm"
        leftIcon={<AddIcon />}
        colorScheme="blue"
        onClick={() => {
          onOpen();
        }}
      >
        Add index
      </Button>
    </Stack>
  );
};
