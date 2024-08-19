import { ChevronDownIcon, ChevronUpIcon } from "@chakra-ui/icons";
import { Flex, Icon, Tag, Text } from "@chakra-ui/react";
import {
  CellContext,
  createColumnHelper,
  getExpandedRowModel,
  Table
} from "@tanstack/react-table";
import React, { useMemo } from "react";

import { useSortableReactTable } from "../../../components/table/useSortableReactTable";
import { InfoTooltip } from "../../../components/tooltip/InfoTooltip";
import {
  CollectionType,
  DatabaseTableType
} from "./CollectionsPermissionsTable";
import {
  DatabasePermissionSwitch,
  getIsDefaultRow
} from "./DatabasePermissionSwitch";
import { useFetchDatabasePermissions } from "./useFetchDatabasePermissions";
import {
  SystemDatabaseActionState,
  usePermissionChangeHandlers
} from "./usePermissionChangeHandlers";

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
  systemDatabaseActionState: SystemDatabaseActionState | undefined;
  onConfirmSystemDatabasePermissionChange: () => void;
  onCancelSystemDatabasePermissionChange: () => void;
}>({} as any);

export const useUserPermissionsContext = () => {
  return React.useContext(UserPermissionsContext);
};

const usePermissionTableData = () => {
  const { databasePermissions, refetchDatabasePermissions } =
    useFetchDatabasePermissions();

  const databaseTable = useMemo(
    () =>
      databasePermissions
        ? (Object.entries(databasePermissions)
            .map(([databaseName, permissionObject]) => {
              return {
                databaseName,
                permission: permissionObject.permission,
                collections: Object.entries(permissionObject.collections || {})
                  .map(([collectionName, collectionPermission]) => {
                    // filter out built-in collections
                    if (collectionName.startsWith("_")) {
                      return null;
                    }
                    return {
                      collectionName,
                      permission: collectionPermission
                    };
                  })
                  .filter(Boolean) as CollectionType[]
              };
            })
            .filter(Boolean) as DatabaseTableType[])
        : [],
    [databasePermissions]
  );

  if (!databasePermissions) {
    return {
      databaseTable: []
    };
  }
  let serverLevelDefaultPermission;
  try {
    serverLevelDefaultPermission = databasePermissions["*"].permission;
  } catch (ignore) {
    // just ignore, not part of the response
  }
  return {
    databaseTable: databaseTable,
    serverLevelDefaultPermission,
    refetchDatabasePermissions
  };
};

const columnHelper = createColumnHelper<DatabaseTableType>();

const permissionColumns = [
  columnHelper.accessor(row => (row.permission === "rw" ? true : false), {
    header: () => {
      return (
        <Flex alignItems="center">
          <Text>Administrate</Text>
          <InfoTooltip
            boxProps={{
              color: "gray.700"
            }}
            label="Allows creating/dropping of collections and setting user permissions in the database."
          />
        </Flex>
      );
    },
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
    header: () => {
      return (
        <Flex alignItems="center">
          <Text>Access</Text>
          <InfoTooltip
            boxProps={{
              color: "gray.700"
            }}
            label="Allows access to the database. User cannot create or drop collections."
          />
        </Flex>
      );
    },
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
    header: () => {
      return (
        <Flex alignItems="center">
          <Text>No Access</Text>
          <InfoTooltip
            boxProps={{
              color: "gray.700"
            }}
            label="User has no access to the database."
          />
        </Flex>
      );
    },
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
      header: () => {
        return (
          <Flex alignItems="center">
            <Text>Use Default</Text>
            <InfoTooltip
              boxProps={{
                color: "gray.700"
              }}
              label="Access level is unspecified. Database default (*) will be used."
            />
          </Flex>
        );
      },
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
const ExpandIcon = ({
  info
}: {
  info: CellContext<DatabaseTableType, unknown>;
}) => {
  if (getIsDefaultRow(info) || !info.row.getCanExpand()) {
    return null;
  }
  return (
    <Flex padding="2">
      <Icon
        display="block"
        aria-label="Expand/collapse row"
        as={info.row.getIsExpanded() ? ChevronUpIcon : ChevronDownIcon}
      />
    </Flex>
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
            <InfoTooltip
              boxProps={{
                color: "gray.700"
              }}
              label="Default access level for databases, if authorization level is not specified."
            />
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
          <ExpandIcon info={info} />
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
  const {
    handleDatabaseCellClick,
    handleCollectionCellClick,
    systemDatabaseActionState,
    onConfirmSystemDatabasePermissionChange,
    onCancelSystemDatabasePermissionChange
  } = usePermissionChangeHandlers({
    refetchDatabasePermissions
  });

  const tableInstance = useSortableReactTable<DatabaseTableType>({
    data: databaseTable || [],
    columns: TABLE_COLUMNS,
    defaultSorting: [
      {
        id: "databaseName",
        desc: false
      }
    ],
    defaultFilters: [],
    storageKey: "userPermissions",
    getExpandedRowModel: getExpandedRowModel(),
    getRowCanExpand: () => true
  });

  return (
    <UserPermissionsContext.Provider
      value={{
        handleDatabaseCellClick,
        tableInstance,
        refetchDatabasePermissions,
        handleCollectionCellClick,
        databaseTable,
        systemDatabaseActionState,
        onConfirmSystemDatabasePermissionChange,
        onCancelSystemDatabasePermissionChange
      }}
    >
      {children}
    </UserPermissionsContext.Provider>
  );
};
