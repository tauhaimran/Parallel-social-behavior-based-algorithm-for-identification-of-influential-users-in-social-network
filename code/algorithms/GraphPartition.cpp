#include "GraphPartition.h"
#include <iostream>
#include <metis.h>

PartitionResult metis_partition(MetisGraph* mg, int n_partitions = 2) {
    PartitionResult result;
    result.nparts = n_partitions;
    result.partitions.resize(mg->nvtxs);

    idx_t nvtxs = mg->nvtxs;
    idx_t ncon = 1;           // Number of balancing constraints
    idx_t objval;             // Objective value (unused)
    idx_t* xadj = mg->xadj.data();
    idx_t* adjncy = mg->adjncy.data();
    idx_t* vwgt = nullptr;    // Vertex weights (none)
    idx_t* vsize = nullptr;   // Vertex sizes (none)
    idx_t* adjwgt = nullptr;  // Edge weights (none)

    // METIS options
    idx_t options[METIS_NOPTIONS];
    METIS_SetDefaultOptions(options);
    options[METIS_OPTION_CONTIG] = 0;  // Enable disk-based mode for large graphs

    // Call METIS
    int ret = METIS_PartGraphKway(
        &nvtxs, &ncon, xadj, adjncy, 
        vwgt, vsize, adjwgt, 
        &n_partitions, NULL, NULL, 
        options, &objval, result.partitions.data()
    );

    if(ret != METIS_OK) {
        std::cerr << "METIS partitioning failed!\n";
    }

    return result;
}