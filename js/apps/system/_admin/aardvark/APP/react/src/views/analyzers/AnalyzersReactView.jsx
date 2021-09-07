import { isEqual, sortBy } from 'lodash';
import minimatch from 'minimatch';
import React, { useCallback, useEffect, useState } from 'react';
import useSWR from 'swr';
import { ArangoTable, ArangoTD, ArangoTH } from '../../components/arango/table';
import Modal, { ModalBody, ModalFooter, ModalHeader } from '../../components/modal/Modal';
import { Cell, Grid } from '../../components/pure-css/grid';
import { getApiRouteForCurrentDB } from '../../utils/arangoClient';
import { getChangeHandler, usePermissions } from '../../utils/helpers';
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
        float: 'right',
        marginRight: 10,
        marginTop: 3,
        color: '#fff',
        fontSize: '16pt'
      }}/>
    </a>
    <Modal show={show} setShow={setShow}>
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

const AnalyzersReactView = () => {
  const { data } = useSWR('/analyzer', (path) => getApiRouteForCurrentDB().get(path));
  const { data: permData } = usePermissions();

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
    let tempFilteredAnalyzers = analyzers;

    if (filterExpr) {
      try {
        const filters = filterExpr.trim().split(/\s+/);

        for (const filter of filters) {
          const splitIndex = filter.indexOf(':');
          const field = filter.slice(0, splitIndex);
          const pattern = filter.slice(splitIndex + 1);

          tempFilteredAnalyzers = tempFilteredAnalyzers.filter(
            analyzer => minimatch(analyzer[field].toLowerCase(), `*${pattern.toLowerCase()}*`));
        }
      } catch (e) {
        tempFilteredAnalyzers = tempFilteredAnalyzers.filter(analyzer => {
          const normalizedPattern = `*${filterExpr.toLowerCase()}*`;

          return ['db', 'name', 'type'].some(
            field => minimatch(analyzer[field].toLowerCase(), normalizedPattern));
        });
      }
    }

    processAndSetFilteredAnalyzers(tempFilteredAnalyzers);
  }, [analyzers, filterExpr, processAndSetFilteredAnalyzers]);

  if (data && permData) {
    const permission = permData.body.result;

    if (!isEqual(data.body.result, analyzers)) {
      setAnalyzers(data.body.result);
      processAndSetFilteredAnalyzers(data.body.result);
    }

    return <div className={'innerContent'} id={'analyzersContent'} style={{ paddingTop: 0 }}>
      <Grid>
        <Cell size={'1'}>
          <Grid className={'sectionHeader'}>
            <Cell size={'2-5'}>
              <div className={'title'}><AddAnalyzer analyzers={analyzers}/></div>
            </Cell>

            <Cell size={'3-5'}>
              <FilterHelpModal/>
              <label htmlFor={'filter-input'} style={{
                color: '#fff',
                marginRight: 10,
                float: 'right'
              }}>
                Filter: <input type={'text'} id={'filter-input'} className={'search-input'}
                               value={filterExpr} onChange={getChangeHandler(setFilterExpr)}
                               placeholder={'<glob>|(<db|name|type>:<glob> )+'}
                               style={{
                                 margin: 0,
                                 width: 300,
                                 paddingLeft: 25
                               }}/>
                <i className={'fa fa-filter'} style={{
                  position: 'relative',
                  float: 'left',
                  top: 9,
                  left: 60,
                  cursor: 'default',
                  color: 'rgb(85, 85, 85)'
                }}/>
              </label>
              <label htmlFor={'inbuilt-analyzers'} className="pure-checkbox" style={{
                float: 'right',
                color: '#fff',
                marginTop: 3
              }}>
                <input id={'inbuilt-analyzers'} type={'checkbox'} checked={showInbuiltAnalyzers}
                       onChange={toggleInbuiltAnalyzers} style={{
                  width: 'auto',
                  marginBottom: 7
                }}/> Show In-built Analyzers
              </label>
            </Cell>
          </Grid>
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
                ? filteredAnalyzers.map(analyzer => (
                  <tr key={analyzer.name}>
                    <ArangoTD seq={0}>{analyzer.db}</ArangoTD>
                    <ArangoTD seq={1}>{analyzer.name}</ArangoTD>
                    <ArangoTD seq={2}>{typeNameMap[analyzer.type]}</ArangoTD>
                    <ArangoTD seq={3}>
                      <Actions analyzer={analyzer} permission={permission}/>
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
    </div>;
  }

  return <h1>Analyzers</h1>;
};

window.AnalyzersReactView = AnalyzersReactView;
