import { ArrowBackIcon, DeleteIcon } from "@chakra-ui/icons";
import { Button, Flex, Heading, IconButton, Stack } from "@chakra-ui/react";
import React from "react";
import { useHistory, useRouteMatch } from "react-router-dom";
import { DatabaseInfo } from "./DatabaseInfo";
import { DeleteDatabaseModal } from "./DeleteDatabaseModal";
import { useFetchDatabase } from "../useFetchDatabase";
import { useNavbarHeight } from "../../useNavbarHeight";

export const SingleDatabaseView = () => {
  const { params } = useRouteMatch<{ databaseName: string }>();
  const databaseName = decodeURIComponent(params.databaseName);
  const { database: currentDatabase } = useFetchDatabase(databaseName);
  const [showDeleteModal, setShowDeleteModal] = React.useState(false);
  const history = useHistory();
  const navbarHeight = useNavbarHeight();
  if (!currentDatabase) {
    return null;
  }
  const currentDbName = window.frontendConfig.db;
  const isSystem = currentDatabase.name === "_system";
  const isSameDb = currentDatabase.name === currentDbName;
  const showDeleteButton = !isSystem && !isSameDb;

  return (
    <Stack padding="6" width="100%" height={`calc(100vh - ${navbarHeight}px)`}>
      <Flex direction="row" alignItems="center">
        <IconButton
          aria-label="Back"
          variant="ghost"
          size="sm"
          icon={<ArrowBackIcon width="24px" height="24px" />}
          onClick={() => {
            history.push("/databases");
          }}
        />
        <Heading marginLeft="2" as="h1" size="lg" isTruncated title={databaseName}>
          Database: {databaseName}
        </Heading>
        {showDeleteButton && (
          <Button
            size="xs"
            marginLeft="auto"
            flexShrink={0}
            leftIcon={<DeleteIcon />}
            colorScheme="red"
            onClick={() => {
              setShowDeleteModal(true);
            }}
          >
            Delete
          </Button>
        )}
      </Flex>
      <DatabaseInfo database={currentDatabase} />

      {showDeleteModal && (
        <DeleteDatabaseModal
          database={currentDatabase}
          onSuccess={() => {
            history.push("/databases");
          }}
          onClose={() => {
            setShowDeleteModal(false);
          }}
        />
      )}
    </Stack>
  );
};
