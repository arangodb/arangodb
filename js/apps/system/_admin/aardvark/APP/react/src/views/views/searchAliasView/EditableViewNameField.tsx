import { CheckIcon, EditIcon } from "@chakra-ui/icons";
import {
  IconButton,
  Input,
  Stack,
  Text,
  useDisclosure
} from "@chakra-ui/react";
import React, { useState } from "react";
import { ViewPropertiesType } from "./useFetchViewProperties";

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
      <Stack
        as="form"
        margin="0"
        direction="row"
        alignItems="center"
        onSubmit={async event => {
          event.preventDefault();
          setCurrentName(newName);
          onClose();
        }}
      >
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
        <IconButton type="submit" aria-label="Edit name" icon={<CheckIcon />} />
      </Stack>
    );
  }
  return (
    <Stack direction="row" alignItems="center">
      <Text color="gray.700" fontWeight="600" fontSize="lg">
        {view.name} {!isAdminUser ? "(read only)" : null}
      </Text>
      {!isCluster && isAdminUser ? (
        <IconButton
          aria-label="Open edit name input"
          icon={<EditIcon />}
          onClick={onOpen}
        />
      ) : null}
    </Stack>
  );
};
