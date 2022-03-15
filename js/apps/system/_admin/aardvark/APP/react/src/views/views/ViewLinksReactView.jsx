import { cloneDeep, isEmpty } from "lodash";
import React, { useEffect, useReducer, useRef, useState } from "react";
import { LinkProvider } from "./Contexts/LinkContext";
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
import { useShow, useShowUpdate } from "./Contexts/LinkContext";
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
  const show = useShow();
  const updateShow = useShowUpdate();

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
  const handleNewLinkClick = e => {
    e.preventDefault();
    updateShow("AddNew");
  };

  const handleBackClick = e => {
    e.preventDefault();
    updateShow("LinkList");
  };

  const handleViewLink = link => {
    updateShow("ViewParent");
  };

  return (
    <div className={"centralContent"} id={"content"}>
      {show === "LinkList" && (
        <LinkList
          links={links}
          addClick={handleNewLinkClick}
          viewLink={handleViewLink}
          icon={icon}
        />
      )}
      {show !== "LinkList" && (
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
                  show={show}
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

const ViewLinkWrapper = ({ name }) => {
  return (
    <LinkProvider>
      <ViewLinksReactView name={name} />
    </LinkProvider>
  );
};
window.ViewLinksReactView = ViewLinkWrapper;
