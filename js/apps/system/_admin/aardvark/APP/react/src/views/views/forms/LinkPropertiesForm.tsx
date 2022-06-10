import React, { useContext } from "react";
import { get } from "lodash";
import LinkView from "../Components/LinkView";
import FieldView from "../Components/FieldView";
import { Route, Switch, useRouteMatch } from "react-router-dom";
import { NavButton, SaveButton } from "../Actions";
import { FormState, ViewContext, ViewProps } from "../constants";

const LinkPropertiesForm = ({ name }: ViewProps) => {
  const { formState: fs, isAdminUser, changed, setChanged } = useContext(ViewContext);
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
              <Route path={`${match.path}/:field+/_add`}>
                <FieldView disabled={disabled}/>
              </Route>
              <Route path={`${match.path}/:field+`}>
                <FieldView disabled={disabled}/>
              </Route>
              <Route exact path={`${match.path}`}>
                <LinkView link={link} links={formState.links} disabled={disabled}/>
              </Route>
            </Switch>
          </main>
        </div>
      </div>
    </div>
    <div className="modal-footer">
      <NavButton/>
      {
        isAdminUser && changed
          ? <SaveButton view={formState as FormState} setChanged={setChanged} oldName={name}/>
          : null
      }
    </div>
  </div>;
};

export default LinkPropertiesForm;
