# Node Shaper: Define the layout of your edges


## Configuration

The configuration element of the Node Shaper takes several optional flags.
On example configuration would be the following:

  {
    shape: nodeShaper.shapes.CIRCLE,
    label: function(node){return node.id;},
    size: function(node){return 42},
    onclick: function(node){},
    ondrag: function(node){layouter.drag},
    onupdate: function(node){}
  }
  
The keys have the following effect:

* shape: One value from nodeShaper.shapes, defines the display style of the node.
* label: Either a value, defining the attribute name containing the label, or a function taking a node as input and returns the value that should be displayed
* size: Either a value or a function defining the size of each node. The effect may differ between shapes.
* onclick: Will be bound to every node and invoked if this node is clicked.
* ondrag: Will be invoked whenever a node is dragged. Should be included from the layouter.
* onupdate: Will be triggered on every update tick send by the layouter.


## Test Report


<http://localhost/~hacki/gineeded/jasmine_test/runnerEdgeShaper.html>