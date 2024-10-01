import { AddIcon } from "@chakra-ui/icons";
import { Box, Button, Heading, Stack, useDisclosure } from "@chakra-ui/react";
import React from "react";
import { HashRouter, Route, Switch } from "react-router-dom";
import { ChakraCustomProvider } from "../../theme/ChakraCustomProvider";
import { useDisableNavBar } from "../../utils/useDisableNavBar";
import { useGlobalStyleReset } from "../../utils/useGlobalStyleReset";
import { AddUserModal } from "./addUser/AddUserModal";
import { UsersTable } from "./listUsers/UsersTable";
import { UsersProvider } from "./UsersContext";

export const UserManagementViewWrap = () => {
  useDisableNavBar();
  useGlobalStyleReset();
  return (
    <Box width="100%">
      <ChakraCustomProvider overrideNonReact>
        <HashRouter basename="/" hashType={"noslash"}>
          <Switch>
            <Route path="/users">
              <UserManagementView />
            </Route>
          </Switch>
        </HashRouter>
      </ChakraCustomProvider>
    </Box>
  );
};

const UserManagementView = () => {
  const {
    isOpen: isAddUserModalOpen,
    onOpen: onAddUserModalOpen,
    onClose: onAddUserModalClose
  } = useDisclosure();
  return (
    <Box padding="4" width="100%">
      <UserManagementHeader onAddUserModalOpen={onAddUserModalOpen} />
      <AddUserModal isOpen={isAddUserModalOpen} onClose={onAddUserModalClose} />
      <UsersProvider>
        <UsersTable />
      </UsersProvider>
    </Box>
  );
};

const UserManagementHeader = ({
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
        colorScheme="green"
        onClick={onAddUserModalOpen}
      >
        Add user
      </Button>
    </Stack>
  );
};
