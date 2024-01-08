import { useEffect } from "react";

export const useSetupBreadcrumbs = ({
  breadcrumbText
}: {
  breadcrumbText: string;
}) => {
  const addBreadcrumb = (breadcrumbText: string) => {
    const breadcrumbDiv = document.querySelector(
      "#subNavigationBar .breadcrumb"
    ) as HTMLDivElement;
    if (breadcrumbDiv) {
      breadcrumbDiv.innerText = breadcrumbText;
      return true;
    }
    return false;
  };
  useEffect(() => {
    const isBreadcrumbAdded = addBreadcrumb(breadcrumbText);
    if (isBreadcrumbAdded) {
      return;
    }
    // if not added, retry after 500ms
    const timer = window.setTimeout(() => {
      addBreadcrumb(breadcrumbText);
    }, 500);
    return () => {
      window.clearTimeout(timer);
    };
  }, [breadcrumbText]);
};
