import { ChevronDownIcon, ChevronUpIcon } from "@chakra-ui/icons";
import { Flex, IconButton, Tag, Text } from "@chakra-ui/react";
import {
  CellContext,
  createColumnHelper,
  getExpandedRowModel,
  Table
} from "@tanstack/react-table";
import React from "react";
import { useSortableReactTable } from "../../../components/table/useSortableReactTable";
import { getCurrentDB } from "../../../utils/arangoClient";
import { notifyError } from "../../../utils/notifications";
import {
  CollectionType,
  DatabaseTableType
} from "./CollectionsPermissionsTable";
import {
  DatabasePermissionSwitch,
  getIsDefaultRow
} from "./DatabasePermissionSwitch";
import {
  useFetchDatabasePermissions,
  useUsername
} from "./useFetchDatabasePermissions";

const UserPermissionsContext = React.createContext<{
  tableInstance: Table<DatabaseTableType>;
  refetchDatabasePermissions?: () => void;
  databaseTable: DatabaseTableType[];
  handleDatabaseCellClick: ({
    info,
    permission
  }: {
    info: CellContext<DatabaseTableType, unknown>;
    permission: string;
  }) => Promise<void>;
  handleCollectionCellClick: ({
    permission,
    info,
    databaseName
  }: {
    permission: string;
    info: CellContext<CollectionType, unknown>;
    databaseName: string;
  }) => Promise<void>;
}>({} as any);

export const useUserPermissionsContext = () => {
  return React.useContext(UserPermissionsContext);
};

const usePermissionTableData = () => {
  const { databasePermissions, refetchDatabasePermissions } =
    useFetchDatabasePermissions();
  if (!databasePermissions)
    return {
      databaseTable: []
    };
  const databaseTable = Object.entries(databasePermissions)
    .map(([databaseName, permissionObject]) => {
      return {
        databaseName,
        permission: permissionObject.permission,
        collections: Object.entries(permissionObject.collections || {}).map(
          ([collectionName, collectionPermission]) => {
            return {
              collectionName,
              permission: collectionPermission
            };
          }
        )
      };
    })
    .filter(Boolean) as DatabaseTableType[];
  let serverLevelDefaultPermission;
  try {
    serverLevelDefaultPermission = databasePermissions["*"].permission;
  } catch (ignore) {
    // just ignore, not part of the response
  }
  return {
    databaseTable: [...databaseTable],
    serverLevelDefaultPermission,
    refetchDatabasePermissions
  };
};

const columnHelper = createColumnHelper<DatabaseTableType>();

const permissionColumns = [
  columnHelper.accessor(row => (row.permission === "rw" ? true : false), {
    header: "Administrate",
    id: "rw",
    cell: info => {
      return (
        <DatabasePermissionSwitch info={info} checked={info.cell.getValue()} />
      );
    },
    enableSorting: false,
    meta: {
      filterType: "single-select"
    }
  }),
  columnHelper.accessor(row => (row.permission === "ro" ? true : false), {
    header: "Access",
    id: "ro",
    cell: info => {
      return (
        <DatabasePermissionSwitch info={info} checked={info.cell.getValue()} />
      );
    },
    enableSorting: false,
    meta: {
      filterType: "single-select"
    }
  }),
  columnHelper.accessor(row => (row.permission === "none" ? true : false), {
    header: "No Accesss",
    id: "none",
    cell: info => {
      return (
        <DatabasePermissionSwitch info={info} checked={info.cell.getValue()} />
      );
    },
    enableSorting: false,
    meta: {
      filterType: "single-select"
    }
  }),
  columnHelper.accessor(
    row => (row.permission === "undefined" ? true : false),
    {
      header: "Use Default",
      id: "undefined",
      cell: info => {
        return (
          <DatabasePermissionSwitch
            info={info}
            checked={info.cell.getValue()}
          />
        );
      },
      enableSorting: false,
      meta: {
        filterType: "single-select"
      }
    }
  )
];
const ExpandButton = ({
  info
}: {
  info: CellContext<DatabaseTableType, unknown>;
}) => {
  if (getIsDefaultRow(info) || !info.row.getCanExpand()) {
    return null;
  }
  return (
    <IconButton
      display="block"
      padding="0"
      variant="unstyled"
      size="sm"
      aria-label="Expand/collapse row"
      icon={info.row.getIsExpanded() ? <ChevronUpIcon /> : <ChevronDownIcon />}
      _focus={{
        boxShadow: "none"
      }}
      onClick={info.row.getToggleExpandedHandler()}
    />
  );
};
export const TABLE_COLUMNS = [
  columnHelper.accessor("databaseName", {
    header: "Database",
    id: "databaseName",
    meta: {
      filterType: "text"
    },
    sortingFn: (rowA, rowB, columnId) => {
      if (rowA.original.databaseName === "*") {
        return 0;
      }
      return rowA
        .getValue<string>(columnId)
        ?.localeCompare(rowB.getValue<string>(columnId) || "");
    },
    cell: info => {
      if (getIsDefaultRow(info)) {
        return (
          <Flex padding="2">
            <Tag>Default</Tag>
          </Flex>
        );
      }
      return (
        <Flex
          _hover={{
            bg: "gray.100"
          }}
          height="full"
          alignItems="center"
          cursor={info.row.getCanExpand() ? "pointer" : ""}
          onClick={info.row.getToggleExpandedHandler()}
        >
          <ExpandButton info={info} />
          <Text fontWeight={info.row.getIsExpanded() ? "semibold" : "normal"}>
            {info.cell.getValue()}
          </Text>
        </Flex>
      );
    }
  }),
  ...permissionColumns
];

export const UserPermissionsContextProvider = ({
  children
}: {
  children: React.ReactNode;
}) => {
  const { databaseTable, refetchDatabasePermissions } =
    usePermissionTableData();
  const { username } = useUsername();
  const { handleDatabaseCellClick, handleCollectionCellClick } =
    usePermissionChangeHandlers({
      refetchDatabasePermissions
    });

  const tableInstance = useSortableReactTable<DatabaseTableType>({
    data: databaseTable || [],
    columns: TABLE_COLUMNS,
    initialSorting: [
      {
        id: "databaseName",
        desc: false
      }
    ],
    getExpandedRowModel: getExpandedRowModel(),
    getRowCanExpand: () => true
  });

  React.useEffect(() => {
    window.arangoHelper.buildUserSubNav(username, "Permissions");
  });

  return (
    <UserPermissionsContext.Provider
      value={{
        handleDatabaseCellClick,
        tableInstance,
        refetchDatabasePermissions,
        handleCollectionCellClick,
        databaseTable
      }}
    >
      {children}
    </UserPermissionsContext.Provider>
  );
};

const usePermissionChangeHandlers = ({
  refetchDatabasePermissions
}: {
  refetchDatabasePermissions?: () => void;
}) => {
  const { username } = useUsername();

  const handleDatabaseCellClick = async ({
    info,
    permission
  }: {
    info: CellContext<DatabaseTableType, unknown>;
    permission: string;
  }) => {
    const { databaseName } = info.row.original;
    try {
      const currentDbRoute = getCurrentDB().route(
        `_api/user/${username}/database/${databaseName}`
      );
      if (permission === "undefined") {
        await currentDbRoute.delete();
        refetchDatabasePermissions?.();
        return;
      }
      await currentDbRoute.put({
        grant: permission
      });
      refetchDatabasePermissions?.();
    } catch (e: any) {
      notifyError(e.message);
    }
  };

  const handleCollectionCellClick = async ({
    permission,
    info,
    databaseName
  }: {
    permission: string;
    info: CellContext<CollectionType, unknown>;
    databaseName: string;
  }) => {
    const { collectionName } = info.row.original;
    try {
      const currentDbRoute = getCurrentDB().route(
        `_api/user/${username}/database/${databaseName}/${collectionName}`
      );
      if (permission === "undefined") {
        await currentDbRoute.delete();
        refetchDatabasePermissions?.();
        return;
      }
      await currentDbRoute.put({ grant: permission });
      refetchDatabasePermissions?.();
    } catch (e: any) {
      notifyError(e.message);
    }
  };
  return {
    handleDatabaseCellClick,
    handleCollectionCellClick
  };
};
