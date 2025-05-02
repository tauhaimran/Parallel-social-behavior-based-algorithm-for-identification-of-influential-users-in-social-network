#include <iostream>
#include <omp.h>

int main() {
    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        int total = omp_get_num_threads();
        #pragma omp critical
        std::cout << "OpenMP: Hello from thread " << tid << " of " << total << std::endl;
    }

    return 0;
}
// Compile with: g++ -o openmp_test openmp_test.cpp -fopenmp
// Run with: ./openmp_test