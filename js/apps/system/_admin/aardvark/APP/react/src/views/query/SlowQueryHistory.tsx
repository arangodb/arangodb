import {
  Button,
  ModalFooter,
  ModalHeader,
  Table,
  TableCaption,
  TableContainer,
  Tbody,
  Td,
  Th,
  Thead,
  Tr
} from "@chakra-ui/react";
import { QueryInfo } from "arangojs/database";
import React from "react";
import moment from "../../../../frontend/js/lib/moment.min";
import { Modal } from "../../components/modal";
import { getCurrentDB } from "../../utils/arangoClient";
import { useFetchSlowQueries } from "./useFetchSlowQueries";

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
export const SlowQueryHistory = () => {
  const { runningQueries, refetchQueries } = useFetchSlowQueries();
  const [showDeleteModal, setShowDeleteModal] = React.useState(false);
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
          </Thead>
          <Tbody>
            {runningQueries.map(query => {
              return (
                <Tr key={query.id}>
                  {TABLE_COLUMNS.map(column => {
                    return (
                      <Td whiteSpace="normal" key={column.accessor}>
                        {column.accessorFn
                          ? column.accessorFn(query)
                          : query[column.accessor as keyof QueryInfo]}
                      </Td>
                    );
                  })}
                </Tr>
              );
            })}
            {runningQueries.length === 0 && (
              <Tr>
                <Td colSpan={TABLE_COLUMNS.length}>No slow queries.</Td>
              </Tr>
            )}
          </Tbody>
          <TableCaption marginBottom="2" textAlign="right">
            <Button
              size="sm"
              onClick={() => {
                setShowDeleteModal(true);
              }}
              colorScheme="red"
            >
              Delete History
            </Button>
          </TableCaption>
        </Table>
      </TableContainer>
      <DeleteSlowQueryHistoryModal
        isOpen={showDeleteModal}
        onClose={() => {
          setShowDeleteModal(false);
        }}
        refetchQueries={refetchQueries}
      />
    </>
  );
};

const DeleteSlowQueryHistoryModal = ({
  isOpen,
  onClose,
  refetchQueries
}: {
  isOpen: boolean;
  onClose: () => void;
  refetchQueries: () => Promise<void>;
}) => {
  const [isLoading, setIsLoading] = React.useState(false);
  const onDeleteQuery = async () => {
    setIsLoading(true);
    const currentDb = getCurrentDB();
    try {
      await currentDb.clearSlowQueries();
      await refetchQueries();
      onClose();
      setIsLoading(false);
      window.arangoHelper.arangoNotification("Deleted slow queries history");
    } catch (e) {
      window.arangoHelper.arangoError("Failed to delete slow queries history");
      onClose();
      setIsLoading(false);
    }
  };
  return (
    <Modal onClose={onClose} isOpen={isOpen}>
      <ModalHeader>
        Are you sure you want to delete the slow query log entries?
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
