import React from "react";
import { LoadingOutlined } from '@ant-design/icons';

const LoadingSpinner = () => {
  return (
    <div style={{ 'margin': '24px 0px', 'textAlign': 'center' }}>
      <LoadingOutlined style={{ 'fontSize': '48px', 'color': '#fbe08e' }}/>
    </div>
  );
}

export default LoadingSpinner;
