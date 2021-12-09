import React, { useEffect, useState } from 'react';
import { ArangoTable, ArangoTD, ArangoTH } from '../../components/arango/table';
import Checkbox from '../../components/pure-css/form/Checkbox';
import { isAdminUser as userIsAdmin, usePermissions } from '../../utils/helpers';
import { buildSubNav, useView } from './helpers';

const PrimarySortView = ({ primarySort = [] }) => {
  return primarySort.length
    ? <ArangoTable style={{ marginLeft: 0 }}>
      <thead>
      <tr>
        <ArangoTH seq={0}>Field</ArangoTH>
        <ArangoTH seq={1}>ASC</ArangoTH>
      </tr>
      </thead>
      <tbody>
      {
        primarySort.map(primarySortField =>
          <tr key={primarySortField.field}>
            <ArangoTD seq={0}>{primarySortField.field}</ArangoTD>
            <ArangoTD seq={1}>
              <Checkbox disabled checked={primarySortField.asc}/>
            </ArangoTD>
          </tr>
        )
      }
      </tbody>
    </ArangoTable>
    : '<Nothing Found>';
};

const StoredValuesView = ({ storedValues = [] }) => {
  return storedValues.length
    ? <ArangoTable style={{ marginLeft: 0 }}>
      <thead>
      <tr>
        <ArangoTH seq={0}>Fields</ArangoTH>
        <ArangoTH seq={1}>Compression</ArangoTH>
      </tr>
      </thead>
      <tbody>
      {
        storedValues.map((storedValue, idx) =>
          <tr key={idx}>
            <ArangoTD seq={0}>{storedValue.fields.join(', ')}</ArangoTD>
            <ArangoTD seq={1}>{storedValue.compression}</ArangoTD>
          </tr>
        )
      }
      </tbody>
    </ArangoTable>
    : '<Nothing Found>';
};

const ViewInfoReactView = ({ name }) => {
  const view = useView(name);
  const permissions = usePermissions();
  const [isAdminUser, setIsAdminUser] = useState(false);

  useEffect(() => {
    const observer = buildSubNav(isAdminUser, name, 'Info');

    return () => observer.disconnect();
  }, [isAdminUser, name]);

  const tempIsAdminUser = userIsAdmin(permissions);
  if (tempIsAdminUser !== isAdminUser) { // Prevents an infinite render loop.
    setIsAdminUser(tempIsAdminUser);
  }

  return <div className={'centralContent'} id={'content'}>
    <div id={'modal-dialog'} className={'createModalDialog'} tabIndex={-1} role={'dialog'}
         aria-labelledby={'myModalLabel'} aria-hidden={'true'}>
      <div className="modal-body">
        <div>
          <div style={{
            color: '#717d90',
            fontWeight: 600,
            fontSize: '12.5pt',
            padding: 10,
            borderBottom: '1px solid rgba(0, 0, 0, .3)'
          }}>
            Properties
          </div>
          <ArangoTable id={'viewInfoTable'}>
            <tbody>
            <tr>
              <th className="collectionInfoTh2">ID:</th>
              <th className="collectionInfoTh">
                <div className="modal-text">
                  {view.id}
                </div>
              </th>
              <th className="tooltipInfoTh"/>
            </tr>

            <tr>
              <th className="collectionInfoTh2">Globally Unique ID:</th>
              <th className="collectionInfoTh">
                <div className="modal-text">
                  {view.globallyUniqueId}
                </div>
              </th>
              <th className="tooltipInfoTh"/>
            </tr>

            <tr>
              <th className="collectionInfoTh2">Type:</th>
              <th className="collectionInfoTh">
                <div className="modal-text">
                  {view.type}
                </div>
              </th>
              <th className="tooltipInfoTh"/>
            </tr>

            <tr>
              <th className="collectionInfoTh2">Write Buffer Active:</th>
              <th className="collectionInfoTh">
                <div className="modal-text">
                  {view.writebufferActive}
                </div>
              </th>
              <th className="tooltipInfoTh">
                <div>
                <span rel="tooltip" className="modalTooltips arangoicon icon_arangodb_info"
                      title="Maximum number of concurrent active writers (segments) that perform a transaction.">
                </span>
                </div>
              </th>
            </tr>

            <tr>
              <th className="collectionInfoTh2">Write Buffer Idle:</th>
              <th className="collectionInfoTh">
                <div className="modal-text">
                  {view.writebufferIdle}
                </div>
              </th>
              <th className="tooltipInfoTh">
                <div>
                <span rel="tooltip" className="modalTooltips arangoicon icon_arangodb_info"
                      title="Maximum number of writers (segments) cached in the pool.">
                </span>
                </div>
              </th>
            </tr>

            <tr>
              <th className="collectionInfoTh2">Write Buffer Size Max:</th>
              <th className="collectionInfoTh">
                <div className="modal-text">
                  {view.writebufferSizeMax}
                </div>
              </th>
              <th className="tooltipInfoTh">
                <div>
                <span rel="tooltip" className="modalTooltips arangoicon icon_arangodb_info"
                      title="Maximum memory byte size per writer (segment) before a writer (segment) flush is triggered.">
                </span>
                </div>
              </th>
            </tr>
            </tbody>
          </ArangoTable>

          <div style={{
            color: '#717d90',
            fontWeight: 600,
            fontSize: '12.5pt',
            padding: 10,
            borderBottom: '1px solid rgba(0, 0, 0, .3)',
            borderTop: '1px solid rgba(0, 0, 0, .3)'
          }}>
            Primary Sort
          </div>
          <ArangoTable id={'viewPrimarySortTable'}>
            <tbody>
            <tr>
              <th className="collectionInfoTh2">Primary Sort Compression:</th>
              <th className="collectionInfoTh">
                <div className="modal-text">
                  {view.primarySortCompression}
                </div>
              </th>
              <th className="tooltipInfoTh">
                <div>
                <span rel="tooltip" className="modalTooltips arangoicon icon_arangodb_info"
                      title="Defines how to compress the primary sort data.">
                </span>
                </div>
              </th>
            </tr>

            <tr>
              <th className="collectionInfoTh2">Primary Sort Order:</th>
              <th className="collectionInfoTh">
                <div className="modal-text">
                  <PrimarySortView primarySort={view.primarySort}/>
                </div>
              </th>
              <th className="tooltipInfoTh"/>
            </tr>
            </tbody>
          </ArangoTable>

          <div style={{
            color: '#717d90',
            fontWeight: 600,
            fontSize: '12.5pt',
            padding: 10,
            borderBottom: '1px solid rgba(0, 0, 0, .3)',
            borderTop: '1px solid rgba(0, 0, 0, .3)'
          }}>
            Stored Values
          </div>
          <ArangoTable id={'viewStoredValuesTable'}>
            <tbody>
            <tr>
              <th className="collectionInfoTh2">Stored Values:</th>
              <th className="collectionInfoTh">
                <div className="modal-text">
                  <StoredValuesView storedValues={view.storedValues}/>
                </div>
              </th>
              <th className="tooltipInfoTh">
                <div>
                <span rel="tooltip" className="modalTooltips arangoicon icon_arangodb_info"
                      title="An array of objects to describe which document attributes to store in the View index.">
                </span>
                </div>
              </th>
            </tr>
            </tbody>
          </ArangoTable>
        </div>
      </div>
    </div>
  </div>;
};

window.ViewInfoReactView = ViewInfoReactView;
