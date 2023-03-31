import { get } from "lodash";
import React, { useContext } from "react";
import { Route, Switch, useRouteMatch } from "react-router-dom";
import FieldView from "../Components/FieldView";
import LinkView from "../Components/LinkView";
import { FormState, ViewContext, ViewProps } from "../constants";

const LinkPropertiesForm = ({ name }: ViewProps) => {
  const { formState: fs, isAdminUser } = useContext(ViewContext);
  const match = useRouteMatch();

  const formState = fs as FormState;
  const link = get(match.params, "link");
  const disabled = !isAdminUser;

  return (
    <Switch>
      <Route path={`${match.path}/:field+`}>
        <FieldView disabled={disabled} name={name} />
      </Route>
      <Route exact path={`${match.path}`}>
        <LinkView
          link={link}
          links={formState.links}
          disabled={disabled}
          name={name}
        />
      </Route>
    </Switch>
  );
};

export default LinkPropertiesForm;
