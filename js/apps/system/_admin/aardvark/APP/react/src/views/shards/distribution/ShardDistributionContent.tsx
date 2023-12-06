import React, { useEffect } from "react";
import { Cell, Legend, Pie, PieChart, Tooltip } from "recharts";
import {
  ServerShardStatisticsData,
  useFetchShardStatistics
} from "./useFetchShardStatistics";
import d3 from "d3";
import {
  Flex,
  Heading,
  Spinner,
  Stack,
  Stat,
  StatLabel,
  StatNumber,
  Text
} from "@chakra-ui/react";
import { useSortableReactTable } from "../../../components/table/useSortableReactTable";
import { createColumnHelper } from "@tanstack/react-table";
import { ReactTable } from "../../../components/table/ReactTable";

const columnHelper = createColumnHelper<ServerShardStatisticsData>();

const TABLE_COLUMNS = [
  columnHelper.accessor("name", {
    header: "DB Server",
    id: "name",
    meta: {
      filterType: "text"
    }
  }),
  columnHelper.accessor("shards", {
    header: "Total (absolute)",
    id: "shards"
  }),
  columnHelper.accessor("shardsPct", {
    header: "Total (percent)",
    id: "shardsPct"
  }),
  columnHelper.accessor("leaders", {
    header: "Leaders (absolute)",
    id: "leaders"
  }),
  columnHelper.accessor("leadersPct", {
    header: "Leaders (percent)",
    id: "leadersPct"
  }),
  columnHelper.accessor("followers", {
    header: "Followers (absolute)",
    id: "followers"
  }),
  columnHelper.accessor("followersPct", {
    header: "Followers (percent)",
    id: "followersPct"
  })
];

export const ShardDistributionContent = ({
  refetchToken
}: {
  refetchToken: any;
}) => {
  const initialRefetchToken = React.useRef(refetchToken);
  const colors = d3.scale.category20().range();
  const { data: statistics, mutate } = useFetchShardStatistics();
  const tableInstance = useSortableReactTable({
    data: statistics?.table ?? [],
    columns: TABLE_COLUMNS
  });

  useEffect(() => {
    if (refetchToken !== initialRefetchToken.current) {
      mutate();
    }
  }, [mutate, refetchToken]);

  return (
    <Stack gap="4">
      <Stack direction="row" alignItems="center" justifyContent="space-around">
        <Stat>
          <StatLabel>Databases</StatLabel>
          <StatNumber>
            {statistics ? (
              statistics.total.databases
            ) : (
              <Spinner size={"xs"} marginLeft={1} />
            )}
          </StatNumber>
        </Stat>
        <Stat>
          <StatLabel>Collections</StatLabel>
          <StatNumber>
            {statistics ? (
              statistics.total.collections
            ) : (
              <Spinner size={"xs"} marginLeft={1} />
            )}
          </StatNumber>
        </Stat>
        <Stat>
          <StatLabel>Total Shards</StatLabel>
          <StatNumber>
            {statistics ? (
              statistics.total.shards
            ) : (
              <Spinner size={"xs"} marginLeft={1} />
            )}
          </StatNumber>
        </Stat>
        <Stat>
          <StatLabel>DB Servers with Shards</StatLabel>
          <StatNumber>
            {statistics ? (
              statistics.total.servers
            ) : (
              <Spinner size={"xs"} marginLeft={1} />
            )}
          </StatNumber>
        </Stat>
        <Stat>
          <StatLabel>Leader Shards</StatLabel>
          <StatNumber>
            {statistics ? (
              statistics.total.leaders
            ) : (
              <Spinner size={"xs"} marginLeft={1} />
            )}
          </StatNumber>
        </Stat>
        <Stat>
          <StatLabel>Shard Group Leader Shards</StatLabel>
          <StatNumber>
            {statistics ? (
              statistics.total.realLeaders
            ) : (
              <Spinner size={"xs"} marginLeft={1} />
            )}
          </StatNumber>
        </Stat>
        <Stat>
          <StatLabel>Follower Shards</StatLabel>
          <StatNumber>
            {statistics ? (
              statistics.total.followers
            ) : (
              <Spinner size={"xs"} marginLeft={1} />
            )}
          </StatNumber>
        </Stat>
      </Stack>

      <Text fontSize={"lg"}>Shard distribution</Text>

      <Flex
        direction="row"
        alignItems="center"
        justifyContent="space-around"
        padding="2"
      >
        <Stack alignItems="center">
          <Heading size="sm">Total Shard Distribution</Heading>
          <PieChart width={300} height={300}>
            <Tooltip />
            <Pie
              data={statistics?.shards ?? []}
              dataKey="data"
              nameKey="label"
              label
            >
              {statistics?.shards.map(({ label }, i) => (
                <Cell key={label} fill={colors[i % colors.length]} />
              ))}
            </Pie>
            <Legend />
          </PieChart>
        </Stack>

        <Stack alignItems="center">
          <Heading size="sm">Leader Shard Distribution</Heading>
          <PieChart width={300} height={300}>
            <Tooltip />
            <Pie
              data={statistics?.leaders ?? []}
              dataKey="data"
              nameKey="label"
              label
            >
              {statistics?.leaders.map(({ label }, i) => (
                <Cell key={label} fill={colors[i % colors.length]} />
              ))}
            </Pie>
            <Legend />
          </PieChart>
        </Stack>

        <Stack alignItems="center">
          <Heading size="sm">Follower Shard Distribution</Heading>
          <PieChart width={300} height={300}>
            <Tooltip />
            <Pie
              data={statistics?.followers ?? []}
              dataKey="data"
              nameKey="label"
              label
            >
              {statistics?.followers.map(({ label }, i) => (
                <Cell key={label} fill={colors[i % colors.length]} />
              ))}
            </Pie>
            <Legend />
          </PieChart>
        </Stack>
      </Flex>

      <ReactTable<ServerShardStatisticsData> table={tableInstance} />
    </Stack>
  );
};
