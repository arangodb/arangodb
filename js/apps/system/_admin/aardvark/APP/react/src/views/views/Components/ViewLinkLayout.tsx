import React, { ReactNode } from "react";
import { ArangoTable, ArangoTD, ArangoTH } from "../../../components/arango/table";
import { Link } from 'react-router-dom';

type ViewLinkLayoutProps = {
  fragments?: string[];
  children: ReactNode;
};

const ViewLinkLayout = ({ fragments = [], children }: ViewLinkLayoutProps) =>
  <div
    id={'modal-dialog'}
    className={'createModalDialog'}
    tabIndex={-1}
    role={'dialog'}
    aria-labelledby={'myModalLabel'}
    aria-hidden={'true'}
  >
    <div className="modal-body" style={{ overflowY: 'visible' }}>
      <div className={'tab-content'}>
        <div className="tab-pane tab-pane-modal active">
          <main>
            <ArangoTable>
              <thead>
              <tr>
                <ArangoTH seq={0} style={{ width: "100%" }}>
                  <Link to={'/'}>Links</Link>
                  {
                    fragments.slice(0, fragments.length - 1).map((fragment, idx) => {
                      const path = fragments.slice(0, idx + 1).join('/');

                      return <span key={idx}>&nbsp;&#47;&nbsp;<Link to={`/${path}`}>{fragment}</Link></span>;
                    })
                  }
                  &nbsp;&#47;&nbsp;{fragments[fragments.length - 1]}
                </ArangoTH>
              </tr>
              </thead>
              <tbody>
              <tr>
                <ArangoTD seq={0}>{children}</ArangoTD>
              </tr>
              </tbody>
            </ArangoTable>
          </main>
        </div>
      </div>
    </div>
  </div>;

export default ViewLinkLayout;
