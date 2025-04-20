🎯 1. PowerPoint Presentation Outline for Phase 1 (PSAIIM)
Title Slide

Title: Parallel Social Behavior-Based Algorithm for Influential User Detection

Your name(s), Roll number(s), Course title

Slide 1: Problem Overview

Influence Maximization (IM) in social networks

Real-world applications: marketing, epidemic control, political influence

Slide 2: Limitations of Traditional IM

Structural-only models (greedy, PageRank, k-shell)

Lack of semantic consideration (likes, interests, behaviors)

Scalability issues with large networks

Slide 3: PSAIIM – Key Idea

Combines structure + semantic behavior

Parallel algorithm for large-scale social networks

Incorporates user interactions + interests (semantic edge weights)

Slide 4: PSAIIM Pipeline

Graph Partitioning → Parallel Influence Power Calculation → Seed Candidate Selection → Final Seed Selection

(Show diagram from paper if possible)

Slide 5: Semantic Modeling

Weighted user behavior: likes, comments, shares (with different influence levels)

Common interests using Jaccard similarity

Custom edge weight formula for influence

Slide 6: Graph Partitioning and Parallelism

Partition graph using SCC/CAC (we’ll use METIS)

MPI → for inter-community parallelism

OpenMP → for intra-community PageRank + BFS influence trees

Slide 7: Influence Power Calculation

Modified parallel PageRank using behavior weights + interest similarity

Execute in parallel for each graph partition

Slide 8: Seed Node Selection

Influence-BFS Tree concept

Select nodes with high influence spread + non-overlapping trees

Efficient propagation modeling

Slide 9: Tools & Technologies

MPI: Parallel processing across partitions

OpenMP: Multi-threaded PageRank and BFS

METIS: Efficient graph partitioning for MPI

Dataset: Twitter / Gnutella (small-scale to large)

Slide 10: Phase 2 Preview

Implement parallel PageRank + influence-BFS with OpenMP

METIS partitioned graphs processed via MPI

Measure speedup, scalability, memory usage

Visualize execution time vs. dataset size

Slide 11: References

Paper citation

Dataset sources

METIS and MPI documentation