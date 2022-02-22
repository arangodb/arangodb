import { cloneDeep } from "lodash";
import React, { useEffect, useReducer, useRef, useState } from "react";
import {
  getReducer,
  isAdminUser as userIsAdmin,
  usePermissions
} from "../../utils/helpers";
import { SaveButton } from "./Actions";
import LinkPropertiesForm from "./forms/LinkPropertiesForm";
import { buildSubNav, postProcessor, useView, useCollection } from "./helpers";
import { ArangoTable, ArangoTH, ArangoTD } from "../../components/arango/table";
import CollectionList from "./Components/LinkList";

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
  const permissions = usePermissions();
  const [isAdminUser, setIsAdminUser] = useState(false);

  useEffect(() => {
    initialState.current.formCache = cloneDeep(view);

    dispatch({
      type: "setFormState",
      formState: view
    });
  }, [view, name]);

  const collections = useCollection();

  useEffect(() => {
    console.log(collections);
  }, [collections]);

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

  const mockData = [
    {
      id: 0,
      name: "fooBar",
      attr: "fooBar, foo",
      desc: "This is fooBar",
      action: "This is an action"
    },
    {
      id: 1,
      name: "airPort",
      attr: "Foo",
      desc: "This is fooBar",
      action: "This is an action"
    },
    {
      id: 2,
      name: "route",
      attr: "fooBar, foo",
      desc: "This is fooBar",
      action: "This is an action"
    }
  ];

  return (
    <div className={"centralContent"} id={"content"}>
      <CollectionList />
      <ArangoTable className={"edit-index-table arango-table"}>
        <thead>
          <tr class="figuresHeader">
            <ArangoTH>ID</ArangoTH>
            <ArangoTH>Attributes</ArangoTH>
            <ArangoTH>Name</ArangoTH>
            <ArangoTH>Options</ArangoTH>
            <ArangoTH>Action</ArangoTH>
          </tr>
        </thead>

        <tbody>
          {mockData.map((c, key) => (
            <tr>
              <ArangoTD key={key}>{key}</ArangoTD>
              <ArangoTD key={key}>{c.attr}</ArangoTD>
              <ArangoTD key={key}>{c.name}</ArangoTD>
              <ArangoTD key={key}>{c.desc}</ArangoTD>
              <ArangoTD key={key}>{c.action}</ArangoTD>
            </tr>
          ))}
        </tbody>
        <tfoot>
          <tr>
            <ArangoTD> </ArangoTD>
            <ArangoTD> </ArangoTD>
            <ArangoTD> </ArangoTD>
            <ArangoTD> </ArangoTD>
            <ArangoTD>
              <i className="fa fa-plus-circle"></i>
            </ArangoTD>
          </tr>
        </tfoot>
      </ArangoTable>
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
              />
            </div>
          </div>
        </div>
        <div className="modal-footer">
          <SaveButton view={formState} oldName={name} />
        </div>
      </div>
    </div>
  );
};

window.ViewLinksReactView = ViewLinksReactView;
