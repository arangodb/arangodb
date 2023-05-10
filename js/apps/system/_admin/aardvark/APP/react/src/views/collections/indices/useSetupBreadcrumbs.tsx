import { useEffect } from "react";

export const useSetupBreadcrumbs = ({
  collectionName,
  readOnly
}: {
  readOnly: boolean;
  collectionName: string;
}) => {
  useEffect(() => {
    const breadcrumbDiv = document.querySelector(
      "#subNavigationBar .breadcrumb"
    ) as HTMLDivElement;
    if (breadcrumbDiv) {
      let text = `Collection: ${collectionName}`;
      if (readOnly) {
        text = `Collection: ${collectionName} (read-only)`;
      }
      breadcrumbDiv.innerText = text;
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [collectionName, readOnly]);
};
