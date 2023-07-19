import { AddIcon } from "@chakra-ui/icons";
import { Box, Button, Heading, Stack, useDisclosure } from "@chakra-ui/react";
import React from "react";
import { AddUserModal } from "./addUser/AddUserModal";
import { UsersTable } from "./listUsers/UsersTable";
import { UsersProvider } from "./UsersContext";

export const UsersView = () => {
  const {
    isOpen: isAddUserModalOpen,
    onOpen: onAddUserModalOpen,
    onClose: onAddUserModalClose
  } = useDisclosure();
  return (
    <Box padding="4" width="100%">
      <UserViewHeader onAddUserModalOpen={onAddUserModalOpen} />
      <AddUserModal isOpen={isAddUserModalOpen} onClose={onAddUserModalClose} />
      <UsersProvider>
        <UsersTable />
      </UsersProvider>
    </Box>
  );
};

const UserViewHeader = ({
  onAddUserModalOpen
}: {
  onAddUserModalOpen: () => void;
}) => {
  return (
    <Stack direction="row" marginBottom="4" alignItems="center">
      <Heading size="lg">Users</Heading>
      <Button
        size="sm"
        leftIcon={<AddIcon />}
        colorScheme="blue"
        onClick={onAddUserModalOpen}
      >
        Add user
      </Button>
    </Stack>
  );
};
