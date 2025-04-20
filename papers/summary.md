Here’s a **technical summary of the research paper** *"Parallel Social Behavior-Based Algorithm for Identification of Influential Users in Social Network"* tailored for your **PDC project (Phase 1)**:

---

## 📄 Paper Title:
**Parallel Social Behavior-Based Algorithm for Identification of Influential Users in Social Network**  
Authors: Wassim Mnasri, Mehdi Azaouzi, Lotfi Ben Romdhane  
Published in: *Applied Intelligence, 2021*

---

## 🔍 Problem Overview:
**Influence Maximization (IM)** is the task of identifying users (nodes) in a social network who can maximize the spread of information. Traditional methods rely heavily on structural (graph-based) properties and ignore **semantic behavior** (e.g., user interests, interaction types like likes, shares).

---

## 🧠 Core Contribution — PSAIIM Algorithm:
The authors propose **PSAIIM**:  
**Parallel Social behavior and Interest-based Influence Maximization**, which:

- Integrates **user interaction behavior** (likes, shares, comments) and **common interests** to model influence.
- Uses **community detection (SCC/CAC)** to partition the network and enable **parallelization**.
- Implements a **parallelized PageRank**-like algorithm on each community to compute **influence power**.
- Introduces **influence-BFS trees** to extract the final **seed set** of influential users.

---

## ⚙️ Parallelization Strategy:

### ✳️ Tools/Concepts Aligned with Your Project:
| Strategy/Component         | Your Project Tool |
|---------------------------|-------------------|
| Community detection        | **METIS**         |
| Inter-community parallelism | **MPI**           |
| Intra-community parallelism | **OpenMP**        |
| Influence power calculation | Parallel **PageRank** |
| Influence propagation      | Influence BFS Trees |

- **Graph Partitioning**: Uses SCC/CAC-based detection (you'll replace this with **METIS** for MPI-based workload distribution).
- **Parallel Execution**: Each partition is processed in **parallel threads** using multi-core CPUs.
- **Propagation Modeling**: Influence is modeled using a combination of **semantic weights** (actions + interest similarity) and **connectivity**.

---

## 🧪 Experimental Setup:
- Implemented in Java, tested on real datasets (e.g., Tencent Weibo, Twitter).
- Compared against baseline methods like PageRank, Greedy, MLIM.
- Showed **better scalability**, **higher influence spread**, and **lower runtime** using **multi-core parallelism**.

---

## 📈 Scalability Analysis:
- Speed-up scales with graph size (up to 15× improvement).
- Runtime grows sublinearly with node count due to effective partitioning.
- Memory usage is significantly optimized vs. baseline models.

---

## 📊 Why This Paper Works for the PDC Project:
- It involves **graph-based influence analysis**, a classic PDC problem.
- Uses **both inter-node (MPI)** and **intra-node (OpenMP)** parallelism.
- Provides a **custom influence metric** that can be implemented with MPI/OpenMP.
- Demonstrates **strong and weak scaling** on real datasets — exactly what's expected in Phase 2.

---

## ✅ Summary of What You Need to Present (Phase 1):

| Requirement | Content You Can Present |
|------------|-------------------------|
| Problem | Influence Maximization in Social Networks |
| Parallel Algorithm | PSAIIM: Uses semantic + structural properties with parallel PageRank |
| Strategy | Community detection → Parallel Influence Power Calculation → Seed Selection |
| Tools | Use METIS for graph partitioning, MPI for components, OpenMP for BFS/PR inside |
| Outcome | Higher speed, better influence spread, lower memory |

---

Let me know if you'd like a **PowerPoint outline** or a **code skeleton suggestion** for starting Phase 2.