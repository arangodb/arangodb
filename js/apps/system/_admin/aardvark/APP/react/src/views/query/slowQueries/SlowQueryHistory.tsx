import { Box, Button, Flex, ModalFooter, ModalHeader } from "@chakra-ui/react";
import { QueryInfo } from "arangojs/database";
import React from "react";
import moment from "../../../../../frontend/js/lib/moment.min";
import { Modal } from "../../../components/modal";
import { ReactTable } from "../../../components/table/ReactTable";
import { useSortableReactTable } from "../../../components/table/useSortableReactTable";
import { getCurrentDB } from "../../../utils/arangoClient";
import { useFetchSlowQueries } from "./useFetchSlowQueries";

const TABLE_COLUMNS = [
  {
    header: "ID",
    accessorKey: "id",
    id: "id"
  },
  {
    header: "Query String",
    accessorKey: "query",
    id: "query"
  },
  {
    header: "Bind Parameters",
    accessorKey: "bindVars",
    id: "bindVars",
    accessorFn: (row: any) => {
      return JSON.stringify(row.bindVars);
    }
  },
  {
    header: "User",
    accessorKey: "user",
    id: "user"
  },
  {
    header: "Peak Memory Usage",
    accessorKey: "peakMemoryUsage",
    id: "peakMemoryUsage"
  },
  {
    header: "Runtime",
    accessorKey: "runTime",
    id: "runTime",
    accessorFn: (row: any) => {
      return row.runTime.toFixed(2) + "s";
    }
  },
  {
    header: "Started",
    accessorKey: "started",
    id: "started",
    accessorFn: (row: any) => {
      return moment(row.started).format("YYYY-MM-DD HH:mm:ss");
    }
  }
];
export const SlowQueryHistory = () => {
  const { runningQueries, refetchQueries } = useFetchSlowQueries();
  const [showDeleteModal, setShowDeleteModal] = React.useState(false);
  const tableInstance = useSortableReactTable<QueryInfo>({
    data: runningQueries || [],
    columns: TABLE_COLUMNS
  });

  return (
    <Flex direction="column">
      <ReactTable table={tableInstance} />
      <Box alignSelf="flex-end" paddingX="6" paddingY="2" marginY="4">
        <Button
          size="sm"
          onClick={() => {
            setShowDeleteModal(true);
          }}
          colorScheme="red"
        >
          Delete History
        </Button>
      </Box>

      <DeleteSlowQueryHistoryModal
        isOpen={showDeleteModal}
        onClose={() => {
          setShowDeleteModal(false);
        }}
        refetchQueries={refetchQueries}
      />
    </Flex>
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
