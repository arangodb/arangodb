import { DeleteIcon } from "@chakra-ui/icons";
import {
  Button,
  IconButton,
  ModalFooter,
  ModalHeader,
  Table,
  TableContainer,
  Tbody,
  Td,
  Th,
  Thead,
  Tr
} from "@chakra-ui/react";
import { QueryInfo } from "arangojs/database";
import React from "react";
import moment from "../../../../../frontend/js/lib/moment.min";
import { Modal } from "../../../components/modal";
import { getCurrentDB } from "../../../utils/arangoClient";
import { useFetchRunningQueries } from "./useFetchRunningQueries";

const TABLE_COLUMNS = [
  {
    Header: "ID",
    accessor: "id"
  },
  {
    Header: "Query String",
    accessor: "query"
  },
  {
    Header: "Bind Parameters",
    accessor: "bindVars",
    accessorFn: (row: any) => {
      return JSON.stringify(row.bindVars);
    }
  },
  {
    Header: "User",
    accessor: "user"
  },
  {
    Header: "Peak Memory Usage",
    accessor: "peakMemoryUsage"
  },
  {
    Header: "Runtime",
    accessor: "runTime",
    accessorFn: (row: any) => {
      return row.runTime.toFixed(2) + "s";
    }
  },
  {
    Header: "Started",
    accessor: "started",
    accessorFn: (row: any) => {
      return moment(row.started).format("YYYY-MM-DD HH:mm:ss");
    }
  }
];
export const RunningQueries = () => {
  const { runningQueries, refetchQueries } = useFetchRunningQueries();
  const [showDeleteModal, setShowDeleteModal] = React.useState(false);
  const [queryToDelete, setQueryToDelete] = React.useState<string | null>(null);
  const onDeleteClick = (queryId: string) => {
    setShowDeleteModal(true);
    setQueryToDelete(queryId);
  };
  return (
    <>
      <TableContainer backgroundColor="white">
        <Table>
          <Thead>
            {TABLE_COLUMNS.map(column => {
              return (
                <Th width="100px" key={column.accessor} whiteSpace="normal">
                  {column.Header}
                </Th>
              );
            })}
            <Th width="100px" whiteSpace="normal">
              Actions
            </Th>
          </Thead>
          <Tbody>
            {runningQueries.map(query => {
              return (
                <Tr>
                  {TABLE_COLUMNS.map(column => {
                    return (
                      <Td whiteSpace="normal" key={column.accessor}>
                        {column.accessorFn
                          ? column.accessorFn(query)
                          : query[column.accessor as keyof QueryInfo]}
                      </Td>
                    );
                  })}
                  <Td>
                    <IconButton
                      aria-label="Kill Query"
                      icon={<DeleteIcon />}
                      colorScheme="red"
                      variant="ghost"
                      onClick={() => onDeleteClick(query.id)}
                    />
                  </Td>
                </Tr>
              );
            })}
            {runningQueries.length === 0 && (
              <Tr>
                <Td colSpan={TABLE_COLUMNS.length}>No running queries.</Td>
              </Tr>
            )}
          </Tbody>
        </Table>
      </TableContainer>
      <DeleteQueryModal
        isOpen={showDeleteModal}
        onClose={() => {
          setShowDeleteModal(false);
          setQueryToDelete(null);
        }}
        queryToDelete={queryToDelete}
        refetchQueries={refetchQueries}
      />
    </>
  );
};

const DeleteQueryModal = ({
  isOpen,
  onClose,
  refetchQueries,
  queryToDelete
}: {
  isOpen: boolean;
  queryToDelete: string | null;
  onClose: () => void;
  refetchQueries: () => Promise<void>;
}) => {
  const [isLoading, setIsLoading] = React.useState(false);
  const onDeleteQuery = async () => {
    if (!queryToDelete) return;
    setIsLoading(true);
    const currentDb = getCurrentDB();
    try {
      await currentDb.killQuery(queryToDelete);
      window.arangoHelper.arangoNotification(`Deleted query: ${queryToDelete}`);
    } catch (e) {
      window.arangoHelper.arangoError("Failed to kill query");
    }
    await refetchQueries();
    onClose();
    setIsLoading(false);
  };
  return (
    <Modal onClose={onClose} isOpen={isOpen && !!queryToDelete}>
      <ModalHeader>
        Are you sure you want to kill the query: {queryToDelete}?
      </ModalHeader>
      <ModalFooter>
        <Button
          isLoading={isLoading}
          colorScheme="red"
          mr={3}
          onClick={onDeleteQuery}
        >
          Delete
        </Button>
        <Button variant="ghost" onClick={onClose}>
          Cancel
        </Button>
      </ModalFooter>
    </Modal>
  );
};
