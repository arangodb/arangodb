import { isEqual, sortBy } from 'lodash';
import React, { useEffect, useState } from 'react';
import useSWR from 'swr';
import { ArangoTable, ArangoTD, ArangoTH } from '../../components/arango/table';
import Modal, { ModalBody, ModalFooter, ModalHeader } from '../../components/modal/Modal';
import { Cell, Grid } from '../../components/pure-css/grid';
import { getApiRouteForCurrentDB } from '../../utils/arangoClient';
import { facetedFilter, getChangeHandler, usePermissions } from '../../utils/helpers';
import Actions from './Actions';
import AddView from './AddView';

const FilterHelpModal = () => {
  const [show, setShow] = useState(false);

  const showFilterHelp = (event) => {
    event.preventDefault();
    setShow(true);
  };

  return <>
    <a href={'#views'} onClick={showFilterHelp}>
      <i className={'fa fa-question-circle'} style={{
        marginTop: 5,
        color: 'rgb(85, 85, 85)',
        fontSize: '18px'
      }}/>
    </a>
    <Modal show={show} setShow={setShow} cid={'modal-content-filter-help'}>
      <ModalHeader title={'Filter Help'}/>
      <ModalBody>
        <dl>
          <dt>Simple Usage</dt>
          <dd>
            Just type a few characters and the filter will show all views which include the
            search string in their name or type.
          </dd>
          <dt>Pattern Search</dt>
          <dd>
            Enter a&nbsp;<b>
            <a target={'_blank'} rel={'noreferrer'}
               href={'https://www.gnu.org/software/bash/manual/html_node/Pattern-Matching.html'}>
              glob
            </a>
          </b>&nbsp;pattern and the filter will show all views which match the pattern in their
            name or type. Note that the pattern is not anchored, and can match anywhere in a string.
          </dd>
          <dt>Faceted Pattern Search</dt>
          <dd>
            Prefix the search pattern
            with <code>name:</code> or <code>type:</code> to restrict the pattern to that respective
            field. Multiple field-pattern combos can be entered, separated by a space.
          </dd>
        </dl>
      </ModalBody>
      <ModalFooter>
        <button className="button-close" onClick={() => setShow(false)}>Close</button>
      </ModalFooter>
    </Modal>
  </>;
};

const facets = ['name', 'type'];

const ViewsReactView = () => {
  const { data } = useSWR('/view', (path) => getApiRouteForCurrentDB().get(path));
  const { data: permData } = usePermissions();

  const [filterExpr, setFilterExpr] = useState('');
  const [filteredViews, setFilteredViews] = useState([]);
  const [views, setViews] = useState([]);

  useEffect(() => {
    const filteredViews = facetedFilter(filterExpr, views, facets);

    setFilteredViews(sortBy(filteredViews, ['type', 'name']));
  }, [views, filterExpr]);

  if (data && permData) {
    const permission = permData.body.result;

    if (!isEqual(data.body.result, views)) {
      setViews(data.body.result);
    }

    return <>
      <div className="headerBar">
        <div className="search-field">
          <input type={'text'} id={'filterInput'} className={'search-input'} value={filterExpr}
                 onChange={getChangeHandler(setFilterExpr)} placeholder={'Filter...'}/>
          <i id="searchSubmit" className="fa fa-search"/>
        </div>
        <div className="headerButtonBar">
          <ul className="headerButtonList">
            <li className="enabled">
              <FilterHelpModal/>
            </li>
          </ul>
        </div>
      </div>

      <div className={'contentDiv'} id={'viewsContent'} style={{ paddingTop: 0 }}>
        <Grid>
          <Cell size={'1'}>
            <div className={'sectionHeader'}>
              <div className={'title'}><AddView views={views}/></div>
            </div>
            <ArangoTable>
              <thead>
              <tr>
                <ArangoTH seq={0}>Name</ArangoTH>
                <ArangoTH seq={1}>Type</ArangoTH>
                <ArangoTH seq={2}>Actions</ArangoTH>
              </tr>
              </thead>
              <tbody>
              {
                filteredViews.length
                  ? filteredViews.map((view, idx) => (
                    <tr key={view.name}>
                      <ArangoTD seq={0}>{view.name}</ArangoTD>
                      <ArangoTD seq={1}>{view.type}</ArangoTD>
                      <ArangoTD seq={2}>
                        <Actions view={view} permission={permission} modalCidSuffix={idx}/>
                      </ArangoTD>
                    </tr>
                  ))
                  : <tr>
                    <ArangoTD seq={0} colSpan={3}>No views found.</ArangoTD>
                  </tr>
              }
              </tbody>
            </ArangoTable>
          </Cell>
        </Grid>
      </div>
    </>;
  }

  return <h1>Views</h1>;
};

window.ViewsReactView = ViewsReactView;
