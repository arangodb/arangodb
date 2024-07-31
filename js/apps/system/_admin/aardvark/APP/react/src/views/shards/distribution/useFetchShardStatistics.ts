import useSWR from "swr";
import { getAdminRouteForCurrentDB } from "../../../utils/arangoClient";
import { ClusterHealth, useFetchClusterHealth } from "./useFetchClusterHealth";

type ShardStatistics = {
  collections: number;
  databases: number;
  followers: number;
  leaders: number;
  realLeaders: number;
  servers: number;
  shards: number;
};

export type ServerShardStatisticsData = {
  id: string;
  name: string;
  shardsPct: string;
  leadersPct: string;
  followersPct: string;
} & ShardStatistics;

export type ShardStatisticsData = {
  total: ShardStatistics;
  table: ServerShardStatisticsData[];
  shards: { data: number; label: string }[];
  leaders: { data: number; label: string }[];
  followers: { data: number; label: string }[];
};

export const useFetchShardStatistics = () => {
  const clusterHealth = useFetchClusterHealth();
  const globalStats = useSWR<ShardStatistics>(
    "cluster/shardStatistics",
    async () => {
      const res = await getAdminRouteForCurrentDB().get(
        "cluster/shardStatistics"
      );
      return res.parsedBody.result;
    }
  );
  const localStats = useSWR<Record<string, ShardStatistics>>(
    "cluster/shardStatistics?details=true&DBserver=all",
    async () => {
      const res = await getAdminRouteForCurrentDB().get(
        "cluster/shardStatistics",
        { details: "true", DBserver: "all" }
      );
      return res.parsedBody.result;
    }
  );
  const meta = {
    isLoading:
      clusterHealth.isLoading || globalStats.isLoading || localStats.isLoading,
    isValidating:
      clusterHealth.isValidating ||
      globalStats.isValidating ||
      localStats.isValidating,
    error: clusterHealth.error || globalStats.error || localStats.error,
    mutate: async () => {
      await Promise.all([globalStats.mutate(), localStats.mutate()]);
    }
  };
  if (!clusterHealth.data || !globalStats.data || !localStats.data) {
    return { ...meta, data: undefined };
  }
  return {
    ...meta,
    data: buildStats(globalStats.data, localStats.data, clusterHealth.data)
  };
};

const buildStats = (
  globalStatistics: ShardStatistics,
  localStatistics: Record<string, ShardStatistics>,
  clusterHealth: ClusterHealth
): ShardStatisticsData => {
  const serverIds = Object.keys(localStatistics)
    .map(id => ({
      id,
      name: clusterHealth.Health[id].ShortName || id
    }))
    .sort((a, b) => a.name.localeCompare(b.name));
  return {
    total: globalStatistics,
    table: serverIds.map(({ id, name }) => ({
      id,
      name,
      shardsPct: toPercent(localStatistics[id].shards, globalStatistics.shards),
      leadersPct: toPercent(
        localStatistics[id].leaders,
        globalStatistics.leaders
      ),
      followersPct: toPercent(
        localStatistics[id].followers,
        globalStatistics.followers
      ),
      ...localStatistics[id]
    })),
    shards: serverIds.map(({ id, name }) => ({
      data: localStatistics[id].shards,
      label: name
    })),
    leaders: serverIds.map(({ id, name }) => ({
      data: localStatistics[id].leaders,
      label: name
    })),
    followers: serverIds.map(({ id, name }) => ({
      data: localStatistics[id].followers,
      label: name
    }))
  };
};

function toPercent(value: number, total: number): string {
  return `${((value / total) * 100).toFixed(2)} %`;
}
