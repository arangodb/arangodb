import { cloneDeep, noop } from "lodash";
import React, {
  createContext,
  useEffect,
  useReducer,
  useRef,
  useState
} from "react";
import {
  getReducer,
  isAdminUser as userIsAdmin,
  usePermissions
} from "../../utils/helpers";
import { BackButton, SaveButton } from "./Actions";
import LinkList from "./Components/LinkList";
import LinkPropertiesForm from "./forms/LinkPropertiesForm";
import { buildSubNav, postProcessor, useView } from "./helpers";
export const ViewContext = createContext({
  show: "",
  setShow: noop,
  formState: {},
  dispatch: noop,
  field: { field: "", basePath: "" },
  setField: noop,
  link: "",
  setNewLink: noop,
  newLink: "",
  setParentLink: noop
});

export const usePreviouse = value => {
  const ref = useRef();
  useEffect(() => {
    ref.current = value;
  }, [value]);
  return ref.value;
};

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
  const [show, setShow] = useState("LinkList");
  const [field, setField] = useState({});
  const [newLink, setNewLink] = useState("");
  const [link, setParentLink] = useState("");

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

  const removeLink = l => {
    dispatch({
      type: "setField",
      field: {
        path: `links[${l}]`,
        value: null
      }
    });
  };

  const handleView = l => {
    setParentLink(l);
    setShow("ViewParent");
  };
  const handleBackClick = e => {
    e.preventDefault();
    setShow("LinkList");
  };

  return (
    <ViewContext.Provider
      value={{
        show,
        setShow,
        formState,
        dispatch,
        field,
        setField,
        link,
        setNewLink,
        newLink,
        setParentLink
      }}
    >
      <div className={"centralContent"} id={"content"}>
        {show === "LinkList" ? (
          <LinkList
            links={links}
            addClick={() => setShow("AddNew")}
            viewLink={handleView}
            icon={"fa-plus-circle"}
          />
        ) : (
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
                    disabled={!isAdminUser}
                    view={name}
                  />
                </div>
              </div>
            </div>
            <div className="modal-footer">
              <BackButton disabled={newLink} buttonClick={handleBackClick} />
              <SaveButton view={formState} oldName={name} />
            </div>
          </div>
        )}
      </div>
    </ViewContext.Provider>
  );
};

window.ViewLinksReactView = ViewLinksReactView;
