import React from "react";
import { Route, Switch } from "react-router-dom";
import CollectionsDropdown from "./forms/inputs/CollectionsDropdown";
import LinkPropertiesForm from "./forms/LinkPropertiesForm";

export const LinksContent = ({ name }: { name: string }) => {
  return (
    <div>
      <Switch>
        <Route path={"/:link"}>
          <CollectionsDropdown />
          <LinkPropertiesForm name={name} />
        </Route>
        <Route exact path={"/"}>
          <CollectionsDropdown />
        </Route>
      </Switch>
    </div>
  );
};
