#!/usr/bin/python3
from pathlib import Path
import sys
from dash import Dash, html, dash_table, dcc, callback, Output, Input
import plotly.express as px
import pandas as pd

from tools.process_examine import (
    LOADS,
    MEMORY,
    TREE_TEXTS,
    PARSED_LINES,
    load_file,
    load_testing_js_mappings,
    build_process_tree,
    convert_pids_to_gantt,
    get_load,
)

filter_str = None
if len(sys.argv) > 2:
    filter_str = sys.argv[2]
if len(sys.argv) > 3:
    start_pid = int(sys.argv[3])
main_log_file = Path(sys.argv[1])
PID_TO_FN = load_testing_js_mappings(main_log_file)
load_file(main_log_file, PID_TO_FN, filter_str)

jobs = convert_pids_to_gantt(PID_TO_FN)


df = pd.DataFrame(jobs)


app = Dash()

app.layout = [
    html.Div(children='My First App with Data, Graph, and Controls'),
    html.Hr(),
    # dcc.RadioItems(options=['pop', 'lifeExp', 'gdpPercap'], value='lifeExp', id='controls-and-radio-item'),
    dash_table.DataTable(data=df.to_dict('records'), page_size=6),
    #dcc.Graph(figure={}, id='controls-and-graph')
    # dcc.Dropdown(df.country.unique(), 'Canada', id='dropdown-selection'),
    dcc.Graph(id='graph-content')

]

# Add controls to build the interaction
@callback(
    Output(component_id='controls-and-graph', component_property='figure'),
    Input(component_id='controls-and-radio-item', component_property='value')
)

def update_graph(col_chosen):
    #fig = px.timeline(df, x_start="Start", x_end="Finish", y="Task")
    # fig.update_yaxes(autorange="reversed") # otherwise tasks are listed from the bottom up
    # fig = px.histogram(df, x='continent', y=col_chosen, histfunc='avg')
    fig = px.line(get_load(), x="time", y="load")
    return fig


if __name__ == '__main__':
    app.run(debug=True)
