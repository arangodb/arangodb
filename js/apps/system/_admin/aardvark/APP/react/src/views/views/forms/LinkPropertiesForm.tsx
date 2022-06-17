import React, { useContext } from "react";
import { get } from "lodash";
import LinkView from "../Components/LinkView";
import FieldView from "../Components/FieldView";
import { Route, Switch, useRouteMatch } from "react-router-dom";
import { FormState, ViewContext, ViewProps } from "../constants";
import NewField from "../Components/NewField";

const LinkPropertiesForm = ({ name }: ViewProps) => {
  const { formState: fs, isAdminUser } = useContext(ViewContext);
  const match = useRouteMatch();

  const formState = fs as FormState;
  const link = get(match.params, 'link');
  const disabled = !isAdminUser;

  return <div
    id={'modal-dialog'}
    className={'createModalDialog'}
    tabIndex={-1}
    role={'dialog'}
    aria-labelledby={'myModalLabel'}
    aria-hidden={'true'}
  >
    <div className="modal-body" style={{ overflowY: 'visible' }}>
      <div className={'tab-content'}>
        <div className="tab-pane tab-pane-modal active" id="Links">
          <main>
            <Switch>
              <Route path={`${match.path}/:field*/_add`}>
                {isAdminUser ? <NewField/> : null}
              </Route>
              <Route path={`${match.path}/:field+`}>
                <FieldView disabled={disabled} name={name}/>
              </Route>
              <Route exact path={`${match.path}`}>
                <LinkView link={link} links={formState.links} disabled={disabled} name={name}/>
              </Route>
            </Switch>
          </main>
        </div>
      </div>
    </div>
  </div>;
};

export default LinkPropertiesForm;
