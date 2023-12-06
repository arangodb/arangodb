import { getAdminRouteForCurrentDB } from "../../../utils/arangoClient";
import useSWR from "swr";

export type ClusterHealth = {
  ClusterId: string;
  Health: Record<
    string,
    | {
        CanBeDeleted: boolean;
        Endpoint: string;
        Engine: "rocksdb";
        Host?: undefined;
        LastAckedTime: number;
        Leader: string;
        Leading: boolean;
        Role: "Agent";
        ShortName?: undefined;
        Status: string;
        SyncStatus?: undefined;
        SyncTime?: undefined;
        Timestamp?: undefined;
        Version: string;
      }
    | {
        CanBeDeleted: boolean;
        Endpoint: string;
        Engine: "rocksdb";
        Host: string;
        LastAckedTime: string;
        Leader: string;
        Leading?: undefined;
        Role: "Coordinator" | "DBServer";
        ShortName: string;
        Status: string;
        SyncStatus: string;
        SyncTime: string;
        Timestamp: string;
        Version: string;
      }
  >;
};

export const useFetchClusterHealth = () => {
  return useSWR<ClusterHealth>("cluster/health", async () => {
    const res = await getAdminRouteForCurrentDB().get("cluster/health");
    const { ClusterId, Health } = res.body;
    return { ClusterId, Health };
  });
};
