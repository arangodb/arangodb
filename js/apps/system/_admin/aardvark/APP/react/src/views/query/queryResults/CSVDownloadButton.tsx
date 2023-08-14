import { Button } from "@chakra-ui/react";
import Papa from "papaparse";
import React from "react";
import { downloadBlob } from "../../../utils/downloadHelper";
import { QueryResultType } from "../ArangoQuery.types";
import { getAllowCSVDownload } from "./QueryExecuteResult";

export const CSVDownloadButton = ({
  queryResult
}: {
  queryResult: QueryResultType;
}) => {
  const allowCSVDownload = getAllowCSVDownload(queryResult);

  const onDownloadCSV = async () => {
    const csv = Papa.unparse(queryResult.result);
    const blob = new Blob([csv], { type: "text/csv;charset=utf-8;" });
    const url = window.URL.createObjectURL(blob);
    downloadBlob(url, "query-result.csv");
  };
  if (!allowCSVDownload) {
    return null;
  }
  return (
    <Button size="sm" onClick={onDownloadCSV}>
      Download CSV
    </Button>
  );
};
