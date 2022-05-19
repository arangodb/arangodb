import { chain, cloneDeep, difference, isNull, map } from 'lodash';
import React, { useEffect, useReducer, useRef, useState } from 'react';
import { HashRouter, Route, Switch } from 'react-router-dom';
import useSWR from 'swr';
import { getApiRouteForCurrentDB } from '../../utils/arangoClient';
import { getReducer, isAdminUser as userIsAdmin, usePermissions } from '../../utils/helpers';
import LinkList from './Components/LinkList';
import NewLink from './Components/NewLink';
import LinkPropertiesForm from './forms/LinkPropertiesForm';
import { buildSubNav, postProcessor, useLinkState, useView } from './helpers';
import { ViewContext } from './constants';

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
  const [collection, setCollection, addDisabled, links] = useLinkState(
    state.formState,
    "links"
  );
  const { data } = useSWR(["/collection", "excludeSystem=true"], (path, qs) =>
    getApiRouteForCurrentDB().get(path, qs)
  );
  const [options, setOptions] = useState([]);

  useEffect(() => {
    if (data) {
      const linkKeys = chain(links)
        .omitBy(isNull)
        .keys()
        .value();
      const collNames = map(data.body.result, "name");
      const tempOptions = difference(collNames, linkKeys).sort();

      setOptions(tempOptions);
    }
  }, [data, links]);

  useEffect(() => {
    initialState.current.formCache = cloneDeep(view);

    dispatch({
      type: 'setFormState',
      formState: view
    });
  }, [view]);

  useEffect(() => {
    const observer = buildSubNav(isAdminUser, name, 'Links');

    return () => observer.disconnect();
  }, [isAdminUser, name]);

  const tempIsAdminUser = userIsAdmin(permissions);
  if (tempIsAdminUser !== isAdminUser) {
    // Prevents an infinite render loop.
    setIsAdminUser(tempIsAdminUser);
  }

  const formState = state.formState;

  return <ViewContext.Provider
    value={{
      formState,
      dispatch,
      isAdminUser
    }}
  >
    <div className={'centralContent'} id={'content'}>
      <HashRouter basename={`view/${name}/links/`} hashType={'noslash'}>
        <Switch>
          <Route exact path={'/'}>
            <LinkList/>
          </Route>
          <Route exact path={'/_add'}>
             {
              isAdminUser
                ? <NewLink
                  disabled={addDisabled || !options.includes(collection)}
                  collection={collection}
                  updateCollection={setCollection}
                  options={options}
                />
                : null
             }
          </Route>
          <Route path={'/:link'}>
            <LinkPropertiesForm disabled={!isAdminUser}/>
          </Route>
        </Switch>
      </HashRouter>
    </div>
  </ViewContext.Provider>;
};

window.ViewLinksReactView = ViewLinksReactView;
