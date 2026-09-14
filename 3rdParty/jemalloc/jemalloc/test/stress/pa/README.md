# Page Allocator (PA) Microbenchmark Suite

This directory contains a comprehensive microbenchmark suite for testing and analyzing jemalloc's Page Allocator (PA) system, including the Hugepage-aware Page Allocator (HPA) and Slab Extent Cache (SEC) components.

## Overview

The PA microbenchmark suite consists of two main programs designed to preprocess allocation traces and replay them against jemalloc's internal PA system to measure performance, memory usage, and allocation patterns.

To summarize how to run it, assume we have a file `test/stress/pa/data/hpa.csv` collected from a real application using USDT, the simulation can be run as follows:
```
make tests_pa
./test/stress/pa/pa_data_preprocessor hpa test/stress/pa/data/hpa.csv test/stress/pa/data/sample_hpa_output.csv
./test/stress/pa/pa_microbench -p -o test/stress/pa/data/sample_hpa_stats.csv test/stress/pa/data/sample_hpa_output.csv
```

If it's sec, simply replace the first parameter passed to `pa_data_preprocessor` with sec.

## Architecture

### PA System Components

The Page Allocator sits at the core of jemalloc's memory management hierarchy:

```
Application
    ↓
Arena (tcache, bins)
    ↓
PA (Page Allocator) ← This is what we benchmark
    ├── HPA (Hugepage-aware Page Allocator)
    └── SEC (Slab Extent Cache)
    ↓
Extent Management (emap, edata)
    ↓
Base Allocator
    ↓
OS (mmap/munmap)
```

### Microbenchmark Architecture

```
Raw Allocation Traces
    ↓
[pa_data_preprocessor] ← Preprocesses and filters traces
    ↓
CSV alloc/dalloc Files
    ↓
[pa_microbench] ← Replays against real PA system
    ↓
Performance Statistics & Analysis
```

## Programs

### 1. pa_data_preprocessor

A C++ data preprocessing tool that converts raw allocation traces into a standardized CSV format suitable for microbenchmarking.

**Purpose:**
- Parse and filter raw allocation trace data
- Convert various trace formats to standardized CSV
- Filter by process ID, thread ID, or other criteria
- Validate and clean allocation/deallocation sequences

### 2. pa_microbench

A C microbenchmark that replays allocation traces against jemalloc's actual PA system to measure performance and behavior with HPA statistics collection.

**Purpose:**
- Initialize real PA infrastructure (HPA, SEC, base allocators, emaps)
- Replay allocation/deallocation sequences from CSV traces
- Measure allocation latency, memory usage, and fragmentation
- Test different PA configurations (HPA-only vs HPA+SEC)
- Generate detailed HPA internal statistics

**Key Features:**
- **Real PA Integration**: Uses jemalloc's actual PA implementation, not simulation
- **Multi-shard Support**: Tests allocation patterns across multiple PA shards
- **Configurable Modes**: Supports HPA-only mode (`-p`) and HPA+SEC mode (`-s`)
- **Statistics Output**: Detailed per-shard statistics and timing data
- **Configurable Intervals**: Customizable statistics output frequency (`-i/--interval`)

## Building

### Compilation

```bash
# Build both PA microbenchmark tools
cd /path/to/jemalloc
make tests_pa
```

This creates:
- `test/stress/pa/pa_data_preprocessor` - Data preprocessing tool
- `test/stress/pa/pa_microbench` - PA microbenchmark

## Usage

### Data Preprocessing

```bash
# Basic preprocessing
./test/stress/pa/pa_data_preprocessor <hpa/sec> input_trace.txt output.csv
```

### Microbenchmark Execution

```bash
# Run with HPA + SEC (default mode)
./test/stress/pa/pa_microbench -s -o stats.csv trace.csv

# Run with HPA-only (no SEC)
./test/stress/pa/pa_microbench -p -o stats.csv trace.csv

# Show help
./test/stress/pa/pa_microbench -h
```
