import { CheckIcon, DeleteIcon } from "@chakra-ui/icons";
import { Box, Button, Text } from "@chakra-ui/react";
import React from "react";
import {
  SearchAliasProvider,
  useSearchAliasContext
} from "./SearchAliasContext";
import { SearchAliasJsonForm } from "./SearchAliasJsonForm";
import { ViewPropertiesType } from "./useFetchViewProperties";

export const SearchAliasViewSettings = ({
  view
}: {
  view: ViewPropertiesType;
}) => {
  return (
    <SearchAliasProvider initialView={view}>
      <Box
        height="calc(100vh - 60px)"
        display={"grid"}
        gridTemplateRows="80px 1fr"
        rowGap="5"
      >
        <SearchAliasHeader />
        <SearchAliasContent />
      </Box>
    </SearchAliasProvider>
  );
};

const SearchAliasHeader = () => {
  const { view } = useSearchAliasContext();
  return (
    <Box padding="5" borderBottomWidth="2px" borderColor="gray.200">
      <Box display="grid" gridTemplateRows={"1fr 1fr"}>
        <Text color="gray.700" fontWeight="600" fontSize="lg">
          {view.name}
        </Text>
        <Box display="grid" gridTemplateColumns="1fr 1fr">
          <CopyProperties />
          <ActionButtons />
        </Box>
      </Box>
    </Box>
  );
};

const ActionButtons = () => {
  const { onSave, errors, changed } = useSearchAliasContext();
  return (
    <Box display={"flex"} justifyContent="end" gap="4">
      <Button
        size="xs"
        colorScheme="blue"
        leftIcon={<CheckIcon />}
        onClick={onSave}
        isDisabled={errors.length > 0 || !changed}
      >
        Save view
      </Button>
      <Button size="xs" colorScheme="red" leftIcon={<DeleteIcon />}>
        Delete
      </Button>
    </Box>
  );
};

const CopyProperties = () => {
  return (
    <Box>
      <Text>Copy mutable properties</Text>
    </Box>
  );
};
const SearchAliasContent = () => {
  return <SearchAliasJsonForm />;
};
