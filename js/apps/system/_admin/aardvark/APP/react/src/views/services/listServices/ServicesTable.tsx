import { Link, Stack } from "@chakra-ui/react";
import { createColumnHelper } from "@tanstack/react-table";
import { some } from "lodash";
import React from "react";
import { Link as RouterLink } from "react-router-dom";
import { ReactTable } from "../../../components/table/ReactTable";
import { TableControl } from "../../../components/table/TableControl";
import { useSortableReactTable } from "../../../components/table/useSortableReactTable";
import { ServiceDescription } from "../Service.types";
import { useServicesContext } from "../ServicesContext";
const columnHelper = createColumnHelper<ServiceDescription>();

const needsConfiguration = (config: { [key: string]: any }) => {
  return some(config, (cfg: any) => {
    return cfg.current === undefined && cfg.required !== false;
  });
};

const needsDepsConfiguration = (deps: { [key: string]: any }) => {
  return some(deps, (dep: any) => {
    return dep.current === undefined && dep.definition.required !== false;
  });
};
const TABLE_COLUMNS = [
  columnHelper.accessor("mount", {
    header: "Mount",
    id: "mount",
    cell: info => {
      const cellValue = info.cell.getValue();
      return (
        <Link
          as={RouterLink}
          to={`/service/${encodeURIComponent(cellValue)}`}
          textDecoration="underline"
          color="blue.500"
          _hover={{
            color: "blue.600"
          }}
        >
          {cellValue}
        </Link>
      );
    },
    meta: {
      filterType: "text"
    }
  }),
  columnHelper.accessor("name", {
    header: "Name",
    id: "name",
    meta: {
      filterType: "text"
    }
  }),
  columnHelper.accessor("version", {
    header: "Version",
    id: "version",
    meta: {
      filterType: "single-select"
    }
  }),
  columnHelper.accessor("system", {
    header: "System",
    id: "system",
    meta: {
      filterType: "single-select"
    }
  }),
  columnHelper.accessor("development", {
    header: "Development",
    id: "development",
    meta: {
      filterType: "single-select"
    }
  }),
  columnHelper.accessor(
    row =>
      needsConfiguration(row.config) ? "Needs Configuration" : "Configured",
    {
      id: "configured",
      header: "Configuration Status",
      meta: {
        filterType: "single-select"
      }
    }
  ),
  columnHelper.accessor(
    row =>
      needsDepsConfiguration(row.deps)
        ? "Needs Deps configuration"
        : "Configured",
    {
      id: "dependenciesConfigured",
      header: "Dependencies Status",
      meta: {
        filterType: "single-select"
      }
    }
  )
];

export const ServicesTable = () => {
  const { services } = useServicesContext();
  const tableInstance = useSortableReactTable<ServiceDescription>({
    data: services || [],
    columns: TABLE_COLUMNS,
    defaultSorting: [
      {
        id: "name",
        desc: false
      }
    ],
    defaultFilters: [
      {
        id: "system",
        value: false
      }
    ],
    storageKey: "services"
  });
  return (
    <Stack>
      <TableControl<ServiceDescription>
        table={tableInstance}
        columns={TABLE_COLUMNS}
      />
      <ReactTable<ServiceDescription>
        table={tableInstance}
        emptyStateMessage="No services found"
      />
    </Stack>
  );
};
