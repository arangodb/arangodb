import { Box } from "@chakra-ui/react";
import { cloneDeep, isEqual, uniqueId } from "lodash";
import React, { useEffect, useReducer, useRef, useState } from "react";
import { HashRouter } from "react-router-dom";
import useSWR from "swr";
import { getApiRouteForCurrentDB } from "../../utils/arangoClient";
import { State } from "../../utils/constants";
import {
  getReducer,
  isAdminUser as userIsAdmin,
  usePermissions
} from "../../utils/helpers";
import { useDisableNavBar } from "../../utils/useDisableNavBar";
import { useGlobalStyleReset } from "../../utils/useGlobalStyleReset";
import { FormState, ViewContext } from "./constants";
import { postProcessor, useView } from "./helpers";
import "./split-pane-styles.css";
import { ViewHeader } from "./ViewHeader";
import { ViewSection } from "./ViewSection";
import "./viewsheader.css";

export const ViewSettings = ({
  name,
  isCluster
}: {
  name: string;
  isCluster: boolean;
}) => {
  useDisableNavBar();
  useGlobalStyleReset();
  const [editName, setEditName] = useState(false);

  const handleEditName = () => {
    setEditName(true);
  };

  const closeEditName = () => {
    setEditName(false);
  };

  const initialState = useRef<State<FormState>>({
    formState: { name, id: "", type: "arangosearch" },
    formCache: { name },
    renderKey: uniqueId("force_re-render_"),
    show: true,
    showJsonForm: true,
    lockJsonForm: false
  });
  // workaround, because useView only exposes partial FormState by default
  const view = (useView(name) as any) as FormState;
  const [changed, setChanged] = useState(
    !!window.sessionStorage.getItem(`${name}-changed`)
  );
  const [state, dispatch] = useReducer(
    getReducer(initialState.current, postProcessor, setChanged, name),
    initialState.current
  );
  const permissions = usePermissions();
  const [isAdminUser, setIsAdminUser] = useState(false);
  const { data } = useSWR(isAdminUser ? "/view" : null, path =>
    getApiRouteForCurrentDB().get(path)
  );
  const [views, setViews] = useState([]);

  useEffect(() => {
    initialState.current.formCache = cloneDeep(view);

    dispatch({
      type: "initFormState",
      formState: view
    });
    dispatch({
      type: "regenRenderKey"
    });
  }, [view, name]);

  const updateName = (event: React.ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: "setField",
      field: {
        path: "name",
        value: event.target && event.target.value
      }
    });
  };

  useEffect(() => {
    const tempIsAdminUser = userIsAdmin(permissions);
    if (tempIsAdminUser !== isAdminUser) {
      setIsAdminUser(tempIsAdminUser);
      dispatch({
        type: "regenRenderKey"
      });
    }
  }, [isAdminUser, permissions]);

  const formState = state.formState;
  const nameEditDisabled = isCluster || !isAdminUser;
  if (data) {
    if (!isEqual(data.body.result, views)) {
      setViews(data.body.result);
    }
  }
  return (
    <ViewContext.Provider
      value={{
        formState,
        dispatch,
        isAdminUser,
        changed,
        setChanged
      }}
    >
      <HashRouter basename={`view/${name}`} hashType={"noslash"}>
        <Box backgroundColor="white">
          <ViewHeader
            editName={editName}
            formState={formState}
            handleEditName={handleEditName}
            updateName={updateName}
            nameEditDisabled={nameEditDisabled}
            closeEditName={closeEditName}
            isAdminUser={isAdminUser}
            views={views}
            dispatch={dispatch}
            changed={changed}
            name={name}
            setChanged={setChanged}
          />
          <ViewSection
            name={name}
            formState={formState}
            dispatch={dispatch}
            isAdminUser={isAdminUser}
            state={state}
          />
        </Box>
      </HashRouter>
    </ViewContext.Provider>
  );
};
