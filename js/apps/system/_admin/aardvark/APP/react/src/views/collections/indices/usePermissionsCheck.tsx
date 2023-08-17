import { Dispatch, SetStateAction, useEffect } from "react";

export const usePermissionsCheck = ({
  setReadOnly,
  collectionName
}: {
  collectionName: string;
  setReadOnly: Dispatch<SetStateAction<boolean>>;
}) => {
  useEffect(() => {
    window.arangoHelper.checkCollectionPermissions(collectionName, () =>
      setReadOnly(true)
    );
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [collectionName]);
};
