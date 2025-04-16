#include <iostream>
#include <omp.h>

int main() {
    std::cout << "Program starting..." << std::endl;

    // Optional: Set the number of threads programmatically
    // omp_set_num_threads(4); // Or use OMP_NUM_THREADS environment variable

    // This pragma marks the start of a parallel region.
    // The code inside the {} block will be executed by multiple threads.
    #pragma omp parallel
    {
        // Get the unique ID of the current thread
        int thread_id = omp_get_thread_num();

        // Get the total number of threads executing in this parallel region
        int num_threads = omp_get_num_threads();

        // Each thread will print its own message
        // Using std::cout requires careful synchronization in more complex scenarios,
        // but for simple prints like this, it's often okay, though output might interleave.
        // Using printf might be slightly safer for interleaved output in simple cases.
        #pragma omp critical // Ensures only one thread prints at a time to avoid garbled output
        {
            std::cout << "Hello from thread " << thread_id
                      << " out of " << num_threads << " threads." << std::endl;
        }

        // Example of work done by each thread (optional)
        // #pragma omp for // Could add a parallel loop here if needed
        // for(int i=0; i < 5; ++i) {
        //    printf("Thread %d processing item %d\n", thread_id, i);
        // }

    } // End of the parallel region

    std::cout << "Parallel region finished." << std::endl;
    std::cout << "Program finished." << std::endl;

    return 0;
}
