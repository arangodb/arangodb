import { QueryInfo } from "arangojs/database";
import React, { useEffect } from "react";
import { getCurrentDB } from "../../../utils/arangoClient";

export const useFetchSlowQueries = () => {
  const [runningQueries, setSlowQueries] = React.useState<QueryInfo[]>([]);
  const fetchSlowQueries = async () => {
    const currentDb = getCurrentDB();
    const runningQueries = await currentDb.listSlowQueries();
    setSlowQueries(runningQueries);
  };
  useEffect(() => {
    fetchSlowQueries();
  }, []);
  return { runningQueries, refetchQueries: fetchSlowQueries };
};
