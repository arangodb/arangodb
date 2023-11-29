import { useSetupBreadcrumbs } from "../../../components/hooks/useSetupBreadcrumbs";

export const useSetupIndicesViewBreadcrumbs = ({
  collectionName,
  readOnly
}: {
  readOnly: boolean;
  collectionName: string;
}) => {
  let breadcrumbText = `Collection: ${collectionName}`;
  if (readOnly) {
    breadcrumbText = `Collection: ${collectionName} (read-only)`;
  }
  useSetupBreadcrumbs({ breadcrumbText });
};
