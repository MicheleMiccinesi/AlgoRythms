# DEI
## Divide et Impera
The assignment was to write down a parallel framework to perform DAC given functions **divide**, **impera**, **base**, **isBase** by the mid-user... 
Actually this is a **Job Scheduler**, which addresses the problem of the "job stealing" technique of having to check the deque of possibly all workers to eventually find a job... With many workers, that is many processors as in Xeon, with many little jobs to accomplish this may be disadvantageous.

As you'll notice, the general "parallel framework" is to be considered "work in progress"..
