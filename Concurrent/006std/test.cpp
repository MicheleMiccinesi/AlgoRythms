/* License : Michele Miccinesi 2018 -           */
/* test of Divide Et Impera Parallel Framework  */

#define DFS_TO_ME_BFS_TO_YOU
#define ALL_BUT_1_DISTRIBUTE_WJ
//#define CIRCULAR_QUEUE
//#define AVOID_CAS

#include "all.cpp"
namespace mergeSort {
#include "mergesort.cpp"
}
namespace unbMergeSort {
#include "unbalancedmergesort.cpp"
}
namespace quickSort {
#include "quicksort.cpp"
}
namespace pMergeSort {
#include "pmergesort.cpp"
}
#include <limits>
namespace parser {
#include "parser.cpp"
}


int main(int argc, char *argv[]){
    auto *myTest(parser::parser(argc, argv));
    if( myTest ){
        myTest->start();
        delete myTest;
    }
}
