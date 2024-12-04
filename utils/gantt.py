#!/usr/bin/python3
from pathlib import Path
import sys
from dash import Dash, html, dash_table, dcc, callback, Output, Input
import plotly.express as px
import pandas as pd
# import polars as pd
import plotly.graph_objects as go
from plotly.subplots import make_subplots
from datetime import datetime

from tools.process_examine import (
    LOADS,
    MEMORY,
    TREE_TEXTS,
    PARSED_LINES,
    GAUGES,
    load_file,
    load_testing_js_mappings,
    build_process_tree,
    convert_pids_to_gantt,
    get_load,
    append_dict_to_df,
    jobs_db_overview,
    load_db_overview,
    load_db_stacked_jobs,
)
CURRENT_TESTS = ['none']
DATE_FORMAT = '%Y-%m-%d %H:%M:%S.%f'
load = get_load()
#print(load)
jobs = jobs_db_overview()
#print(jobs)
df_load = pd.DataFrame(load)
df_jobs = pd.DataFrame(jobs)
#print(df_jobs)
external_stylesheets = ['https://codepen.io/chriddyp/pen/bWLwgP.css']

# print(df_load)
time_range = [
    df_load["Date"][0],
    df_load["Date"][len(df_load)-1]
]
styles = {
    'pre': {
        'border': 'thin lightgrey solid',
        'overflowX': 'scroll'
    }
}

RESOLUTION=60
app = Dash(__name__, external_stylesheets=external_stylesheets)

app.layout = [
    html.Div(children='Load to CI jobs'),
    html.Hr(),
    html.Div([
        html.H1(children='Testsystem Analyzer'),

        html.Div(children='''
            Testsuites and their run times
        '''),
        dcc.Graph(id='gantt-chart'),
    ]),
    html.Div([
        dcc.Graph(id='load-graph'),
        dcc.Checklist(
            id="loadgraph-checklist",
            options=["load", "netio"],
            value=[ "load", "netio"],
            inline=True
        )
    ]),
    html.Div(className='row', children=[
        html.Div([
            html.Pre(id='relayout-data', style=styles['pre']),
        ], className='three columns')
    ]),
    html.Div([
        dcc.Graph(id='stacked-area-resource-cpu'),
        dcc.Dropdown(GAUGES, GAUGES[0], id='choose_gauge'),
        dcc.Dropdown(CURRENT_TESTS, 'test', id='choose_test')
    ]),
]

@app.callback(
    Output("load-graph", "figure"),
    Input("loadgraph-checklist", "value"))
def update_line_chart(gauge_select):
    # Create figure with secondary y-axis
    fig = make_subplots(specs=[[{"secondary_y": True}]])
    # Add traces
    fig.add_trace(
        go.Scatter(x=df_load['Date'], y=df_load['load'], # replace with your own data source
        name="Load"), secondary_y=False,
    )
    if 'netio' in gauge_select:
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

    fig.update_layout(
        xaxis=dict(
            rangeselector=dict(
                buttons=list([
                    dict(count=1,
                         label="1m",
                         step="month",
                         stepmode="backward"),
                    dict(count=6,
                         label="6m",
                         step="month",
                         stepmode="backward"),
                    dict(count=1,
                         label="YTD",
                         step="year",
                         stepmode="todate"),
                    dict(count=1,
                         label="1y",
                         step="year",
                         stepmode="backward"),
                    dict(step="all")
                ])
            ),
            rangeslider=dict(
                visible=True
            ),
            type="date"
        )
    )

    return fig

@app.callback(
    Output("gantt-chart", "figure"),
    Input('load-graph', 'relayoutData'))
def update_line_chart(relayoutData):
    date_range = []
    if (relayoutData and
        not 'autosize' in relayoutData and
        not 'xaxis.autorange' in relayoutData):
        if "xaxis.range" in relayoutData:
            date_range = [
                pd.to_datetime(relayoutData['xaxis.range'][0]),
                pd.to_datetime(relayoutData['xaxis.range'][1]),
            ]
        if "xaxis.range[0]" in relayoutData:
            date_range = [
                pd.to_datetime(relayoutData['xaxis.range[0]']),
                pd.to_datetime(relayoutData['xaxis.range[1]']),
            ]
        fig_gantt = px.timeline(df_jobs[
            (df_jobs["Start"] <= date_range[0]) & (df_jobs["Finish"] >= date_range[1]) |
            (df_jobs["Start"] >= date_range[1]) & (df_jobs["Finish"] <  date_range[1]) |
            (df_jobs["Start"] >  date_range[0]) & (df_jobs["Finish"] <= date_range[0]) |
            (df_jobs["Start"] >  date_range[0]) & (df_jobs["Finish"] <  date_range[1])]
        , x_start="Start", x_end="Finish", y="Task")
    else:
        fig_gantt = px.timeline(df_jobs, x_start="Start", x_end="Finish", y="Task")
    fig_gantt.update_yaxes(autorange="reversed") # otherwise tasks are listed from the bottom
    return fig_gantt

@app.callback(
    Output("stacked-area-resource-cpu", "figure"),
    Output("choose_test", "options"),
    Input('load-graph', 'relayoutData'),
    Input('choose_gauge', 'value'))
def update_stacked_cpu_chart(relayoutData, chosen_gauge):
    global CURRENT_TESTS
    x=['Winter', 'Spring', 'Summer', 'Fall']
    fig = go.Figure()
    if (relayoutData and
        not 'autosize' in relayoutData and
        not 'xaxis.autorange' in relayoutData):
        if "xaxis.range" in relayoutData:
            date_range = [
                pd.to_datetime(relayoutData['xaxis.range'][0]),
                pd.to_datetime(relayoutData['xaxis.range'][1]),
            ]
        if "xaxis.range[0]" in relayoutData:
            date_range = [
                pd.to_datetime(relayoutData['xaxis.range[0]']),
                pd.to_datetime(relayoutData['xaxis.range[1]']),
            ]
        info = load_db_stacked_jobs(date_range, chosen_gauge)
        df = pd.DataFrame(info[1])
        CURRENT_TESTS=info[0]
        fig = go.Figure()
        for column in info[0]:
            fig.add_trace(go.Scatter(
                x=df["Date"], y=df[column],
                hoverinfo='x+y',
                mode='lines',
                line=dict(width=0.5#, color='rgb(131, 90, 241)'
                          ),
                stackgroup='one', # define stack group
                name=column
            ))
        fig.update_layout(yaxis_range=(0, 100))
        return (fig, CURRENT_TESTS)
    fig.update_layout(yaxis_range=(0, 100))
    return (fig, CURRENT_TESTS)
if __name__ == '__main__':
    app.run(debug=True)
