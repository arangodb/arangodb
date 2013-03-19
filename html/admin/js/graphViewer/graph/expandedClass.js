window.graph.expandedClass = function(node) {
  node.selectAll("circle") // Display nodes as circles
    .attr("class", function(d) { return d._expanded ? "expanded" : "none"});
}