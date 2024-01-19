import { QueryInfo } from "arangojs/database";
import React, { useEffect } from "react";
import { getCurrentDB } from "../../../utils/arangoClient";

export const useFetchRunningQueries = () => {
  const [runningQueries, setRunningQueries] = React.useState<QueryInfo[]>([]);
  const fetchRunningQueries = async () => {
    const currentDb = getCurrentDB();
    const runningQueries = await currentDb.listRunningQueries();
    setRunningQueries(runningQueries);
  };
  useEffect(() => {
    fetchRunningQueries();
  }, []);
  return { runningQueries, refetchRunningQueries: fetchRunningQueries };
};
