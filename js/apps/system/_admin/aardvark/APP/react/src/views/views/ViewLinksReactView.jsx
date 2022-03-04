import { cloneDeep, isEmpty } from "lodash";
import React, { useEffect, useReducer, useRef, useState } from "react";
import {
  getReducer,
  isAdminUser as userIsAdmin,
  usePermissions
} from "../../utils/helpers";
import { SaveButton, BackButton } from "./Actions";
import LinkPropertiesForm from "./forms/LinkPropertiesForm";
import { buildSubNav, postProcessor, useView, useCollection } from "./helpers";
import { ArangoTable, ArangoTH, ArangoTD } from "../../components/arango/table";
import LinkList from "./Components/LinkList";
import NewList from "./Components/NewLink";

const ViewLinksReactView = ({ name }) => {
  const initialState = useRef({
    formState: { name },
    formCache: { name }
  });
  const [state, dispatch] = useReducer(
    getReducer(initialState.current, postProcessor),
    initialState.current
  );
  const view = useView(name);
  const links = view.links;
  const permissions = usePermissions();
  const [isAdminUser, setIsAdminUser] = useState(false);

  useEffect(() => {
    initialState.current.formCache = cloneDeep(view);

    dispatch({
      type: "setFormState",
      formState: view
    });
  }, [view, name]);

  useEffect(() => {
    const observer = buildSubNav(isAdminUser, name, "Links");

    return () => observer.disconnect();
  }, [isAdminUser, name]);

  const tempIsAdminUser = userIsAdmin(permissions);
  if (tempIsAdminUser !== isAdminUser) {
    // Prevents an infinite render loop.
    setIsAdminUser(tempIsAdminUser);
  }

  const formState = state.formState;

  const [viewLink, setViewLink] = useState(false);
  const [icon, setIcon] = useState("fa-plus-circle");
  const [newLink, setNewLink] = useState(false);

  const handleNewLinkClick = e => {
    e.preventDefault();
    setNewLink(true);
    setViewLink(true);
  };

  const handleBackClick = e => {
    e.preventDefault();
    setViewLink(false);
  };

  const handleViewLink = link => {
    setViewLink(true);
    setNewLink(false);
  };

  return (
    <div className={"centralContent"} id={"content"}>
      {!viewLink && (
        <LinkList
          links={links}
          addClick={handleNewLinkClick}
          viewLink={handleViewLink}
          icon={icon}
        />
      )}
      {viewLink && (
        <div
          id={"modal-dialog"}
          className={"createModalDialog"}
          tabIndex={-1}
          role={"dialog"}
          aria-labelledby={"myModalLabel"}
          aria-hidden={"true"}
        >
          <div className="modal-body" style={{ overflowY: "visible" }}>
            <div className={"tab-content"}>
              <div className="tab-pane tab-pane-modal active" id="Links">
                <LinkPropertiesForm
                  formState={formState}
                  dispatch={dispatch}
                  disabled={!isAdminUser}
                  view={name}
                  addNew={newLink}
                />
              </div>
            </div>
          </div>
          <div className="modal-footer">
            <BackButton buttonClick={handleBackClick} />
            <SaveButton view={formState} oldName={name} />
          </div>
        </div>
      )}
    </div>
  );
};

window.ViewLinksReactView = ViewLinksReactView;
