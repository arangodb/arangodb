import React from "react";
import FieldView from "./Components/FieldView";
import { ViewLinksBreadcrumbs } from "./Components/ViewLinksBreadcrumbs";
import { useViewContext } from "./constants";
import CollectionsDropdown from "./forms/inputs/CollectionsDropdown";
import { LinksContextProvider, useLinksContext } from "./LinksContext";

export const LinksContent = () => {
  return (
    <LinksContextProvider>
      <CollectionsDropdown />
      <LinkPropertiesForm />
    </LinksContextProvider>
  );
};

const LinkPropertiesForm = () => {
  const { isAdminUser } = useViewContext();
  const disabled = !isAdminUser;
  const { currentField } = useLinksContext();
  if (!currentField) {
    return null;
  }
  return (
    <>
      <ViewLinksBreadcrumbs />
      <FieldView disabled={disabled} />
    </>
  );
};

export default LinkPropertiesForm;
