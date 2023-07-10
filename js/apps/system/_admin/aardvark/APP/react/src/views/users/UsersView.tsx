import { AddIcon } from "@chakra-ui/icons";
import { Box, Button, Heading, Stack, useDisclosure } from "@chakra-ui/react";
import React from "react";
import { UsersProvider } from "./UsersContext";
import { UsersModeProvider } from "./UsersModeContext";
import { AddUserModal } from "./addUser/AddUserModal";
import { UsersTable } from "./listUsers/UsersTable";

export const UsersView = () => {
  const { isOpen, onOpen, onClose } = useDisclosure();
  return (
    <Box padding="4" width="100%">
      <UserViewHeader onOpen={onOpen} />
      <UsersModeProvider mode="add">
        <AddUserModal isOpen={isOpen} onClose={onClose} />
      </UsersModeProvider>
      <UsersProvider>
        <UsersTable />
      </UsersProvider>
    </Box>
  );
};

const UserViewHeader = ({ onOpen }: { onOpen: () => void }) => {
  return (
    <Stack direction="row" marginBottom="4" alignItems="center">
      <Heading size="lg">Users</Heading>
      <Button
        size="sm"
        leftIcon={<AddIcon />}
        colorScheme="blue"
        onClick={() => {
          onOpen();
        }}
      >
        Add user
      </Button>
    </Stack>
  );
};
