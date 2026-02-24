# Parallel Community Detection using Label Propagation (MPI)

A high-performance implementation of the **Label Propagation Algorithm (LPA)** for detecting community structures in large-scale graphs. This project leverages **MPI (Message Passing Interface)** to parallelize the detection process across multiple processing nodes.

> **Academic Note:** This project was developed as part of a course for my **Master’s Degree**.

## Overview

The Label Propagation Algorithm is a fast, near-linear time method for finding communities in a network. This implementation is designed for distributed-memory systems and uses the **Compressed Sparse Row (CSR)** format to optimize memory and traversal speed.



### Key Features
* **Parallel Execution:** Distributes node processing across multiple MPI ranks.
* **Efficient Memory:** Uses CSR representation to handle large adjacency matrices efficiently.
* **Master-Worker Model:** Rank 0 handles file I/O and result aggregation, while all ranks participate in the computation.
* **Convergence Tracking:** Uses `MPI_Allreduce` to monitor global stability across the network.
* **Detailed Logging:** Each rank generates its own log file (`process_X.txt`) for granular debugging of the propagation process.

## 🛠 Technical Details

### Data Structures
The graph is represented using a **CSR** structure, which stores all edges in a single flat array to maximize cache hits and minimize memory overhead.



### Core Algorithm
1.  **Graph Loading:** Rank 0 reads the input edge list and broadcasts the structure to all workers.
2.  **Workload Distribution:** Each process is assigned a specific range of nodes based on its rank.
3.  **Iterative Update:**
    * Nodes are shuffled locally to avoid oscillations.
    * Each process determines the "best label" for its nodes by finding the most frequent label among neighbors.
4.  **Global Synchronization:** Labels are synchronized across all processes using `MPI_Allgatherv` at the end of every iteration.
5.  **Termination:** The process repeats until a global steady state is reached (no labels change).

## Getting Started

### Prerequisites
* A C compiler (e.g., `gcc`)
* An MPI implementation (e.g., OpenMPI or MPICH)

### Compilation
Compile the source code using the MPI wrapper:
```bash
mpicc -O3 -o community_detect label_propagation.c
