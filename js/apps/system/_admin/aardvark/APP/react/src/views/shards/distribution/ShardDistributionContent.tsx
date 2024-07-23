import {
  Box,
  Flex,
  Grid,
  Heading,
  Spinner,
  Stack,
  Stat,
  StatLabel,
  StatNumber,
  Text
} from "@chakra-ui/react";
import { createColumnHelper } from "@tanstack/react-table";
import d3 from "d3";
import React from "react";
import { Cell, Legend, Pie, PieChart, Tooltip } from "recharts";
import { ReactTable } from "../../../components/table/ReactTable";
import { useSortableReactTable } from "../../../components/table/useSortableReactTable";
import { RebalanceShards } from "./RebalanceShards";
import {
  ServerShardStatisticsData,
  ShardStatisticsData,
  useFetchShardStatistics
} from "./useFetchShardStatistics";

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
  readOnly
}: {
  readOnly: boolean;
}) => {
  const { data: statistics, mutate } = useFetchShardStatistics();
  if (!statistics) {
    return <Spinner />;
  }
  return (
    <Box width="100%" padding="5" background="white">
      <ShardDistributionCharts statistics={statistics} />
      {!readOnly && <RebalanceShards refetchStats={() => mutate()} />}
    </Box>
  );
};

const renderCustomizedLabel = ({
  cx,
  cy,
  midAngle,
  innerRadius,
  outerRadius,
  percent
}: any) => {
  const RADIAN = Math.PI / 180;
  const radius = 25 + innerRadius + (outerRadius - innerRadius);
  const x = cx + radius * Math.cos(-midAngle * RADIAN);
  const y = cy + radius * Math.sin(-midAngle * RADIAN);
  return (
    <text
      x={x}
      y={y}
      fill="#000000"
      textAnchor={x > cx ? "start" : "end"}
      dominantBaseline="central"
    >
      {`${(percent * 100).toFixed(0)}%`}
    </text>
  );
};
const ShardDistributionCharts = ({
  statistics
}: {
  statistics: ShardStatisticsData;
}) => {
  const colors = d3.scale.category20().range();
  const tableInstance = useSortableReactTable({
    data: statistics.table ?? [],
    columns: TABLE_COLUMNS
  });

  return (
    <Stack gap="4">
      <Grid gap="2" templateColumns="1fr 1fr 1fr 1fr">
        <Stat gap="2">
          <StatNumber lineHeight="base">
            {statistics.total.databases}
          </StatNumber>
          <StatLabel>Databases</StatLabel>
        </Stat>
        <Stat gap="2">
          <StatNumber lineHeight="base">
            {statistics.total.collections}
          </StatNumber>
          <StatLabel>Collections</StatLabel>
        </Stat>
        <Stat gap="2">
          <StatNumber lineHeight="base">{statistics.total.shards}</StatNumber>
          <StatLabel>Total Shards</StatLabel>
        </Stat>
        <Stat gap="2">
          <StatNumber lineHeight="base">{statistics.total.servers}</StatNumber>
          <StatLabel>DB Servers with Shards</StatLabel>
        </Stat>
      </Grid>
      <Grid gap="2" templateColumns="1fr 1fr 1fr 1fr">
        <Stat gap="2">
          <StatNumber lineHeight="base">{statistics.total.leaders}</StatNumber>
          <StatLabel>Leader Shards</StatLabel>
        </Stat>
        <Stat gap="2">
          <StatNumber lineHeight="base">
            {statistics.total.realLeaders}
          </StatNumber>
          <StatLabel>Shard Group Leader Shards</StatLabel>
        </Stat>
        <Stat gap="2">
          <StatNumber lineHeight="base">
            {statistics.total.followers}
          </StatNumber>
          <StatLabel>Follower Shards</StatLabel>
        </Stat>
      </Grid>

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
              data={statistics.shards ?? []}
              dataKey="data"
              nameKey="label"
              label={renderCustomizedLabel}
            >
              {statistics.shards.map(({ label }, i) => (
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
              data={statistics.leaders ?? []}
              dataKey="data"
              nameKey="label"
              label={renderCustomizedLabel}
            >
              {statistics.leaders?.map(({ label }, i) => (
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
              data={statistics.followers ?? []}
              dataKey="data"
              nameKey="label"
              label={renderCustomizedLabel}
            >
              {statistics.followers.map(({ label }, i) => (
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
