#include "libc.h" 

int main() {
    printf("*** Simple heap test\n");

    // Track memory usage
    //printf("*** Memory before any allocation: %d bytes\n", getFreeMemory());

    // Allocate 250 bytes
    printf("*** About to allocate 250 bytes\n");
    void* ptr1 = malloc(250);
    // if (ptr1) {
    //     printf("*** Allocated 250 bytes, memory now: %d bytes free\n", getFreeMemory());
    // } else {
    //     printf("*** Failed to allocate 250 bytes\n");
    // }

    // Allocate another 250 bytes
    // printf("*** About to allocate another 250 bytes\n");
    // void* ptr2 = malloc(250);
    // if (ptr2) {
    //     printf("*** Allocated another 250 bytes, memory now: %d bytes free\n", getFreeMemory());
    // } else {
    //     printf("*** Failed to allocate another 250 bytes\n");
    // }

    // Free the first allocation
//     printf("*** About to free the first 250 bytes\n");
    free(ptr1);

    printf("*** Value of mem tracker %d", getMemoryTracker());
   // printf("*** Freed 250 bytes, memory now: %d bytes free\n", getFreeMemory());

//     // Free the second allocation
//     printf("*** About to free the second 250 bytes\n");
//     free(ptr2);
   // printf("*** Freed another 250 bytes, memory now: %d bytes free\n", getFreeMemory());

    // // Allocate 500 bytes
    // printf("*** About to allocate 500 bytes\n");
    // void* ptr3 = malloc(500);
    // // if (ptr3) {
    // //     printf("*** Allocated 500 bytes, memory now: %d bytes free\n", getFreeMemory());
    // // } else {
    // //     printf("*** Failed to allocate 500 bytes\n");
    // // }

    // // Free the 500 bytes
    // // printf("*** About to free the 500 bytes\n");
    // // free(ptr3);
    // //printf("*** Freed 500 bytes, memory now: %d bytes free\n", getFreeMemory());




    printf("*** Simple heap test end\n");
    shutdown();
    return 0;
}
