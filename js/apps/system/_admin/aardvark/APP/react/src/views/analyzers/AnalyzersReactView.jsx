/* global $ */

import { isEqual, map, sortBy } from 'lodash';
import React, { useCallback, useEffect, useState } from 'react';
import useSWR from 'swr';
import { ArangoTable, ArangoTD, ArangoTH } from '../../components/arango/table';
import Modal, { ModalBody, ModalFooter, ModalHeader } from '../../components/modal/Modal';
import { Cell, Grid } from '../../components/pure-css/grid';
import { getApiRouteForCurrentDB } from '../../utils/arangoClient';
import { facetedFilter, getChangeHandler, isAdminUser, usePermissions } from '../../utils/helpers';
import Actions from './Actions';
import AddAnalyzer from './AddAnalyzer';
import { typeNameMap } from './constants';

const FilterHelpModal = () => {
  const [show, setShow] = useState(false);

  const showFilterHelp = (event) => {
    event.preventDefault();
    setShow(true);
  };

  return <>
    <a href={'#analyzers'} onClick={showFilterHelp}>
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
            Just type a few characters and the filter will show all analyzers which include the
            search string in their database, name or type.
          </dd>
          <dt>Pattern Search</dt>
          <dd>
            Enter a&nbsp;<b>
            <a target={'_blank'} rel={'noreferrer'}
               href={'https://www.gnu.org/software/bash/manual/html_node/Pattern-Matching.html'}>
              glob
            </a>
          </b>&nbsp;pattern and the filter will show all analyzers which match the pattern in their
            database, name or type. Note that the pattern is not anchored, and can match anywhere in
            a string.
          </dd>
          <dt>Faceted Pattern Search</dt>
          <dd>
            Prefix the search pattern
            with <code>db:</code>, <code>name:</code> or <code>type:</code> to restrict the pattern
            to that respective field. Multiple field-pattern combos can be entered, separated by a
            space.
          </dd>
        </dl>
      </ModalBody>
      <ModalFooter>
        <button className="button-close" onClick={() => setShow(false)}>Close</button>
      </ModalFooter>
    </Modal>
  </>;
};

const toggleHeaderDropdown = () => {
  $('#analyzersToggle').toggleClass('activated');
  $('#analyzersDropdown2').slideToggle(200);
};

const facets = ['db', 'name', 'type'];

const AnalyzersReactView = () => {
  const { data } = useSWR('/analyzer', (path) => getApiRouteForCurrentDB().get(path));
  const permissions = usePermissions();

  const [filterExpr, setFilterExpr] = useState('');
  const [filteredAnalyzers, setFilteredAnalyzers] = useState([]);
  const [analyzers, setAnalyzers] = useState([]);
  const [showInbuiltAnalyzers, setShowInbuiltAnalyzers] = useState(false);

  const processAndSetFilteredAnalyzers = useCallback(analyzers => {
    analyzers.forEach(analyzer => {
      analyzer.db = analyzer.name.includes('::') ? analyzer.name.split('::')[0] : '_system';
    });

    if (!showInbuiltAnalyzers) {
      analyzers = analyzers.filter(analyzer => analyzer.name.includes('::'));
    }

    setFilteredAnalyzers(sortBy(analyzers, ['db', 'type', 'name']));
  }, [showInbuiltAnalyzers]);

  const toggleInbuiltAnalyzers = () => {
    setShowInbuiltAnalyzers(!showInbuiltAnalyzers);
  };

  useEffect(() => {
    processAndSetFilteredAnalyzers(facetedFilter(filterExpr, analyzers, facets));
  }, [analyzers, filterExpr, processAndSetFilteredAnalyzers]);

  if (data) {
    if (!isEqual(map(data.body.result, 'name'), map(analyzers, 'name'))) {
      setAnalyzers(data.body.result);
      processAndSetFilteredAnalyzers(data.body.result);
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
            <li className="enabled">
              <a id="analyzersToggle" className="headerButton" href={'#analyzers'}
                 onClick={toggleHeaderDropdown}>
                <span className="icon_arangodb_settings2" title="Settings"/>
              </a>
            </li>
          </ul>
        </div>
      </div>

      <div id="analyzersDropdown2" className="headerDropdown">
        <div id="analyzersDropdown" className="dropdownInner">
          <ul>
            <li className="nav-header">System</li>
            <li>
              <a href={'#analyzers'}>
                <label className="checkbox checkboxLabel">
                  <input className="css-checkbox" type="checkbox"
                         onChange={toggleInbuiltAnalyzers}/>
                  <i
                    className={`fa ${showInbuiltAnalyzers ? 'fa-check-square-o' : 'fa-square-o'}`}/>
                  Built-in
                </label>
              </a>
            </li>
          </ul>
        </div>
      </div>

      <div className={'contentDiv'} id={'analyzersContent'} style={{ paddingTop: 0 }}>
        <Grid>
          <Cell size={'1'}>
            {
              isAdminUser(permissions)
                ? <div className={'sectionHeader'}>
                  <div className={'title'}><AddAnalyzer analyzers={analyzers}/></div>
                </div>
                : null
            }
            <ArangoTable className={'arango-table'}>
              <thead>
              <tr>
                <ArangoTH seq={0}>DB</ArangoTH>
                <ArangoTH seq={1}>Name</ArangoTH>
                <ArangoTH seq={2}>Type</ArangoTH>
                <ArangoTH seq={3}>Actions</ArangoTH>
              </tr>
              </thead>
              <tbody>
              {
                filteredAnalyzers.length
                  ? filteredAnalyzers.map((analyzer, idx) => (
                    <tr key={analyzer.name}>
                      <ArangoTD seq={0}>{analyzer.db}</ArangoTD>
                      <ArangoTD seq={1}>{analyzer.name}</ArangoTD>
                      <ArangoTD seq={2}>{typeNameMap[analyzer.type]}</ArangoTD>
                      <ArangoTD seq={3}>
                        <Actions analyzer={analyzer} permission={permissions} modalCidSuffix={idx}/>
                      </ArangoTD>
                    </tr>
                  ))
                  : <tr>
                    <ArangoTD seq={0} colSpan={4}>
                      No analyzers found.
                    </ArangoTD>
                  </tr>
              }
              </tbody>
            </ArangoTable>
          </Cell>
        </Grid>
      </div>
    </>;
  }

  return <h1>Analyzers</h1>;
};

window.AnalyzersReactView = AnalyzersReactView;
