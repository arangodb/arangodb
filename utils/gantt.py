#!/usr/bin/python3
from pathlib import Path
import sys
from dash import Dash, html, dash_table, dcc, callback, Output, Input
import plotly.express as px
import pandas as pd
import plotly.graph_objects as go
from plotly.subplots import make_subplots

#print(dir(px))
#print(dir(dcc))
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
    append_dict_to_df,
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

df_load = get_load()
df_jobs = pd.DataFrame(jobs)

fig_gantt = px.timeline(df_jobs, x_start="Start", x_end="Finish", y="Task")
fig_gantt.update_yaxes(autorange="reversed") # otherwise tasks are listed from the bottom

app = Dash()

app.layout = [
    html.Div(children='Load to CI jobs'),
    html.Hr(),
    # dcc.RadioItems(options=['pop', 'lifeExp', 'gdpPercap'], value='lifeExp', id='controls-and-radio-item'),
    #dash_table.DataTable(data=df_jobs.to_dict('records'), page_size=6),
    #dcc.Graph(figure={}, id='controls-and-graph')
    html.Div([
        html.H1(children='Testsystem Analyzer'),

        html.Div(children='''
            Testsuites and their run times
        '''),

        dcc.Graph(
            id='gantt-chart',
            figure=fig_gantt
        ),
    ]),
    html.Div([
        dcc.Graph(id='load-graph'),
        dcc.Checklist(
            id="checklist",
            options=["load", "netio"],
            value=[ "load", "netio"],
            inline=True
        )
    ]),

]


@app.callback(
    Output("load-graph", "figure"),
    #Output("gantt-chart", "figure"),
    Input("checklist", "value"))


def update_line_chart(gauge):
    #print(gauge)
    #fig = go.Figure()
    #fig.add_trace(go.Scatter(y=df_jobs["Date"], x=df_jobs["load"], mode='lines', name='Load'))
    #fig = px.line(data_frame=df_load,
    #        x="Date", y="load")
    # Create figure with secondary y-axis
    fig = make_subplots(specs=[[{"secondary_y": True}]])

    # Add traces
    fig.add_trace(
        go.Scatter(x=df_load['Date'], y=df_load['load'], # replace with your own data source
        name="Load"), secondary_y=False,
    )

    if 'netio' in gauge:
        fig.add_trace(
            go.Scatter(x=df_load['Date'], y=df_load['netio'], # replace with your own data source
                       name="NetIO"), secondary_y=True
        )

    # Add figure title
    fig.update_layout(title_text="Load graph")

    # Set x-axis title
    fig.update_xaxes(title_text="Date")

    # Set y-axes titles
    fig.update_yaxes(
        title_text="<b>Load</b>",
        secondary_y=False)
    fig.update_yaxes(
        title_text="<b>Net IO LO</b>",
        secondary_y=True)

    return fig


# Add controls to build the interaction
#@callback(
    # Output(component_id='controls-and-graph', component_property='figure'),
    #Input(component_id='controls-and-radio-item', component_property='value')
    #)

def update_graph(col_chosen):
    print(df_jobs)
    return fig_gantt
#    #fig1 = px.timeline(df_jobs, x_start="Start", x_end="Finish", y="Task")
#    #fig1.update_yaxes(autorange="reversed") # otherwise tasks are listed from the bottom up
#    # fig = px.histogram(df_jobs, x='continent', y=col_chosen, histfunc='avg')
#    load_df = get_load()
#    # print(load_df)
#    fig2 = px.scatter(load_df, x=load_df["Date"], y=load_df["load"])
#    return  fig2


if __name__ == '__main__':
    app.run(debug=True)
