import React from "react";

export const useDatabaseReadOnly = () => {
  const [readOnly, setReadOnly] = React.useState<boolean>(false);
  const [isLoading, setIsLoading] = React.useState<boolean>(true);
  window.arangoHelper.checkDatabasePermissions(
    () => {
      setReadOnly(true);
      setIsLoading(false);
    },
    () => {
      setReadOnly(false);
      setIsLoading(false);
    },
    () => {
      setReadOnly(false);
      setIsLoading(false);
    }
  );
  return { readOnly, isLoading };
};
