import React, { useContext } from "react";
import FieldView from "./Components/FieldView";
import { ViewLinksBreadcrumbs } from "./Components/ViewLinksBreadcrumbs";
import { ViewContext } from "./constants";

import CollectionsDropdown from "./forms/inputs/CollectionsDropdown";
import { useLinksContext } from "./LinksContext";

export const LinksContent = () => {
  return (
    <>
      <CollectionsDropdown />
      <LinkPropertiesForm />
    </>
  );
};

const LinkPropertiesForm = () => {
  const { isAdminUser } = useContext(ViewContext);
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
