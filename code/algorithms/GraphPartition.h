#ifndef GRAPH_PARTITION_H
#define GRAPH_PARTITION_H

#include "DataPrepration.h"

struct PartitionResult {
    std::vector<idx_t> partitions;  // Partition ID for each node
    idx_t nparts;                   // Total partitions
};

PartitionResult metis_partition(MetisGraph* mg, int n_partitions);

#endif