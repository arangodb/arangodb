import { DeleteIcon } from "@chakra-ui/icons";
import { Button, IconButton, ModalFooter, ModalHeader } from "@chakra-ui/react";
import { CellContext } from "@tanstack/react-table";
import { QueryInfo } from "arangojs/database";
import React, { useEffect } from "react";
import moment from "../../../../../frontend/js/lib/moment.min";
import { Modal } from "../../../components/modal";
import { ReactTable } from "../../../components/table/ReactTable";
import { useSortableReactTable } from "../../../components/table/useSortableReactTable";
import { getCurrentDB } from "../../../utils/arangoClient";
import { useQueryContext } from "../QueryContextProvider";

const TABLE_COLUMNS = [
  {
    header: "ID",
    id: "id",
    accessorKey: "id"
  },
  {
    header: "Query String",
    id: "query",
    accessorKey: "query"
  },
  {
    header: "Bind Parameters",
    id: "bindVars",
    accessorKey: "bindVars",
    accessorFn: (row: any) => {
      return JSON.stringify(row.bindVars);
    }
  },
  {
    header: "User",
    id: "user",
    accessorKey: "user"
  },
  {
    header: "Peak Memory Usage",
    id: "peakMemoryUsage",
    accessorKey: "peakMemoryUsage"
  },
  {
    header: "Runtime",
    id: "runTime",
    accessorKey: "runTime",
    accessorFn: (row: any) => {
      return row.runTime.toFixed(2) + "s";
    }
  },
  {
    header: "Started",
    accessorKey: "started",
    accessorFn: (row: any) => {
      return moment(row.started).format("YYYY-MM-DD HH:mm:ss");
    }
  },
  {
    header: "Actions",
    id: "actions",
    cell: (info: CellContext<QueryInfo, string>) => <ActionCell info={info} />
  }
];
const ActionCell = ({ info }: { info: CellContext<QueryInfo, string> }) => {
  const [showDeleteModal, setShowDeleteModal] = React.useState(false);
  const [queryToDelete, setQueryToDelete] = React.useState<string | null>(null);
  const onDeleteClick = (queryId: string) => {
    setShowDeleteModal(true);
    setQueryToDelete(queryId);
  };
  const { refetchRunningQueries } = useQueryContext();

  const id = info.row.original.id;
  return (
    <>
      <IconButton
        aria-label="Kill Query"
        icon={<DeleteIcon />}
        colorScheme="red"
        variant="ghost"
        onClick={() => onDeleteClick(id)}
      />
      <DeleteQueryModal
        isOpen={showDeleteModal}
        onClose={() => {
          setShowDeleteModal(false);
          setQueryToDelete(null);
        }}
        queryToDelete={queryToDelete}
        refetchQueries={refetchRunningQueries}
      />
    </>
  );
};
export const RunningQueries = () => {
  const { runningQueries, refetchRunningQueries } = useQueryContext();
  useEffect(() => {
    refetchRunningQueries();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);
  const tableInstance = useSortableReactTable<QueryInfo>({
    data: runningQueries || [],
    columns: TABLE_COLUMNS
  });

  return <ReactTable table={tableInstance} />;
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
