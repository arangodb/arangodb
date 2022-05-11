/* global arangoHelper, arangoFetch, frontendConfig, $ */

import React from "react";
import { Button, Tooltip } from 'antd';
import { ArrowUpOutlined } from '@ant-design/icons';

const ButtonScrollTo = ({ graphName, onGraphDataLoaded }) => {
  
  return (
    <>
      <Tooltip placement="bottom" title={"Scroll to settings"}>
        <Button
          type="primary"
          style={{ background: "#2ecc71", borderColor: "#2ecc71", float: "right" }}
          icon={<ArrowUpOutlined />}
          onClick={() => {
            const element = document.getElementById("content");
            console.log("element: ", element);
            element.scrollIntoView({ behavior: "smooth" });
          }}>
            To settings
        </Button>
      </Tooltip>
    </>
  );
};

export default ButtonScrollTo;
