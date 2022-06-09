import React, { ReactNode } from "react";
import { ArangoTable, ArangoTD, ArangoTH } from "../../../components/arango/table";
import { Link } from 'react-router-dom';

type ViewLinkLayoutProps = {
  fragments?: string[];
  children: ReactNode;
};

const ViewLinkLayout = ({ fragments = [], children }: ViewLinkLayoutProps) =>
  <ArangoTable>
    <thead>
    <tr>
      <ArangoTH seq={0} style={{ width: "100%" }}>
        <Link to={'/'}>Links</Link>
        {
          fragments.slice(0, fragments.length - 1).map((fragment, idx) => {
            const path = fragments.slice(0, idx + 1).join('/');

            return <span key={idx}>&#47;<Link to={`/${path}`}>{fragment}</Link></span>;
          })
        }
        &#47;{fragments[fragments.length - 1]}
      </ArangoTH>
    </tr>
    </thead>
    <tbody>
    <tr>
      <ArangoTD seq={0}>{children}</ArangoTD>
    </tr>
    </tbody>
  </ArangoTable>;

export default ViewLinkLayout;
