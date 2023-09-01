import { CheckIcon, EditIcon } from "@chakra-ui/icons";
import {
  Flex,
  Heading,
  IconButton,
  Input,
  Stack,
  Text,
  useDisclosure
} from "@chakra-ui/react";
import React, { useState } from "react";
import { ViewPropertiesType } from "../View.types";

export const EditableViewNameField = ({
  view,
  isAdminUser,
  isCluster,
  setCurrentName
}: {
  view: ViewPropertiesType;
  isAdminUser: boolean;
  isCluster: boolean;
  setCurrentName: (name: string) => void;
}) => {
  const { onOpen, onClose, isOpen } = useDisclosure();
  const [newName, setNewName] = useState(view.name);
  if (isOpen) {
    return (
      <Stack margin="0" direction="row" alignItems="center">
        <Heading as="h1" size="lg">
          View: <Text srOnly>{view.name}</Text>
        </Heading>
        <Input
          autoFocus
          value={newName}
          backgroundColor={"white"}
          onChange={e => {
            setNewName(e.target.value.normalize());
          }}
          maxWidth="300px"
          placeholder="Enter name"
        />
        <IconButton
          onClick={async event => {
            event.stopPropagation();
            event.preventDefault();
            setCurrentName(newName);
            onClose();
          }}
          aria-label="Edit name"
          icon={<CheckIcon />}
        />
      </Stack>
    );
  }
  return (
    <Flex direction="row" alignItems="center">
      <Heading as="h1" size="lg" isTruncated title={view.name}>
        View: {view.name}
      </Heading>
      {!isAdminUser ? (
        <Text flexShrink={0} color="gray.700" fontWeight="600" fontSize="lg">
          (read only)
        </Text>
      ) : null}
      {!isCluster && isAdminUser ? (
        <IconButton
          flexShrink={0}
          marginLeft="2"
          aria-label="Open edit name input"
          icon={<EditIcon />}
          onClick={onOpen}
        />
      ) : null}
    </Flex>
  );
};
