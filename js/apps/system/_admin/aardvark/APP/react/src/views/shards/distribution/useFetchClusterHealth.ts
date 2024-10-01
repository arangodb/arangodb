import { getAdminRouteForCurrentDB } from "../../../utils/arangoClient";
import useSWR from "swr";

type CommonHealthType = {
  CanBeDeleted: boolean;
  Endpoint: string;
  Engine: "rocksdb";
  Leader: string;
  Status: string;
  Version: string;
};

type AgentHealthType = CommonHealthType & {
  Host?: undefined;
  LastAckedTime: number;
  Leading: boolean;
  Role: "Agent";
  ShortName?: undefined;
  SyncStatus?: undefined;
  SyncTime?: undefined;
  Timestamp?: undefined;
};

type CoordinatorOrDBServerHealthType = CommonHealthType & {
  Host: string;
  LastAckedTime: string;
  Leading?: undefined;
  Role: "Coordinator" | "DBServer";
  ShortName: string;
  SyncStatus: string;
  SyncTime: string;
  Timestamp: string;
};

export type ClusterHealth = {
  ClusterId: string;
  Health: Record<string, AgentHealthType | CoordinatorOrDBServerHealthType>;
};

export const useFetchClusterHealth = () => {
  return useSWR<ClusterHealth>("cluster/health", async () => {
    const res = await getAdminRouteForCurrentDB().get("cluster/health");
    const { ClusterId, Health } = res.parsedBody;
    return { ClusterId, Health };
  });
};
