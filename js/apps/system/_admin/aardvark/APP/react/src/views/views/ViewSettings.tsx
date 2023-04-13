import { Box } from "@chakra-ui/react";
import { cloneDeep, isEqual, uniqueId } from "lodash";
import React, { useEffect, useReducer, useRef, useState } from "react";
import { HashRouter } from "react-router-dom";
import useSWR from "swr";
import { getApiRouteForCurrentDB } from "../../utils/arangoClient";
import { FormDispatch, State } from "../../utils/constants";
import { encodeHelper } from "../../utils/encodeHelper";
import { getReducer } from "../../utils/helpers";
import { usePermissions, userIsAdmin } from "../../utils/usePermissions";
import { FormState, ViewContext } from "./constants";
import { postProcessor, useView } from "./helpers";
import { LinksContextProvider } from "./LinksContext";
import { ViewHeader } from "./ViewHeader";
import { ViewSection } from "./ViewSection";

export const ViewSettings = ({ name }: { name: string }) => {
  // if we try to fix this by using inital values for id, type,
  // it causes the navigation to break
  const initFormState = { name } as any;

  const initialState = useRef<State<FormState>>({
    formState: initFormState,
    formCache: initFormState,
    renderKey: uniqueId("force_re-render_"),
    // these might be unnecessary fields, unused here, but they cause type issues
    // TODO: type needs to be fixed
    show: true,
    showJsonForm: true,
    lockJsonForm: false
  });
  // workaround, because useView only exposes partial FormState by default
  const view = useView(name) as any as FormState;
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

  const updateName = (name: string) => {
    dispatch({
      type: "setField",
      field: {
        path: "name",
        value: name
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
  if (data) {
    if (!isEqual(data.body.result, views)) {
      setViews(data.body.result);
    }
  }
  const { encoded: encodedName } = encodeHelper(name);
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
      <HashRouter basename={`view/${encodedName}`} hashType={"noslash"}>
        <ViewSettingsInner
          formState={formState}
          updateName={updateName}
          isAdminUser={isAdminUser}
          views={views}
          dispatch={dispatch}
          changed={changed}
          name={name}
          setChanged={setChanged}
          state={state}
        />
      </HashRouter>
    </ViewContext.Provider>
  );
};

const ViewSettingsInner = ({
  formState,
  updateName,
  isAdminUser,
  views,
  dispatch,
  changed,
  name,
  setChanged,
  state
}: {
  formState: FormState;
  updateName: (name: string) => void;
  isAdminUser: boolean;
  views: never[];
  dispatch: FormDispatch<FormState>;
  changed: boolean;
  name: string;
  setChanged: React.Dispatch<React.SetStateAction<boolean>>;
  state: State<FormState>;
}) => {
  return (
    <LinksContextProvider>
      <Box
        height="calc(100vh - 60px)"
        display="grid"
        gridTemplateRows="120px 1fr"
        width="full"
      >
        <ViewHeader
          formState={formState}
          updateName={updateName}
          isAdminUser={isAdminUser}
          views={views}
          dispatch={dispatch}
          changed={changed}
          name={name}
          setChanged={setChanged}
        />
        <ViewSection
          formState={formState}
          dispatch={dispatch}
          isAdminUser={isAdminUser}
          state={state}
        />
      </Box>
    </LinksContextProvider>
  );
};
