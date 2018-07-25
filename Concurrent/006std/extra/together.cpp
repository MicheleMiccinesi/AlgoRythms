/* License : Michele Miccinesi 2018 -           */
/* test of Divide Et Impera Parallel Framework  */

#define DFS_TO_ME_BFS_TO_YOU
#define ALL_BUT_1_DISTRIBUTE_WJ
//#define CIRCULAR_QUEUE
//#define AVOID_CAS

/* License : Michele Miccinesi 2018 -       */
/* Framework for                            */
/* Concurrent Divide and Conquer            */

#include <cassert>

#include <iostream>
#include <vector>
#include <deque>
#include <thread>
#include <functional>
#include <string>
#include <random>
#include <atomic>
#include <tuple>
#include <fstream>
#include <chrono>

/* https://youtu.be/nXaxk27zwlk?t=2441  */
#if defined(__GNUC__)
    #define ALWAYS_INLINE __attribute__((always_inline))
#else
    #define ALWAYS_INLINE
#endif

#if (defined(__GNUC__) || defined(__clang__))
    #define HAS_INLINE_ASSEMBLY
#endif

#ifdef HAS_INLINE_ASSEMBLY
template <class Tp>
inline ALWAYS_INLINE void iLikeUSUR(Tp const& value) {
  asm volatile("" : : "r,m"(value) : "memory");
}

template <class Tp>
inline ALWAYS_INLINE void iLikeUSUR(Tp& value) {
#if defined(__clang__)
  asm volatile("" : "+r,m"(value) : : "memory");
#else
  asm volatile("" : "+m,r"(value) : : "memory");
#endif
}

inline ALWAYS_INLINE void rwBarrier() {
  asm volatile("" : : : "memory");
}
#endif

/* License : Michele Miccinesi 2018 -                                       */
/* Circular Queue MPMC, fixed # consumer = # producer ~ |Circular Buffer|   */
/* Hopefully mostly lock-free and wait-free even on your hardware :)        */

inline uint32_t ceilPow2(uint32_t n){
    n|=(n>>1);
    n|=(n>>2);
    n|=(n>>4);
    n|=(n>>8);
    n|=(n>>16);
    return n;
}

/* this is not a general MPMC FIFO ring Buffer                  */
/* much simpler, because we have just n producers               */
/* n consumers                                                  */

#ifdef CIRCULAR_QUEUE
#ifdef AVOID_CAS
template <class T>
class circleQueue{
    const T t;
    const uint32_t n, N;
    std::atomic<uint32_t> b, e, bSync;
    std::atomic<int32_t> d;
    std::vector<std::atomic<T>> Q;
public:
    circleQueue(const uint32_t& nn, const T& t, uint32_t perfMarginShift=4) : t(t), n(nn), N(ceilPow2(n<<perfMarginShift)), b(0), e(0), bSync(0), d(0), Q(N+1) {
        for( int i=0; i<=N; ++i )
            std::atomic_init(&Q[i], t);
    }

    int32_t push(const T& o){
        uint32_t te{e.fetch_add(1, std::memory_order_relaxed)};
        while( te>bSync.load(std::memory_order_acquire)+N );        //first PROBABILISTIC sync: remove this to get an infty living queue
        //above: avoiding CAS the most as possible
        te &= N;
        for(T tt{t}; !Q[te].compare_exchange_weak(tt, o, std::memory_order_release, std::memory_order_relaxed); tt=t); //deterministic sync
        return d.fetch_add(1, std::memory_order_relaxed)+1;
    }

    std::pair<T, bool> pop(){
        T volatile v(t);        /* is this useful to avoid the cycle being optimized away?  */
        if( d.fetch_sub(1, std::memory_order_relaxed) > 0 ){
            uint32_t tb{b.fetch_add(1, std::memory_order_relaxed) & N};
            while( (v = Q[tb].exchange(t, std::memory_order_acquire)) == t );
            bSync.fetch_add(1, std::memory_order_release);
            return std::make_pair(v, true); 
        } else {
            d.fetch_add(1, std::memory_order_relaxed);
            return std::make_pair(v, false);
        }
    }

    bool pop(T& v){
        if( d.fetch_sub(1, std::memory_order_relaxed) > 0 ){
            uint32_t tb{b.fetch_add(1, std::memory_order_relaxed) & N};
            while( (v = Q[tb].exchange(t, std::memory_order_acquire)) == t );
            bSync.fetch_add(1, std::memory_order_release);
            return true;    
        } else {
            d.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
    }

    bool popFast(T& v){
        if( d.fetch_sub(1, std::memory_order_relaxed) > 0 ){
            if( (v = Q[b.fetch_add(1, std::memory_order_relaxed) & N].exchange(t, std::memory_order_acquire)) != t ){
                bSync.fetch_add(1, std::memory_order_release);
                return true;    
            } else {
                d.fetch_add(1, std::memory_order_relaxed);
                return false;
            }
        } else {
            d.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
    }

    bool full(){
        return static_cast<uint32_t>(d.load(std::memory_order_relaxed))==n;
    }
};
#else
/* Here I am relying for synchronization on weak CAS only ...   */
template <class T>
class circleQueue{
    const T t;
    const uint32_t n, N;
    std::atomic<uint32_t> b, e;
    std::atomic<int32_t> d;
    std::vector<std::atomic<T>> Q;
public:
    circleQueue(const uint32_t& nn, const T& t, uint32_t perfMarginShift=4) : t(t), n(nn), N(ceilPow2(n<<perfMarginShift)), b(0), e(0), d(0), Q(N+1) {
        for( int i=0; i<=N; ++i )
            std::atomic_init(&Q[i], t);
    }

    int32_t push(const T& o){
        uint32_t te{e.fetch_add(1, std::memory_order_relaxed)};
        te &= N;
        for(T tt{t}; !Q[te].compare_exchange_weak(tt, o, std::memory_order_release, std::memory_order_relaxed); tt=t); //deterministic sync
        return d.fetch_add(1, std::memory_order_relaxed)+1;
    }

    std::pair<T, bool> pop(){
        T volatile v(t);        /* is this useful to avoid the cycle being optimized away?  */
        if( d.fetch_sub(1, std::memory_order_relaxed) > 0 ){
            uint32_t tb{b.fetch_add(1, std::memory_order_relaxed) & N};
            while( (v = Q[tb].exchange(t, std::memory_order_acquire)) == t );
            return std::make_pair(v, true); 
        } else {
            d.fetch_add(1, std::memory_order_relaxed);
            return std::make_pair(v, false);
        }
    }

    bool pop(T& v){
        if( d.fetch_sub(1, std::memory_order_relaxed) > 0 ){
            uint32_t tb{b.fetch_add(1, std::memory_order_relaxed) & N};
            while( (v = Q[tb].exchange(t, std::memory_order_acquire)) == t );
            return true;    
        } else {
            d.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
    }

    bool popFast(T& v){
        if( d.fetch_sub(1, std::memory_order_relaxed) > 0 ){
            if( (v = Q[b.fetch_add(1, std::memory_order_relaxed) & N].exchange(t, std::memory_order_acquire)) != t ){
                return true;    
            } else {
                d.fetch_add(1, std::memory_order_relaxed);
                return false;
            }
        } else {
            d.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
    }

    bool full(){
        return static_cast<uint32_t>(d.load(std::memory_order_relaxed))==n;
    }
};
#endif
#endif

class logger{
    const uint32_t n, N;
    std::vector<std::string> V;
    std::atomic<uint32_t> i;
    std::thread T;
public:
    logger(const uint32_t& nn, const std::string& out) : n(nn), N(ceilPow2(nn)), V(N+1), i(0),
        T(&logger::start, this, std::ref(std::cin), std::cref(out)) {}

    void start(std::istream& input, const std::string& out){
        std::ofstream logFile;
        logFile.open(out, std::ios::trunc | std::ios::out);
        int s;
        bool more{true};
        while(more){
            input >> s;
            if( s==1 ){
                uint32_t end(i.load(std::memory_order_relaxed) & N);
                for( uint32_t j=0; j<end; ++j ){
                    logFile << (V[j]+'\n');
                }
            } else {
                more=false;
            }
            logFile << "    -    -    --- --- ---- --- ---    -    -   \n\n";
            logFile.flush();
        }

        logFile.close();
    }

    void push(const std::string& s){
        V[i.fetch_add(1, std::memory_order_relaxed) & N] = s;
    }

    void push(std::vector<std::string>&& S){
        auto SS(S);
        for( auto &s: SS )
            push(s);
    }

    ~logger(){
        if(T.joinable())
            T.join();
    }
};

class jobLogger : public logger{
    std::atomic<uint32_t> toLog;
public:
    enum : uint32_t { jobSummary=1, jobDependency=2, jobReceiving=4, jobDoing=8, 
        workerAvailable=16, queueFull=32, jobDistributing=64, jobCompleted=128, 
        queueMoves=256, jobSummaryForQueueMoves=512|queueMoves };
        
    jobLogger(const uint32_t& nn, const std::string& out, uint32_t _toLog = 0) : logger(nn, out), toLog(_toLog) {}

    jobLogger& operator|=(const uint32_t &m){
        toLog.fetch_or(m, std::memory_order_relaxed);
        return *this;
    }

    jobLogger& operator^=(const uint32_t &m){
        toLog.fetch_xor(m, std::memory_order_relaxed);
        return *this;
    }

    jobLogger& operator&=(const uint32_t &m){
        toLog.fetch_and(m, std::memory_order_relaxed);
        return *this;
    }

    bool logging(const uint32_t &m){
        return (m&toLog.load(std::memory_order_relaxed))==m;
    }
};


/* License : Michele Miccinesi 2018 -       */
/* job manager: simple lock free scheduler  */
/* NOTE ABOUT MACRO:                        */
/* with DFS_TO_ME_BFS_TO_YOU the worker is taking jobs from top */
/*                           of the stack, otherwise from bottom*/
/* with ALL_BUT_1_DISTRIBUTE_WJ the worker is redistributing all*/
/*                              but 1 of waiting jobs which are */
/*                              ready to be processed, otherwise*/
/*                              it is not redistributing them   */

#ifdef DFS_TO_ME_BFS_TO_YOU
#define ALL_BUT_1_DISTRIBUTE_WJ
#endif

std::atomic<uint64_t> jobId{0};

class job{
public:
    virtual void operator()() {}

    virtual bool completed() { return false; }

    virtual bool inputReady() { return false; }

    virtual bool getPendingJob(job*&) { return false; }

    virtual std::vector<uint64_t> getDependencies() { return std::vector<uint64_t>(); } /* DBG  */

    virtual bool getDistributableJob(job*&) { return false; }

    virtual void* getOutput() { return nullptr; } 

    virtual uint64_t getJobId() { return 0; }

    virtual ~job() {}
};

template <typename group>
class genericWorker{
    const int id;
    std::string idS;
    std::atomic_flag busy;                  /* 1 Group per Time     */
    job *j;                                 /* Active Jobs          */
    std::deque<job*> WJ;                    /* Waiting Jobs         */
    std::deque<job*> DJ;                    /* Distributable Jobs   */
    std::deque<job*> CJ;                    /* Completed Jobs       */
    
    bool getWaitingJob(){
        trim();
        for( auto &wj: WJ ){
            if( wj!=nullptr ){
                if( wj->inputReady() ){
                    j=wj;
                    wj=nullptr;
                    return true;
                }
            }
        }
        return false;
    }

    bool getWaitingJobs(){
        trim();
        bool gotIt{false};
        for( auto &wj: WJ )
            if( wj!=nullptr )
                if( wj->inputReady() ){
                    DJ.push_back(wj);
                    wj=nullptr;
                    gotIt=true;
                }
        return gotIt;
    }

    void doJ(){
        (*j)();
        job* jj;
        while( j->getPendingJob(jj) )
            WJ.push_back(jj);
        while( j->getDistributableJob(jj) )
            DJ.push_back(jj);
            
        CJ.push_back(j);
        j=nullptr;
    }

    void doJ(jobLogger& logMe){
        (*j)();
        if( logMe.logging( jobLogger::jobCompleted ) )
            logMe.push(idS+" completed J"+std::to_string(j->getJobId()));
        job* jj;
        while( j->getPendingJob(jj) )
            WJ.push_back(jj);
        while( j->getDistributableJob(jj) )
            DJ.push_back(jj);
            
        CJ.push_back(j);
        j=nullptr;
    }

    bool processDistributableJob(group &g){
        if( DJ.empty() )
            return false;

        #ifdef ALL_BUT_1_DISTRIBUTE_WJ
        if( DJ.size()==1 ){
            #ifdef DFS_TO_ME_BFS_TO_YOU
            j=DJ.back();
            DJ.pop_back();
            #else
            j=DJ.front();
            DJ.pop_front();
            #endif
            doJ();
        } else 
        #endif
        if( g.offerJob(DJ.front()) ) {
            DJ.pop_front();
        } else {
            #ifdef DFS_TO_ME_BFS_TO_YOU
            j=DJ.back();
            DJ.pop_back();
            #else
            j=DJ.front();
            DJ.pop_front();
            #endif
            doJ();
        }
        return true;
    }

    bool processDistributableJob(group &g, jobLogger& logMe){
        if( !logMe.logging( jobLogger::jobDistributing ) )
            return processDistributableJob(g);
        logMe.push(idS+"=====> looking for some DJ: we have "+std::to_string(DJ.size())+" DJs");
        if( DJ.empty() )
            return false;

        #ifdef ALL_BUT_1_DISTRIBUTE_WJ
        if( DJ.size()==1 ){
            #ifdef DFS_TO_ME_BFS_TO_YOU
            j=DJ.back();
            DJ.pop_back();
            #else
            j=DJ.front();
            DJ.pop_front();
            #endif
            logMe.push(idS+"=====> doing DJ"+std::to_string(j->getJobId()));
            doJ(logMe);
        } else 
        #endif
        if( g.offerJob(DJ.front()) ) {
            logMe.push(idS+"=====> offered DJ"+std::to_string(DJ.front()->getJobId()));
            DJ.pop_front();
        } else {
            #ifdef DFS_TO_ME_BFS_TO_YOU
            j=DJ.back();
            DJ.pop_back();
            #else
            j=DJ.front();
            DJ.pop_front();
            #endif
            logMe.push(idS+"=====> doing DJ"+std::to_string(j->getJobId()));
            doJ(logMe);
        }
        return true;
    }

    void startCycle(group& g){      
        bool volatile doneNewJob{false};
        #ifndef ALL_BUT_1_DISTRIBUTE_WJ
        bool volatile gotWaitingJob{false};
        #endif
        while( !g.isCompleted() ){
            do {
                if( g.lookForJob(id, j) ){
                    doJ();
                    doneNewJob = true;
                }
                #ifdef ALL_BUT_1_DISTRIBUTE_WJ
                while( processDistributableJob(g) || getWaitingJobs() );
                #else
                while( processDistributableJob(g) || (gotWaitingJob=getWaitingJob()) ){
                    if( gotWaitingJob ){
                        doJ();
                        gotWaitingJob = false;
                    }
                }
                #endif
                if( doneNewJob ){
                    doneNewJob = false;
                    g.beAvailable(id);
                }
            } while( !g.closed() );
        }
    }

    void startLCycle(group& g, jobLogger& logMe){
        bool volatile doneNewJob{false};
        #ifndef ALL_BUT_1_DISTRIBUTE_WJ
        bool volatile gotWaitingJob{false};
        #endif
        while( !g.isCompleted() ){
            do {
                if( g.lookForJob(id, j) ){
                    if( logMe.logging( jobLogger::jobReceiving ) )
                        logMe.push(idS+"=====> receiving J"+std::to_string(j->getJobId()));
                    doJ(logMe);
                    doneNewJob = true;
                }
                #ifdef ALL_BUT_1_DISTRIBUTE_WJ
                while( processDistributableJob(g, logMe) || getWaitingJobs() );
                #else
                while( processDistributableJob(g, logMe) || (gotWaitingJob=getWaitingJob()) ){
                    if( gotWaitingJob ){
                        if( logMe.logging( jobLogger::jobDoing ) )
                            logMe.push(idS+"=====> doing WJ"+std::to_string(j->getJobId()));
                        doJ(logMe);
                        gotWaitingJob = false;
                    }
                }
                #endif
                if( doneNewJob ){
                    doneNewJob = false;
                    g.beAvailable(id, logMe);
                    if( logMe.logging( jobLogger::workerAvailable ) )
                        logMe.push(idS+"=====> is available");
                }
                if( logMe.logging( jobLogger::jobSummary ) )
                    logMe.push(printData( logMe.logging( jobLogger::jobDependency ) )); 
            } while( !g.closed() );
            if( logMe.logging( jobLogger::queueFull ) )
                logMe.push(idS+"=====> sees queue FULL");
        }
    }
public:
    explicit genericWorker(int id): id(id), idS("W"+std::to_string(id)), busy(ATOMIC_FLAG_INIT), j(nullptr), WJ(), DJ(), CJ() {}

    genericWorker& operator=( genericWorker& ) = delete;
    genericWorker& operator=( genericWorker const& ) = delete;

    auto trim(){
        while( !WJ.empty() ){
            if( WJ.front()==nullptr )
                WJ.pop_front();
            else if( WJ.front()->completed() ){
                CJ.push_back(WJ.front());
                WJ.pop_front();
            }
            else
                break;
        }
        while( !WJ.empty() ){
            if( WJ.back()==nullptr )
                WJ.pop_back();
            else if( WJ.back()->completed() ){
                CJ.push_back(WJ.back());
                WJ.pop_back();
            }
            else
                break;
        }
        return WJ.size();
    }

    bool start(group& g){
        if( busy.test_and_set(std::memory_order_acquire) )
            return false;

    /* I have some doubts about the fact that the compiler could optimize away  */
    /* the waiting function, so I am doing it in the following strange way...   */
    /*  iLikeUSUR();    */
        if( !g.waitReady() )
            return start(g);
    /*  iLikeUSUR();    */
        startCycle(g);

        busy.clear(std::memory_order_release);
        return true;
    }

    std::pair<bool, std::chrono::duration<double>> startWthChrono(group& g){
        if( busy.test_and_set(std::memory_order_acquire) ){
            auto timeDummy = std::chrono::high_resolution_clock::now();
            return std::make_pair(false, timeDummy-timeDummy);
        }

        if( !g.waitReady() )
            return startWthChrono(g);

        auto timeBegin = std::chrono::high_resolution_clock::now();
        startCycle(g);
        auto timeEnd = std::chrono::high_resolution_clock::now();

        return std::make_pair(true, timeEnd-timeBegin);
    }
    
    bool startL(group& g, jobLogger& logMe){
        if( busy.test_and_set(std::memory_order_acquire) )
            return false;

        if( !g.waitReady() )
            return startL(g, logMe);

        startLCycle(g, logMe);

        busy.clear(std::memory_order_release);
        return true;
    }   

    std::pair<bool, std::chrono::duration<double>> startLWthChrono(group& g, jobLogger& logMe){
        if( busy.test_and_set(std::memory_order_acquire) ){
            auto timeDummy = std::chrono::high_resolution_clock::now();
            return std::make_pair(false, timeDummy-timeDummy);
        }

        if( !g.waitReady() )
            return startLWthChrono(g, logMe);

        auto timeBegin = std::chrono::high_resolution_clock::now();
        startLCycle(g, logMe);
        auto timeEnd = std::chrono::high_resolution_clock::now();

        busy.clear(std::memory_order_release);
        return std::make_pair(true, timeEnd-timeBegin);
    }   
    
    ~genericWorker(){
        for( auto &jj: CJ )
            if( jj!=nullptr )
                delete jj;
        for( auto &jj: WJ )
            if( jj!=nullptr )
                delete jj;
        for( auto &jj: DJ )
            if( jj!=nullptr )
                delete jj;
    }

    std::vector<std::string> printData(bool showDependencies=false){
        std::vector<std::string> V;
        V.emplace_back(idS+"=====> CJ: "+std::to_string(CJ.size())+" --- WJ: "+std::to_string(WJ.size())+"--- DJ: "+std::to_string(DJ.size()));
        if( showDependencies ){
            for( auto &wj: WJ ){
                std::string s;
                if( wj!=nullptr ){
                        for( auto &d: wj->getDependencies() )
                            s+=std::to_string(static_cast<int64_t>(d))+' ';
                        if( !s.empty() )
                            V.emplace_back(idS+" :: "+std::to_string(wj->getJobId())+" <- "+s);
                }
            }
        }
        return V;
    }
};

/* ACHTUNG: I am NOT considering the possible sharing of the same job   */
/* or of the same worker across different workgroups here! Be aware!!   */
/* Could be interesting to implement it... not here, not now.           */

#ifdef CIRCULAR_QUEUE

class workgroup{
public: 
    typedef genericWorker<workgroup> worker;
private:
    bool done;
    const int n;
    std::atomic<bool> ready;                    /* Start when everything is set up  */
    std::atomic_flag working;                   /* Avoid starting twice!            */
    std::vector<std::thread> T;                 /* Thread handlers, 0 is unused     */
    std::vector<std::atomic<job*>> J;           /* J[id] : Job for thread id        */
    std::deque<worker> W;                       /* Workers                          */
    circleQueue<int> A;                         /* Available Threads                */
    std::vector<std::atomic_flag> AA;           /* Already Available                */
    job *startingJob;
    std::chrono::duration<double> lastDuration;
public:
    explicit workgroup(int n) : done{false}, n(n), ready(false), working(ATOMIC_FLAG_INIT), T(n), J(n), W(), A(n, -1), AA(n) {
        for( int i=0; i<n; ++i ){
            W.emplace_back(i);
            J[i].store(nullptr);
            AA[i].clear();
        }
    }

    bool start(job* const j, jobLogger& logMe, bool wthChrono = false){
        if( !working.test_and_set(std::memory_order_acquire) ){     /* Acquire: see hereafter...    */
            startingJob = j;        

            J[0].store(j, std::memory_order_relaxed);
            AA[0].clear(std::memory_order_relaxed);
            for( int i=1; i<n; ++i ){
                AA[i].test_and_set(std::memory_order_relaxed);
                A.push(i);
                T[i] = std::thread(&worker::startL, std::ref(W[i]), std::ref(*this), std::ref(logMe));
            }
            ready.store(true, std::memory_order_release);   /* Sync Release point   */
            if( wthChrono ){
                lastDuration = W[0].startLWthChrono(*this, logMe).second;
                logMe.push("==============  COMPLETED IN "+std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(lastDuration).count())+"ms  ==============");
            }
            else
                W[0].startL(*this, logMe);

            for( int i=0; i<n; ++i ){
                J[i].store(nullptr, std::memory_order_relaxed);
                AA[i].clear(std::memory_order_relaxed);
                if( T[i].joinable() )
                    T[i].join();
            }
            ready.store(false, std::memory_order_relaxed);
            working.clear(std::memory_order_release);       /* Sync Release point   */
            return true;
        }
        return false;
    }

    bool start(job* const j, bool wthChrono = false){
        if( !working.test_and_set(std::memory_order_acquire) ){     /* Acquire: see hereafter...    */
            startingJob = j;        

            J[0].store(j, std::memory_order_relaxed);
            AA[0].clear(std::memory_order_relaxed);
            for( int i=1; i<n; ++i ){
                AA[i].test_and_set(std::memory_order_relaxed);
                A.push(i);
                T[i] = std::thread(&worker::start, std::ref(W[i]), std::ref(*this));
            }
            ready.store(true, std::memory_order_release);   /* Sync Release point   */
            if( wthChrono )
                lastDuration = W[0].startWthChrono(*this).second;
            else
                W[0].start(*this);

            for( int i=0; i<n; ++i ){
                J[i].store(nullptr, std::memory_order_relaxed);
                AA[i].clear(std::memory_order_relaxed);
                if( T[i].joinable() )
                    T[i].join();
            }
            ready.store(false, std::memory_order_relaxed);
            working.clear(std::memory_order_release);       /* Sync Release point   */
            return true;
        }
        return false;
    }

    void beAvailable(const int& id, jobLogger &logMe){
        if( !logMe.logging( jobLogger::queueMoves ) ){
            beAvailable(id);
            return;
        }

        if(!AA[id].test_and_set(std::memory_order_acquire)){
            auto Qdim(A.push(id));
            logMe.push(std::to_string(id)+" is in QUEUE, |Q|="+std::to_string(Qdim));
            if( logMe.logging( jobLogger::jobSummaryForQueueMoves ) )
                for( auto &w: W )
                    logMe.push(w.printData());
        } else {
            logMe.push(std::to_string(id)+" was already in QUEUE");
        }
    }

    void beAvailable(const int& id){
        if(!AA[id].test_and_set(std::memory_order_acquire))
            A.push(id);
    }

    /* If only each thread is asking for its own id you can avoid atomic for AA     */
    bool lookForJob(const int& id, job *& j){
        if( (j=J[id].exchange(nullptr, std::memory_order_acquire)) != nullptr ){
            AA[id].clear(std::memory_order_release);
            return true;
        }
        return false;
    }

    void waitForJob(const int& id, job *& j){
        while( (j = J[id].exchange(nullptr, std::memory_order_acquire)) == nullptr );
        AA[id].clear(std::memory_order_release);
    }

    bool offerJob(job* const j){
        int id;
        if(A.pop(id)){
            J[id].store(j, std::memory_order_release);      /* relaxed...   */
            return true;
        } else
            return false;
    }

    bool closed(){          /* return possibly closed, unnecessary but useful    */
        return A.full();
    }

    bool isCompleted(){
        if( startingJob!=nullptr )
            return startingJob->completed();
        else{
/*DBG*/     std::cout << "Warning: starting job appearing undefined!" << std::endl;
            return true;
        }
    }

    std::vector<std::string> printData(){
        std::vector<std::string> S;
        for( auto &w: W )
            for( auto &s: w.printData() )
                S.emplace_back(s);
        return S;
    }

    bool waitReady(){
        while( !ready.load(std::memory_order_acquire) ){
            std::this_thread::yield();
        }
        return ready.load(std::memory_order_acquire);
    }

    bool isReady(){
        return ready.load(std::memory_order_acquire);
    }

    std::chrono::duration<double> getDuration(){
        return lastDuration;
    }

    ~workgroup(){
        job *j;
        for( int i=0; i<static_cast<int>(J.size()); ++i )
            if( (j = J[i].exchange(nullptr, std::memory_order_relaxed)) != nullptr ){
                std::cout << "Warning: uncompleted processes!" << std::endl;
                delete j;
            }
        for( int i=0; i<static_cast<int>(T.size()); ++i )
            if( T[i].joinable() )
                T[i].join();
    }
};

#else

class workgroup{
public: 
    typedef genericWorker<workgroup> worker;
private:
    bool done;
    const int n;
    std::atomic<bool> ready;                    /* Start when everything is set up  */
    std::atomic_flag working;                   /* Avoid starting twice!            */
    std::vector<std::thread> T;                 /* Thread handlers, 0 is unused     */
    std::vector<std::atomic<job*>> J;           /* J[id] : Job for thread id        */
    std::deque<worker> W;                       /* Workers                          */
    std::vector<std::atomic<bool>> A;           /* Available                */
    std::atomic<int> d;
    job *startingJob;
    std::chrono::duration<double> lastDuration;
public:
    explicit workgroup(int n) : done{false}, n(n), ready(false), working(ATOMIC_FLAG_INIT), T(n), J(n), W(), A(n), d(0) {
        for( int i=0; i<n; ++i ){
            W.emplace_back(i);
            J[i].store(nullptr);
            std::atomic_init(&A[i], false);
        }
    }

    bool start(job* const j, jobLogger& logMe, bool wthChrono = false){
        if( !working.test_and_set(std::memory_order_acquire) ){     /* Acquire: see hereafter...    */
            startingJob = j;        

            J[0].store(j, std::memory_order_relaxed);
            A[0].store(false, std::memory_order_relaxed);
            d.store(n-1, std::memory_order_relaxed);
            for( int i=1; i<n; ++i ){
                A[i].store(true, std::memory_order_relaxed);
                T[i] = std::thread(&worker::startL, std::ref(W[i]), std::ref(*this), std::ref(logMe));
            }
            ready.store(true, std::memory_order_release);   /* Sync Release point   */
            if( wthChrono ){
                lastDuration = W[0].startLWthChrono(*this, logMe).second;
                logMe.push("==============  COMPLETED IN "+std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(lastDuration).count())+"ms  ==============");
            }
            else
                W[0].startL(*this, logMe);

            for( int i=0; i<n; ++i ){
                J[i].store(nullptr, std::memory_order_relaxed);
                A[i].store(false, std::memory_order_relaxed);
                if( T[i].joinable() )
                    T[i].join();
            }
            ready.store(false, std::memory_order_relaxed);
            working.clear(std::memory_order_release);       /* Sync Release point   */
            return true;
        }
        return false;
    }

    bool start(job* const j, bool wthChrono = false){
        if( !working.test_and_set(std::memory_order_acquire) ){     /* Acquire: see hereafter...    */
            startingJob = j;        

            J[0].store(j, std::memory_order_relaxed);
            A[0].store(false, std::memory_order_relaxed);
            d.store(n-1, std::memory_order_relaxed);
            for( int i=1; i<n; ++i ){
                A[i].store(true, std::memory_order_relaxed);
                T[i] = std::thread(&worker::start, std::ref(W[i]), std::ref(*this));
            }
            ready.store(true, std::memory_order_release);   /* Sync Release point   */
            if( wthChrono )
                lastDuration = W[0].startWthChrono(*this).second;
            else
                W[0].start(*this);

            for( int i=0; i<n; ++i ){
                J[i].store(nullptr, std::memory_order_relaxed);
                A[i].store(false, std::memory_order_relaxed);
                if( T[i].joinable() )
                    T[i].join();
            }
            ready.store(false, std::memory_order_relaxed);
            working.clear(std::memory_order_release);       /* Sync Release point   */
            return true;
        }
        return false;
    }

    void beAvailable(const int& id, jobLogger &logMe){
        if( !logMe.logging( jobLogger::queueMoves ) ){
            beAvailable(id);
            return;
        }

        if(!A[id].exchange(true, std::memory_order_relaxed)){
            auto Qdim{d.fetch_add(1, std::memory_order_relaxed)+1};
            logMe.push(std::to_string(id)+" is among the AVAILABLES, |A|="+std::to_string(Qdim));
            if( logMe.logging( jobLogger::jobSummaryForQueueMoves ) )
                for( auto &w: W )
                    logMe.push(w.printData());
        } else {
            logMe.push(std::to_string(id)+" was already among the AVAILABLES");
        }
    }

    void beAvailable(const int& id){
        if(!A[id].exchange(true, std::memory_order_relaxed))
            d.fetch_add(1, std::memory_order_relaxed);
    }

    bool lookForJob(const int& id, job *& j){
        if( (j=J[id].exchange(nullptr, std::memory_order_acquire)) != nullptr )
            return true;
        return false;
    }

    void waitForJob(const int& id, job *& j){
        while( (j = J[id].exchange(nullptr, std::memory_order_acquire)) == nullptr );
    }

    bool offerJob(job* const j){
        for( int id=0; id<n; ++id )
            if( A[id].exchange(false, std::memory_order_relaxed) ){
                d.fetch_sub(1, std::memory_order_relaxed);
                J[id].store(j, std::memory_order_release);
                return true;
            }
        return false;
    }

    bool closed(){          /* return possibly closed, unnecessary but useful    */
        return d.load(std::memory_order_relaxed)==n;
    }

    bool isCompleted(){
        if( startingJob!=nullptr )
            return startingJob->completed();
        else{
/*DBG*/     std::cout << "Warning: starting job appearing undefined!" << std::endl;
            return true;
        }
    }

    std::vector<std::string> printData(){
        std::vector<std::string> S;
        for( auto &w: W )
            for( auto &s: w.printData() )
                S.emplace_back(s);
        return S;
    }

    bool waitReady(){
        while( !ready.load(std::memory_order_acquire) ){
            std::this_thread::yield();
        }
        return ready.load(std::memory_order_acquire);
    }

    bool isReady(){
        return ready.load(std::memory_order_acquire);
    }

    std::chrono::duration<double> getDuration(){
        return lastDuration;
    }

    ~workgroup(){
        job *j;
        for( int i=0; i<static_cast<int>(J.size()); ++i )
            if( (j = J[i].exchange(nullptr, std::memory_order_relaxed)) != nullptr ){
                std::cout << "Warning: uncompleted processes!" << std::endl;
                delete j;
            }
        for( int i=0; i<static_cast<int>(T.size()); ++i )
            if( T[i].joinable() )
                T[i].join();
    }
};

#endif


using namespace std::placeholders;

template <class typeIn, class typeOut>
class setTest{
public:
    typedef std::function<typeOut(const typeIn&)> funcType;
private:
    funcType seqFunc;
    job *parJob;
    std::chrono::duration<double> lastDuration;
public:
    setTest(funcType seq, job *par) : seqFunc(seq), parJob(par) {}

    typeOut start(const typeIn& in, int nThreads, jobLogger* plogMe, bool wthChrono = false){
        if(nThreads<1){
            if( !wthChrono )
                return seqFunc(in);
            else{
                auto timeBegin = std::chrono::high_resolution_clock::now();
                auto V(seqFunc(in)); 
                auto timeEnd = std::chrono::high_resolution_clock::now();
                lastDuration = timeEnd-timeBegin;
                if( plogMe )
                    plogMe->push("==============  COMPLETED IN "+std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(lastDuration).count())+"ms  ==============");
                return V;
            }
        }

        workgroup wg(nThreads);
        if( plogMe )
            wg.start(parJob, *plogMe, wthChrono);
        else 
            wg.start(parJob, wthChrono);
        if( wthChrono )
            lastDuration = wg.getDuration();
        for( auto &s: wg.printData() )
            std::cout << s << '\n';
        std::cout << std::flush;

        return *reinterpret_cast<typeOut*>(parJob->getOutput());
    }

    std::chrono::duration<double> getDuration(){
        return lastDuration;
    }
};

template <class typeIn, class typeOut>
class genericJob : public job{
    bool ownOutput{false};
    std::atomic<bool> completedMine{false}, completedAll{false};
    uint64_t myJobId;           //not const: structure can be recycled?
public:
    std::vector<job *> PJ;      //pending jobs: distributable jobs, but not inputReady on construction
    std::vector<job *> DJ;      //distributable jobs, ready to go 
    std::vector<job *> IJ;      //input jobs: waiting for their input
    std::vector<job *> OJ;      //output jobs: result will be ready when they are completed
private:
    std::function<bool()> f;
public:
    typedef typeIn typeInput;
    typedef typeOut typeOutput;
    typeIn in;
    typeOut out;
    genericJob( std::function<bool(genericJob<typeIn, typeOut>&)> ff, std::vector<job *> &&_IJ) 
    : myJobId{jobId.fetch_add(1, std::memory_order_relaxed)}, f(std::bind(ff, std::ref(*this))) , IJ(std::move(_IJ))
    {    }
    genericJob( std::function<bool(genericJob<typeIn, typeOut>&)> ff, 
        std::vector<job *> &&_IJ, typeIn in ) 
    : myJobId{jobId.fetch_add(1, std::memory_order_relaxed)}, f(std::bind(ff, std::ref(*this))), in(in), IJ(std::move(_IJ)) 
    {    }
    genericJob( std::function<bool(genericJob<typeIn, typeOut>&)> ff, std::vector<job *> &_IJ) 
    : myJobId{jobId.fetch_add(1, std::memory_order_relaxed)}, f(std::bind(ff, std::ref(*this))) , IJ(_IJ)
    {    }
    genericJob( std::function<bool(genericJob<typeIn, typeOut>&)> ff, 
        std::vector<job *> &_IJ, typeIn in ) 
    : myJobId{jobId.fetch_add(1, std::memory_order_relaxed)}, f(std::bind(ff, std::ref(*this))), in(in), IJ(_IJ) 
    {    }

    virtual void operator()() {
        completedMine.store(f(), std::memory_order_release);
    }

    virtual bool completed() { 
        if( completedAll.load(std::memory_order_acquire) )
            return true;
        if( completedMine.load(std::memory_order_acquire) ){
            for( auto &oj: OJ )
                if( !oj->completed() )
                    return false;
            completedAll.store(true, std::memory_order_release);
            return true;
        }
        return false; 
    }

    virtual bool inputReady() { 
        for( auto &ij: IJ )
            if( !ij->completed() )
                return false; 
    }
// if more than one worker is asking for... use atomic counters!
    virtual bool getPendingJob(job*& j) { 
        if(PJ.empty())
            return false;
        j=PJ.back();
        PJ.pop_back();
        
        return true;
    }

    virtual bool getDistributableJob(job*& j) { 
        if(DJ.empty())
            return false;
        j=DJ.back();
        DJ.pop_back();
        
        return true;
    }

    virtual void* getOutput() { 
        return &out; 
    } 

    virtual uint64_t getJobId() { 
        return myJobId; 
    }

    virtual ~genericJob() {
    }
};


namespace divImp{
using namespace std::placeholders;

template <class typeOut, class typeIn>
class recDivideJob: public job{
    typeIn in;
    const uint64_t myJobId;
    volatile std::atomic<bool> complete;
    int iPJ{0};
    std::vector<job *> PJ;          /* Pending Job          */
    std::vector<job *> DJ;          /* Distributable Jobs   */
    std::function<bool()> f;
public:
    recDivideJob(workgroup& g, std::function<bool(std::vector<job*>&, std::vector<job*>&, workgroup&, const typeIn&, typeOut*&)> ff, const typeIn& iin) 
    : in(iin), myJobId{ jobId.fetch_add(1, std::memory_order_relaxed) }, complete(false), PJ(), DJ(), f( std::bind(ff, std::ref(PJ), std::ref(DJ), std::ref(g), std::cref(in), std::ref(pout)) )
    {}

    void operator()() override {
        complete.store(f(), std::memory_order_release);
    }

    bool completed() override {
        if(!complete.load(std::memory_order_acquire))
            return false;
        for( auto &pj: PJ )
            if( !pj->completed() )
                return false;
        return true;
    }

    bool inputReady() override {
        return true;
    }

    bool getPendingJob(job*& j) override {
        if(iPJ < static_cast<int>(PJ.size())){
            j = PJ[iPJ++];
            return true;
        }
        return false;
    }

    std::vector<uint64_t> getDependencies() override {
        std::vector<uint64_t> V;
        for( auto j: PJ ){
            if( j!=nullptr ){
                if( j->completed() )
                    V.push_back( -j->getJobId() );
                else
                    V.push_back( j->getJobId() );
            }
        }
        return V;
    }

    bool getDistributableJob(job*& j) override {
        if(!DJ.empty()){
            j = DJ.back();
            DJ.pop_back();
            return true;
        }
        return false;
    }

    void* getOutput() override {
        return pout;
    }

    uint64_t getJobId() override {
        return myJobId;
    }

    typeOut *pout{nullptr};

    ~recDivideJob() override {
        if(pout)
            delete pout;
    }
};

template <class typeOut>
class imperaJob: public job{
    const uint64_t myJobId;
    typeOut*& poutput;
    std::atomic<bool> complete;
    std::vector<job *> IJ;                      /* Input Jobs   */
    std::vector<typeOut> I;                     /* Input        */
    std::function<bool()> f;
public:
    /* impera should write result in the out of the respective recDivide job... */
    imperaJob(std::vector<job*>&& IIJ , std::function<bool(const std::vector<typeOut>&, typeOut*&)> ff, typeOut*& pout) 
    : myJobId{jobId.fetch_add(1, std::memory_order_relaxed)}, poutput(pout), complete(false), IJ(IIJ), I(), f( std::bind(ff, std::cref(I),  std::ref(pout) ) )
    {}
    /* WARNING: the check for ready input is external!  */
    void operator()() override {
        for( auto &ij: IJ )
            I.emplace_back(*static_cast<typeOut*>(ij->getOutput()));
        complete.store(f(), std::memory_order_release);
    }

    bool completed() override {
        return complete.load(std::memory_order_acquire);
    }

    bool inputReady() override {
        for( auto &ij: IJ )
            if( !ij->completed() )
                return false;
        return true;
    }

    bool getPendingJob(job*&) override {
        return false;
    }

    std::vector<uint64_t> getDependencies() override {
        std::vector<uint64_t> V;
        for( auto j: IJ ){
            if( j!=nullptr )
                V.push_back( j->getJobId() );
        }
        return V;
    }

    bool getDistributableJob(job*& j) override {
        return false;
    }

    void *getOutput() override {
        return poutput;
    }

    uint64_t getJobId() override {
        return myJobId;
    }

    ~imperaJob() {}
};

template <class typeIn, class typeOut>
class divImp{
    std::function<std::vector<typeIn>(const typeIn&)> divide;
    std::function<typeOut(const std::vector<typeOut>&)> impera;
    std::function<typeOut(const typeIn&)> base;
    std::function<bool(const typeIn&)> isBase;
    std::chrono::duration<double> lastDuration;

    typeOut recDivide(const typeIn& in){
        if( isBase(std::cref(in)) )
            return base(std::cref(in));

        std::vector<typeOut> V;
        for( auto &i: divide(std::cref(in)) )
            V.emplace_back(recDivide(i));

        return impera(std::cref(V));
    }

    bool recDivideFun(std::vector<job *>& PJ, std::vector<job *>& DJ, workgroup& g, const typeIn& in, typeOut *&pout){
        if( isBase(std::cref(in)) ){
            pout = new typeOut(base(std::cref(in)));
            return true;
        }

        if( in.depth < typeIn::threshold ){
            pout = new typeOut(recDivide(in));
            return true;
        }

        auto setDistributableJobs = [&]() -> std::vector<job*> {
            std::vector<job*> J;
            for( auto &i: divide(std::cref(in)) ){
                auto *j (newRecDivideJob(g, i));
                DJ.push_back(j);
                J.push_back(j);
            }
            return J;
        };

        PJ.push_back(newImperaJob(setDistributableJobs(), pout));

        return true;
    } 

    bool imperaFun(const std::vector<typeOut>& Vout, typeOut*& pout){
        pout = new typeOut(impera(std::cref(Vout)));

        return true;
    }

    job *newRecDivideJob(workgroup& g, const typeIn& in){
        std::function<bool(std::vector<job*>&, std::vector<job*>&, workgroup&, const typeIn&, typeOut*&)> ff 
            = std::bind( &divImp::recDivideFun, this, _1, _2, _3, _4, _5 );
        return new recDivideJob<typeOut, typeIn>(g, ff, in);
    }

    job *newImperaJob(std::vector<job*>&& IJ, typeOut*& pout){
        std::function<bool(const std::vector<typeOut>&, typeOut*&)> ff 
            = std::bind( &divImp::imperaFun, this, _1, _2 );
        return new imperaJob<typeOut>(std::move(IJ), ff, pout);
    }
public:
    typeOut start(const typeIn& in, int n, jobLogger* plogMe, bool wthChrono = false){
        if(n<1){
            if( !wthChrono )
                return recDivide(in);
            else{
                auto timeBegin = std::chrono::high_resolution_clock::now();
                auto V(recDivide(in)); 
                auto timeEnd = std::chrono::high_resolution_clock::now();
                lastDuration = timeEnd-timeBegin;
                if( plogMe )
                    plogMe->push("==============  COMPLETED IN "+std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(lastDuration).count())+"ms  ==============");
                return V;
            }
        }

        
        workgroup wg(n);
        job *startingJob(newRecDivideJob(wg, in));
        if( plogMe )
            wg.start(startingJob, *plogMe, wthChrono);
        else 
            wg.start(startingJob, wthChrono);
        if( wthChrono )
            lastDuration = wg.getDuration();
        for( auto &s: wg.printData() )
            std::cout << s << '\n';
        std::cout << std::flush;

        return *reinterpret_cast<typeOut*>(startingJob->getOutput());
    }

    divImp(auto &divide, auto &impera, auto &base, auto &isBase)
    : divide(divide), impera(impera), base(base), isBase(isBase) 
    {}

    std::chrono::duration<double> getDuration(){
        return lastDuration;
    }
};
};

/* brief extension to std for parsing vectors... 	*/
namespace std{
template <class T>
string to_string(const vector<T>& V ){
    string s="[";
    if( !V.empty() ){
        for( int i=0; i<V.size()-1; ++i )
            s+=to_string(V[i])+',';
        s+=to_string(V.back());
    }
    s+="]";

    return s;
}
};

/*
namespace std{

template <class T>
vector<T> operator-(const vector<T>& V, const vector<T>& W){
	auto m{min(V.size(), W.size())}, M{max(V.size(), W.size())};
	
	vector<T> R(M);
	for( auto i=0; i<m; ++i )
		R[i]=V[i]-W[i];
	for( auto i=m; i<V.size(); ++i )
		R[i]=V[i];
	for( auto i=m; i<W.size(); ++i )
		R[i]=-W[i];

	return R;
}

template <class T>
vector<T> operator+(const vector<T>& V, const vector<T>& W){
	auto m{min(V.size(), W.size())}, M{max(V.size(), W.size())};
	
	vector<T> R(M);
	for( auto i=0; i<m; ++i )
		R[i]=V[i]+W[i];
	for( auto i=m; i<V.size(); ++i )
		R[i]=V[i];
	for( auto i=m; i<W.size(); ++i )
		R[i]=W[i];

	return R;
}

template <class T>
vector<T>& operator+=(vector<T>& V, const vector<T>& W){
	auto m{min(V.size(), W.size())}, M{max(V.size(), W.size())};
	
	V.resize(M);
	for( auto i=0; i<m; ++i )
		V[i]+=W[i];
	for( auto i=m; i<W.size(); ++i )
		V[i]=W[i];

	return V;
}

template <class T>
vector<T>& operator-=(vector<T>& V, const vector<T>& W){
	auto m{min(V.size(), W.size())}, M{max(V.size(), W.size())};
	
	V.resize(M);
	for( auto i=0; i<m; ++i )
		V[i]-=W[i];
	for( auto i=m; i<W.size(); ++i )
		V[i]=-W[i];

	return V;
}

template <class T>
vector<T>& operator++(vector<T>& V){
	for( auto &v: V )
		++v;
	return V;
}

template <class T>
vector<T>& operator--(vector<T>& V){
	for( auto &v: V )
		--v;
	return V;
}
};

*/

namespace mergeSort {
/* License : Michele Miccinesi 2018 -       */
/* Mergesort components                     */


template <typename T>
struct subVector{
    static int threshold;
    const int depth;
    subVector(int depth, std::vector<T>& V, std::vector<T>& W, const int& i, const int &j) : depth(depth), V(V), W(W), i(i), j(j) {}
    std::vector<T>& V, &W;
    int i, j;
};

template <>
int subVector<int>::threshold = -10;

template <class T>
auto divide = [](const subVector<T>& Vin) -> std::vector<subVector<T>>{
    std::vector<subVector<T>> Vout;
    Vout.emplace_back(Vin.depth-1, Vin.V, Vin.W, Vin.i, (Vin.i+Vin.j)/2);
    Vout.emplace_back(Vin.depth-1, Vin.V, Vin.W, (Vin.i+Vin.j)/2+1, Vin.j);
    return Vout;
};

template <class T>
auto impera = [](const std::vector<subVector<T>>& Vin) -> subVector<T>{
    subVector<T> Vout (Vin[0].depth+1, Vin[0].W, Vin[0].V, Vin[0].i, Vin[1].j);
    int ii[] = { Vin[0].i, Vin[1].i };
    int i { Vin[0].i };  
    while( true ){
        bool b{Vin[0].V[ii[0]]>Vin[1].V[ii[1]]};
        bool nb=!b;
        Vout.V[i++]=Vin[b].V[ii[b]++];
        if( ii[b]>Vin[b].j ){
            while( ii[nb]<=Vin[nb].j )
                Vout.V[i++]=Vin[nb].V[ii[nb]++];
            break;
        }
    }

    return Vout;
};

template <class T>
auto base = [](const subVector<T>& Vin) -> subVector<T>{
    subVector<T> Vout{Vin.depth, Vin.W, Vin.V, Vin.i, Vin.j};
    switch(Vin.j-Vin.i){
    case 1:
        if( Vin.V[Vin.i]>Vin.V[Vin.j] ){
            Vout.V[Vin.i]=Vin.V[Vin.j];
            Vout.V[Vin.j]=Vin.V[Vin.i];
        } else {
            Vout.V[Vin.i]=Vin.V[Vin.i];
            Vout.V[Vin.j]=Vin.V[Vin.j];
        }
        break;
    case 0:
        Vout.V[Vin.i]=Vin.V[Vin.i];
        break;
    } 
    return Vout;
};

template <class T>
auto isBase = [](const subVector<T>& V) -> bool{
    return V.j-V.i<2;
};
}
namespace unbMergeSort {
/* License: Michele Miccinesi 2018  */
/* Unbalanced Mergesort Components  */

/* Notice that alpha and beta MUST be both at least 1   */
unsigned int alpha{1}, beta{1};

template <typename T>
struct subVector{
    static int threshold;
    const int depth;
    subVector(std::vector<T>& V, std::vector<T>& W, const int& i, const int &j) : depth(j-i+1), V(V), W(W), i(i), j(j) {}
    std::vector<T>& V, &W;
    int i, j;
};

template <>
int subVector<int>::threshold = 0;

template <class T>
auto divide = [](const subVector<T>& Vin) -> std::vector<subVector<T>>{
    std::vector<subVector<T>> Vout;
    Vout.emplace_back(Vin.V, Vin.W, Vin.i, (alpha*Vin.i+beta*Vin.j)/(alpha+beta));
    Vout.emplace_back(Vin.V, Vin.W, (alpha*Vin.i+beta*Vin.j)/(alpha+beta)+1, Vin.j);
    return Vout;
};

template <class T>
auto impera = [](const std::vector<subVector<T>>& Vin) -> subVector<T>{
    subVector<T> Vout(Vin[0].W, Vin[0].V, Vin[0].i, Vin[1].j);
    int ii[] = { Vin[0].i, Vin[1].i };
    int i { Vin[0].i };  
    while( true ){
        bool b{ Vin[0].V[ii[0]]>Vin[1].V[ii[1]] };
        bool nb = !b;
        Vout.V[i++] = Vin[b].V[ii[b]++];
        if( ii[b] > Vin[b].j ){
            while( ii[nb] <= Vin[nb].j )
                Vout.V[i++] = Vin[nb].V[ii[nb]++];
            break;
        }
    }

    return Vout;
};

template <class T>
auto base = [](const subVector<T>& Vin) -> subVector<T>{
    subVector<T> Vout{Vin.W, Vin.V, Vin.i, Vin.j};
    switch(Vin.j-Vin.i){
    case 1:
        if( Vin.V[Vin.i]>Vin.V[Vin.j] ){
            Vout.V[Vin.i]=Vin.V[Vin.j];
            Vout.V[Vin.j]=Vin.V[Vin.i];
        } else {
            Vout.V[Vin.i]=Vin.V[Vin.i];
            Vout.V[Vin.j]=Vin.V[Vin.j];
        }
        break;
    case 0:
        Vout.V[Vin.i]=Vin.V[Vin.i];
        break;
    } 
    return Vout;
};

template <class T>
auto isBase = [](const subVector<T>& V) -> bool{
    return V.j-V.i<2;
};
}
namespace quickSort {
/* License : Michele Miccinesi 2018 -       */
/* Quicksort components                     */

template <typename T>
struct subVector{
    static int threshold;
    const int depth;
    subVector(std::vector<T>& V, const int& i, const int &j) : depth(j-i+1), V(V), i(i), j(j) {}
    std::vector<T>& V;
    int i, j;
};

template <>
int subVector<int>::threshold = 0;

template <class T>
auto divide = [](const subVector<T>& Vin) -> std::vector<subVector<T>>{
    std::vector<subVector<T>> Vout;
    
    int i{Vin.i}, j{Vin.j}, k{Vin.j};
    const T pivot(Vin.V[j]);
    while( i<j ){
        if( Vin.V[i]>pivot ){
            std::swap(Vin.V[--j], Vin.V[i]);
        } 
        else if( Vin.V[i]==pivot ){
            Vin.V[i]=Vin.V[--j];
            Vin.V[j]=Vin.V[--k];
            Vin.V[k]=pivot;
        }
        else{
            ++i;
        }
    }

    int e{Vin.j};
    Vout.emplace_back(Vin.V, Vin.i, j-1);
    Vout.emplace_back(Vin.V, e-k+j+1, e);
    
    for( k = std::max(k, e-k+j+1); k<=e; ++k, ++j )
        std::swap(Vin.V[j], Vin.V[k]);
    return Vout;
};

template <class T>
auto impera = [](const std::vector<subVector<T>>& Vin) -> subVector<T>{
    subVector<T> Vout(Vin[0].V, Vin[0].i, Vin[1].j);
    return Vout;
};

template <class T>
auto base = [](const subVector<T>& Vin) -> subVector<T>{
    subVector<T> Vout{Vin.V, Vin.i, Vin.j};
    if( Vout.j>Vout.i )
        if( Vout.V[Vout.i]>Vout.V[Vout.j] )
            std::swap(Vout.V[Vout.i], Vout.V[Vout.j]);

    return Vout;
};

template <class T>
auto isBase = [](const subVector<T>& V) -> bool{
    return V.j-V.i<2;
};
}
namespace pMergeSort {
/* License : Michele Miccinesi 2018 - 			*/
/* Mergesort with Parallel merge components 	*/
/* DBG */
template <class T>
bool checkOrder(std::vector<T>& V, int i,int j){
    for( int k=i+1; k<=j; ++k )
        if( V[k]<V[k-1] )
            return false;
    return true;
}

template <class T>
bool printVector(std::vector<T>& V, int i, int j){
    for( int k=i; k<=j; ++k )
        std::cout << V[k] << ' ';
}

/* WARNING: special cases are not covered because threshold MUST    */
/* prevent them in all real cases!                                  */
template <class T>
inline std::pair<int, int> binarySearchRange(const std::vector<T>& V, const int& i, const int &j, const int &k){
    T mid = V[j];
    std::pair<int, int> range(j, j);
    int l{i}, h;
    if( i<=j && j<=k ){
        h=j;
        while(l<h){
            int m{(l+h)>>1};
            if( V[m]!=mid )
                l=m+1;
            else
                h=m;
        }
        range.first = l;
        l=j, h=k;
        while(l<h){
            int m{(l+h+1)>>1};
            if( V[m]!=mid )
                h=m-1;
            else
                l=m;
        }
        range.second = l;
    } else {
        h = k;
        /* The FIRST >= mid */
        while(l<h){    
            int m{(l+h)>>1};
            if( V[m]<mid )
                l=m+1;
            else
                h=m;
        }
        {
            int m{(l+h)>>1};
            if( V[m]<mid )
                l=m+1;
            else
                h=m;
        }
        range.first = l;
        h = k;
        /* The LAST <= mid */
        while(l<h){
            int m{(l+h+1)>>1};
            if( V[m]>mid )
                h=m-1;
            else
                l=m;
        }
        {
            int m{(l+h+1)>>1};
            if( V[m]>mid )
                h=m-1;
            else
                l=m;
        }
        range.second = h;
    }
    return range;
}

template <class T>
inline std::pair<int, int> binarySearchRange(const std::vector<T>* V, const int& i, const int &j, const int &k){
    return binarySearchRange<T>(*V, i, j, k);
}

template <typename T>
struct subVector{
    static int threshold;
    int depth;
    subVector() {};
    subVector(int depth, std::vector<T>& V, std::vector<T>& W, const int& i, const int &j) : depth(depth), V(&V), W(&W), i(i), j(j) {}
    subVector(int depth, std::vector<T>* V, std::vector<T>* W, const int& i, const int &j) : depth(depth), V(V), W(W), i(i), j(j) {}
    std::vector<T>* V, *W;
    int i, j;
};

template <>
int subVector<int>::threshold = -10;

template <typename T>
struct subVectors{
    bool trivial;
    static int threshold;
    int depth;
    subVectors(std::vector<T>& V, std::vector<T>& W, const int& i1, const int &j1, const int &i2, const int &j2, const int& b, bool trivial = false) 
    : trivial(trivial), depth(j1+j2-i1-i2+2), V(&V), W(&W), b(b) {
        i[0]=i1, i[1]=i2;
        j[0]=j1, j[1]=j2;
    }
    subVectors(std::vector<T>* V, std::vector<T>* W, const int& i1, const int &j1, const int &i2, const int &j2, const int& b, bool trivial = false) 
    : trivial(trivial), depth(j1+j2-i1-i2+2), V(V), W(W), b(b) {
        i[0]=i1, i[1]=i2;
        j[0]=j1, j[1]=j2;
    }
    std::vector<T>* V, *W;
    int b, i[2], j[2];
};

template <typename T>
struct subVectors1{
    static int threshold;
    int depth;
    subVectors1(std::vector<T>& V, std::vector<T>& W, const int& i, const int& j, const int &b)
    : depth(j-i+1), V(&V), W(&W), b(b), i(i), j(j) {}
    subVectors1(std::vector<T>* V, std::vector<T>* W, const int& i, const int& j, const int &b)
    : depth(j-i+1), V(V), W(W), b(b), i(i), j(j) {}    
    std::vector<T>* V, *W;
    int b, i, j;
};

template <>
int subVectors<int>::threshold = (1<<13)-1;
template <>
int subVectors1<int>::threshold = 1<<13;

template <class T>
auto divide = [](const subVector<T>& Vin) -> std::vector<subVector<T>>{
    std::vector<subVector<T>> Vout;
    Vout.emplace_back(Vin.depth-1, Vin.V, Vin.W, Vin.i, (Vin.i+Vin.j)/2);
    Vout.emplace_back(Vin.depth-1, Vin.V, Vin.W, (Vin.i+Vin.j)/2+1, Vin.j);
    return Vout;
};

template <class T>
auto mergeDivide = [](const subVectors<T>& Vin) -> std::vector<subVectors<T>>{
    std::vector<subVectors<T>> Vout;

    int b = Vin.b;
    int length0{Vin.j[0]-Vin.i[0]+1}, length1{Vin.j[1]-Vin.i[1]+1};
    int m { length0<length1 ? (Vin.i[1]+Vin.j[1])>>1: (Vin.i[0]+Vin.j[0])>>1 };
    auto range0 {binarySearchRange(Vin.V, Vin.i[0], m, Vin.j[0])};
    auto range1 {binarySearchRange(Vin.V, Vin.i[1], m, Vin.j[1])};

    int length = range0.first-Vin.i[0] + range1.first-Vin.i[1];
    if( length )
        Vout.emplace_back(Vin.V, Vin.W, Vin.i[0], range0.first-1, Vin.i[1], range1.first-1, b, range0.first==Vin.i[0] || range1.first==Vin.i[1]);
    
    b += length;
    Vout.emplace_back(Vin.V, Vin.W, range0.first, range0.second, range1.first, range1.second, b, true);
    
    b += range0.second-range0.first + range1.second-range1.first+2;
    length = Vin.j[0]-range0.second + Vin.j[1]-range1.second;
    if( length )
        Vout.emplace_back(Vin.V, Vin.W, range0.second+1, Vin.j[0], range1.second+1, Vin.j[1], b, range0.second==Vin.j[0] || range1.second==Vin.j[1]);
    
    return Vout;
};

template <class T>
auto copyDivide = [](const subVectors1<T>& Vin) -> std::vector<subVectors1<T>> {
    std::vector<subVectors1<T>> Vout;
    int mid = (Vin.i+Vin.j)>>1;
    Vout.emplace_back(Vin.V, Vin.W, Vin.i, mid, Vin.b);
    Vout.emplace_back(Vin.V, Vin.W, mid+1, Vin.j, Vin.b+mid+1-Vin.i);
    return Vout;
};

template <class T>
auto impera = [](const std::vector<subVector<T>>& Vin) -> subVector<T>{
    subVector<T> Vout (Vin[0].depth+1, Vin[0].W, Vin[0].V, Vin[0].i, Vin[1].j);
    int ii[] = { Vin[0].i, Vin[1].i };
    int i { Vin[0].i };  
    while( true ){
        bool b{(*(Vin[0].V))[ii[0]]>(*(Vin[1].V))[ii[1]]};
        bool nb=!b;
        (*Vout.V)[i++]=(*(Vin[b].V))[ii[b]++];
        if( ii[b]>Vin[b].j ){
            while( ii[nb]<=Vin[nb].j )
                (*Vout.V)[i++]=(*(Vin[nb].V))[ii[nb]++];
            break;
        }
    }

    return Vout;
};

template <class T>
auto base = [](const subVector<T>& Vin) -> subVector<T>{
    subVector<T> Vout{Vin.depth, Vin.W, Vin.V, Vin.i, Vin.j};
    switch(Vin.j-Vin.i){
    case 1:
        if( (*Vin.V)[Vin.i]>(*Vin.V)[Vin.j] ){
            (*Vout.V)[Vin.i]=(*Vin.V)[Vin.j];
            (*Vout.V)[Vin.j]=(*Vin.V)[Vin.i];
        } else {
            (*Vout.V)[Vin.i]=(*Vin.V)[Vin.i];
            (*Vout.V)[Vin.j]=(*Vin.V)[Vin.j];
        }
        break;
    case 0:
        (*Vout.V)[Vin.i]=(*Vin.V)[Vin.i];
        break;
    } 
    return Vout;
};

template <class T>
auto mergeBase = [](const subVectors<T>& Vin) -> void {
    int ii[] = { Vin.i[0], Vin.i[1] };
    int i { Vin.b };  
    while( true ){
        bool b{(*Vin.V)[ii[0]]>(*Vin.V)[ii[1]]};
        bool nb=!b;
        (*Vin.W)[i++]=(*Vin.V)[ii[b]++];
        if( ii[b]>Vin.j[b] ){
            while( ii[nb]<=Vin.j[nb] )
                (*Vin.W)[i++]=(*Vin.V)[ii[nb]++];
            break;
        }
    }
};

template <class T>
auto copyBase = [](const subVectors1<T>& V) -> void{
    for( int ii=V.b, i=V.i; i<=V.j; ++i, ++ii )
        (*V.W)[ii] = (*V.V)[i];
};

template <class T>
auto isBase = [](const subVector<T>& V) -> bool{
    return V.j-V.i<2;
};

template <class T>
auto mergeIsBase = [](const subVectors<T>& V) -> bool {
    return V.j[0]+V.j[1]-V.i[0]-V.i[1] < subVectors<T>::threshold;
};

template <class T>
auto copyIsBase = [](const subVectors1<T>& V) -> bool {
    return V.j-V.i < subVectors1<T>::threshold;
};

template <class T>
std::function<subVector<T>(const subVector<T>&)> mergeSortRoutine = [](const subVector<T>& Vin) -> subVector<T>{
    if( isBase<T>(Vin) )
        return base<T>(Vin);

    std::vector<subVector<T>> V;
    for( auto &i: divide<T>(Vin) )
        V.emplace_back(mergeSortRoutine<T>(std::cref(i)));

    return impera<T>(V);
};

template <class T>
std::function<bool(genericJob<subVectors1<T>, bool>&)> pCopyRoutine = [](genericJob<subVectors1<T>, bool>& myJob) -> bool{
    if( copyIsBase<T>(myJob.in) ){
        copyBase<T>(myJob.in);
        return true;
    }

    for( auto &v: copyDivide<T>(myJob.in) ){
        auto *copyJob{new genericJob<subVectors1<T>, bool>(pCopyRoutine<T>, std::vector<job*>(), v)};
        myJob.OJ.emplace_back(copyJob);
        myJob.DJ.push_back(copyJob);
    }
    return true;
};

template <class T>
std::function<bool(genericJob<subVectors<T>, bool>&)> pImperaRoutine = [](genericJob<subVectors<T>, bool>& myJob) -> bool {
    if( mergeIsBase<T>(myJob.in) ){
        mergeBase<T>(myJob.in);
        return true;
    }

    for( auto &v: mergeDivide<T>(myJob.in) ){
        if( v.trivial ){
            if(v.j[0]>=v.i[0]){
                subVectors1 sv(v.V, v.W, v.i[0], v.j[0], v.b);
                auto *copyJob{new genericJob<subVectors1<T>, bool>(pCopyRoutine<T>, std::vector<job *>(), sv)};
                myJob.OJ.emplace_back(copyJob);
                myJob.DJ.push_back(copyJob);
            }
            if(v.j[1]>=v.i[1]){
                subVectors1 sv(v.V, v.W, v.i[1], v.j[1], v.b+v.j[0]-v.i[0]+1);
                auto *copyJob{new genericJob<subVectors1<T>, bool>(pCopyRoutine<T>, std::vector<job *>(), sv)};
                myJob.OJ.emplace_back(copyJob);
                myJob.DJ.push_back(copyJob);
            }
        }
        else{
            auto *mergeJob{new genericJob<subVectors<T>, bool>(pImperaRoutine<T>, std::vector<job *>(), v)};
            myJob.OJ.emplace_back(mergeJob);
            myJob.DJ.push_back(mergeJob);
        }
    }
    return true;
};

template <class T>
std::function<bool(genericJob<subVector<T>*,subVector<T>*>&)> imperaRoutine = [](genericJob<subVector<T>*,subVector<T>*>& myJob) -> bool {
    typedef subVector<T> typeIn;
//  typedef typename std::remove_reference<decltype(myJob)>::type jobType;
    myJob.out = myJob.in;

    typeIn *in[2] = {static_cast<typeIn *>(myJob.IJ[0]->getOutput()), static_cast<typeIn *>(myJob.IJ[1]->getOutput())};
    int i[2] = {in[0]->i, in[1]->i};
    int j[2] = {in[0]->j, in[1]->j};
    int b=i[0];

    if( j[1]-i[0]<subVectors<T>::threshold ){
        std::vector<subVector<T>> Vin { *in[0], *in[1] };
        *myJob.in = impera<T>(Vin);
        return true;
    }

    subVectors<T> V( in[0]->V, in[0]->W, i[0], j[0], i[1], j[1], b );
    *myJob.in = subVector<T>(in[0]->depth+1, V.W, V.V, i[0], j[1]);     /* swap V<->W as due!   */

    auto *pImperaJob {new genericJob<subVectors<T>, bool>(pImperaRoutine<T>, std::vector<job*>(), V)};
    myJob.OJ.emplace_back(pImperaJob);
    myJob.DJ.emplace_back(pImperaJob);
    return true;   
};

template <class T>
std::function<bool(genericJob<subVector<T>,subVector<T>>&)> pMergeSortRoutine = [](genericJob<subVector<T>, subVector<T>>& myJob) -> bool {
    typedef subVector<T> typeIn;

    if( isBase<T>(myJob.in) ){
        myJob.out = base<T>(myJob.in);
        return true;
    }

    if( myJob.in.depth < subVector<T>::threshold ){
        myJob.out = mergeSortRoutine<T>(myJob.in);
        return true;
    }
 
    for( auto &v: divide<T>(myJob.in) )
        myJob.DJ.push_back(new genericJob<subVector<T>, subVector<T>>(pMergeSortRoutine<T>, std::vector<job *>(), v));

    auto *imperaJob{new genericJob<subVector<T>*, subVector<T>*>(imperaRoutine<T>, myJob.DJ, &myJob.out)};
    myJob.OJ.emplace_back(imperaJob);
    myJob.PJ.emplace_back(imperaJob);
    return true;
};
}
#include <limits>
namespace parser {
/* License : Michele Miccinesi 2018 -   */
/* Little Parser to generate the test for the DEI framework */
/* from command line parameters     */

/* WARNING: name of files shouldn't begin with a number,    */
/* so that the parser can separate between lists of         */
/* filenames and lists of numbers ...                       */
inline bool preMatch(std::string pre, std::string text){
    for( int i=0; i<pre.size(); ++i ){
        if( i>=text.size() )
            return false;
        if( pre[i]!=text[i] )
            return false;
    }
    return true;
}

template <class integer>
inline integer readInt(const std::string& s, int& i){
    integer n{0}, sign{1};
    if( i<s.size() )
        if( s[i]=='-' ){
            sign=-1;
            ++i;
        }
    while( i<s.size() ){
        if( s[i]>'9' || s[i]<'0' )
            break;
        else 
            (n*=10)+=s[i++]-'0';
    }
    return sign*n;
}

template <class uinteger>
inline uinteger readUInt(const std::string& s, int& i){
    uinteger n{0};
    while( i<s.size() ){
        if( s[i]>'9' || s[i]<'0' )
            break;
        else 
            (n*=10)+=s[i++]-'0';
    }
    return n;
}

template <class T>
struct reader{
    static T get(const std::string& s, int &i);
};

template <>
struct reader<int>{
    static int get(const std::string& s, int& i){
        return readInt<int>(s, i);
    }
};

template <>
struct reader<long>{
    static long get(const std::string& s, int& i){
        return readInt<long>(s, i);
    }
};

template <>
struct reader<unsigned long>{
    static unsigned long get(const std::string& s, int& i){
        return readUInt<unsigned long>(s, i);
    }
};

/* We don't have defined operations on strings, so s1..s2 doesn't have a meaning!   */
template <>
struct reader<std::string>{
    static std::string get(const std::string& s, int& i){
        std::string res;
        while( i<s.size() ){
            if( s[i]==']' || s[i]==',' || s[i]=='[' )
                return res;
            res += s[i++];
        }
    }
};

template <class T>
struct readVector;

/* TODO: other specifications... when needed!   */
template <class T>
inline int readList(std::vector<T>&, std::string, int);

template <class T>
struct reader<std::vector<T>>{
    static std::vector<T> get(const std::string& s, int& i){
        std::vector<T> V;
        int ii {readList(V, s, i)};
        if( ii>i+1 )
            i=ii;
        return V;
    }
};

template <class T>
inline T read(const std::string& s, int& i){
    return reader<T>::get(s, i);
}

template <class T>
struct readVector<std::vector<T>>{
    static inline std::vector<std::vector<T>> get(const std::string& s, int i){
        std::vector<std::vector<T>> V;

        if(i>=s.size())
            return V;
        if( s[i]!='[' )
            return V;
        else 
            ++i;

        V.emplace_back( readVector<T>::get(s, i) );
        while(i<s.size()){
            switch( s[i] ){
            case ']': 
                return V;
            case ',':
                V.emplace_back( readVector<T>::get(s, ++i) );
                break;
            default: 
                return V;
            }
        }
        return V;
    }
};

template <class T>
struct readVector{
    static inline std::vector<T> get(const std::string& s, int i){
        std::vector<T> V;

        char lastSeparator{'.'};
        if(i>=s.size())
            return V;
        if( s[i]!='[' )
            return V;
        else 
            ++i;

        V.emplace_back( read<T>(s, i) );
        while(i<s.size()){
            switch( s[i] ){
            case ']': 
                return V;
            case ',':
                V.emplace_back( read<T>(s, ++i) );
                lastSeparator = ',';
                break;
            case '.':
                if(++i<s.size())
                    if( s[i]=='.' ){
                        if( lastSeparator==',' && V.back()!=V[V.size()-2] ){
                            if( V.back()>V[V.size()-2] )
                                for( T step{V.back()-V[V.size()-2]}, v{V.back()+step}, end{read<T>(s, ++i)}; v <= end; v+=step )
                                    V.emplace_back(v);
                            else if( V.back()<V[V.size()-2] )
                                for( T step{V.back()-V[V.size()-2]}, v{V.back()+step}, end{read<T>(s, ++i)}; v >= end; v+=step )
                                    V.emplace_back(v);
                        }else{
                            T end{read<T>(s, ++i)};
                            T v{V.back()};
                            if( end>=v )
                                for( ++v; v<=end; ++v )
                                    V.emplace_back(v);
                            else
                                for( --v; v>=end; --v )
                                    V.emplace_back(v);
                        }
                    } else 
                        return V;
                else
                    return V;
                break;
            default: 
                return V;
                break;
            }
        }
        return V;

    }
};

template <>
inline int readList<std::string>(std::vector<std::string>& V, std::string s, int i){
    int ii{i};

    if(i>=s.size())
        return i;
    if( s[i]!='[' )
        return i;
    else 
        ++i;

    V.emplace_back( read<std::string>(s, i) );
    while(i<s.size()){
        switch( s[i] ){
        case ']': 
            return ++i;
        case ',':
            V.emplace_back( read<std::string>(s, ++i) );
            break;
        default: 
            return ii;
        }
    }
    return ii;
}
/* NOTICE: wrong input => unspecified behaviour :D  */
template <class T>
inline int readList(std::vector<T>& V, std::string s, int i){
    int ii{i};
    char lastSeparator{'.'};
    if(i>=s.size())
        return i;
    if( s[i]!='[' )
        return i;
    else 
        ++i;

    V.emplace_back( read<T>(s, i) );
    while(i<s.size()){
        switch( s[i] ){
        case ']': 
            return ++i;
            break;
        case ',':
            V.emplace_back( read<T>(s, ++i) );
            lastSeparator = ',';
            break;
        case '.':
            if(++i<s.size())
                if( s[i]=='.' ){
                    if( lastSeparator==',' && V.back()!=V[V.size()-2] ){
                        if( V.back()>V[V.size()-2] )
                            for( T step{V.back()-V[V.size()-2]}, v{V.back()+step}, end{read<T>(s, ++i)}; v <= end; v+=step )
                                V.emplace_back(v);
                        else if( V.back()<V[V.size()-2] )
                            for( T step{V.back()-V[V.size()-2]}, v{V.back()+step}, end{read<T>(s, ++i)}; v >= end; v+=step )
                                V.emplace_back(v);
                    }else{
                        T end{read<T>(s, ++i)};
                        T v{V.back()};
                        if( end>=v )
                            for( ++v; v<=end; ++v )
                                V.emplace_back(v);
                        else
                            for( --v; v>=end; --v )
                                V.emplace_back(v);
                    }
                } else 
                    return ii;
            else
                return ii;
            break;
        default: 
            return ii;
            break;
        }
    }
    return ii;
}

template <class integer>
struct uniformDist{
    static std::uniform_int_distribution<integer> get(){
        return std::uniform_int_distribution<integer>(std::numeric_limits<integer>::min(), std::numeric_limits<integer>::max());
    }
};

template <>
struct uniformDist<float>{
    static std::uniform_real_distribution<float> get(){
        return std::uniform_real_distribution<float>(std::numeric_limits<float>::min(), std::numeric_limits<float>::max());
    }
};

template <>
struct uniformDist<double>{
    static std::uniform_real_distribution<double> get(){
        return std::uniform_real_distribution<double>(std::numeric_limits<double>::min(), std::numeric_limits<double>::max());
    }
};

struct test{
    std::random_device r;
    std::seed_seq seed1;
    std::mt19937 e1;

    enum class reportable : uint32_t { null=0, threads=1, size=2, threshold=4, chrono=8, parameters=16 };
    friend reportable operator&(const reportable& a, const reportable& b){
        return reportable(uint32_t(a) & uint32_t(b));
    }

    enum class algorithm { unspecified, mergesort, umergesort, quicksort, pmergesort };

    algorithm testName;
    std::string getTestName(){
        switch(testName){
        case algorithm::unspecified:
            return "unspecified";
        case algorithm::mergesort:
            return "mergesort";
        case algorithm::umergesort:
            return "umergesort";
        case algorithm::quicksort:
            return "quicksort";
        case algorithm::pmergesort:
            return "pmergesort";
        default:
            return "noname";
        }
    }

    bool chronometer{false}, result{false}, uniqueResultFile{false};

    jobLogger* logMe{nullptr};
    int logSize{(1<<14)-1};
    uint32_t toBeLogged{0};
    
    std::vector<std::string> inputFiles, resultFile;
    std::vector<int> threads;
    std::string reportFile, logFile;
    reportable reportSpec{ reportable{0} };

    test() : testName(algorithm::unspecified), seed1{r(), r(), r(), r(), r(), r(), r(), r()}, e1(seed1) {}

    template <class T>
    void parseDataT(std::string s, int i){
        if( i>=s.size() )
            return;

        std::vector<int> rndSizes;
        auto distribution(uniformDist<T>::get());
        if( s[i]=='[' ){
            if( ++i>=s.size() )
                return;
            if( s[i]>='0' && s[i]<='9' )
                readList<int>(rndSizes, s, --i);
            else
                readList<std::string>(inputFiles, s, --i);
        }
        else if( s[i]<'0' || s[i]>'9' )
            inputFiles.emplace_back(read<std::string>(s, i));
        else
            rndSizes.emplace_back(read<int>(s, i));

        for( auto &sz: rndSizes ){
            inputFiles.push_back("mergesort"+std::to_string(sz)+"sz.input");
            std::ofstream file;
            file.open(inputFiles.back(), std::ios::trunc);
            for( int n=0; n<sz; ++n )
                file << distribution(e1) << ' ';
        }
    }
    
    virtual void parseThreshold(std::string, int) = 0;
    virtual void parseData(std::string, int) = 0;
    virtual bool parseSpecificParameters(std::string) {
        return false;
    };
    virtual bool start() = 0;

    void printReport(std::ofstream& out, std::string _nthread, std::string _size, std::string _threshold, std::string _chrono, std::string _parameters){
        if( !reportFile.empty() && (reportSpec != reportable::null)){
            if( (reportSpec & reportable::threads) != reportable::null ){
                out << _nthread << ';';
            }
            if( (reportSpec & reportable::size) != reportable::null ){
                out << _size << ';';
            }
            if( (reportSpec & reportable::threshold) != reportable::null ){
                out << _threshold << ';';
            }
            if( (reportSpec & reportable::chrono) != reportable::null ){
                out << _chrono << ';';
            }
            if( (reportSpec & reportable::parameters) != reportable::null ){
                out << _parameters << ';';
            }
            out << '\n';
        }   
    }

    void prepare(std::ofstream& resultF, std::ofstream& reportF){
        if( !logFile.empty() ){
            if( logMe )
                delete logMe;
            logMe = new jobLogger(logSize, logFile, toBeLogged);
        } else if( logMe ){
            delete logMe;
            logMe = nullptr;
        }

        if( uniqueResultFile )
            resultF.open( resultFile.back(), std::ios::trunc );

        if( !reportFile.empty() && (reportSpec != reportable::null)){
            reportF.open(reportFile, std::ios::app);
            printReport(reportF, "threads", "size", "threshold", "chrono(ms)", "parameters");
        }
    }
    template <class T>
    void newResultFile(std::ofstream& resultF, const std::string& inFilename, const int& nThreads, const T& threshold){
        if( !uniqueResultFile && result ){
            if( resultF.is_open() )
                resultF.close();

            std::string newFilename;
            newFilename+=inFilename[0];
            for( int i=1; i<inFilename.size(); ++i ){
                if( inFilename[i]=='.' )
                    break;
                newFilename += inFilename[i];
            }
            newFilename += std::to_string(nThreads)+"thd"+std::to_string(threshold)+"tsh.output";
            resultFile.emplace_back(newFilename);
            resultF.open(newFilename, std::ios::trunc);
        }
    }

    virtual ~test(){
        if( logMe )
            delete logMe;
    }
};

template <class T>
struct mergeSortTest : test{
    std::vector<int> thresholds;

    mergeSortTest() {
        testName = algorithm::mergesort;
    }
    virtual void parseThreshold(std::string s, int i) override {
        if( readList<int>(thresholds, s, i)==i )
            thresholds.push_back(read<int>(s, i));
    }
    virtual void parseData(std::string s, int i) override {
        parseDataT<T>(s, i);
    }
    virtual bool start(){
        std::ofstream resultF;
        std::ofstream reportF;
        prepare( resultF, reportF );

        using namespace mergeSort;
        for( auto &filename: inputFiles ){
            std::vector<T> V;
            
            {
                std::ifstream file;
                file.open(filename);
                for( T v; file >> v; V.emplace_back(v) );
                file.close();
            }

            if( thresholds.empty() )
                thresholds.emplace_back(subVector<T>::threshold);
            for( auto &threshold: thresholds ){
                for( auto &nThreads: threads ){
                    std::vector<T> V1(V), V2(V1.size());
                    subVector<T> fullRange(0, V1, V2, 0, V1.size()-1);
                    fullRange.threshold = threshold;
                    divImp::divImp<subVector<T>, subVector<T>> mergesort(divide<T>, impera<T>, base<T>, isBase<T>);
                    
                    newResultFile(resultF, filename, nThreads, threshold);
                    
                    if( result ){
                        for( auto &o: mergesort.start(fullRange, nThreads, logMe, chronometer).V )
                            resultF << o << ' ';
                        resultF.flush();
                    } else 
                        mergesort.start(fullRange, nThreads, logMe, chronometer);

                    printReport( reportF, std::to_string(nThreads), std::to_string(V.size()), std::to_string(threshold), 
                        chronometer?std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(mergesort.getDuration()).count()):"", "");
                }
            }
        }
        if( reportF.is_open() )
            reportF.flush();

        return true;
    }
};

template <class T>
struct uMergeSortTest : test{
    std::vector<int> thresholds;
    int _alpha{1}, _beta{1};

    uMergeSortTest(){
        testName = algorithm::umergesort; 
    }
    virtual void parseThreshold(std::string s, int i) override {
        if( readList<int>(thresholds, s, i)==i )
            thresholds.push_back(read<int>(s, i));
    }
    virtual void parseData(std::string s, int i) override  {
        parseDataT<T>(s, i);
    }
    virtual bool parseSpecificParameters(std::string s) override {
        int i{6};
        if( preMatch("ratio=", s) ){
            _alpha = read<int>(s, i);
            if( i<s.size() )
                if( s[i]==':' )
                    _beta = read<int>(s, ++i);
            return true;
        }
        return false;
    }
    virtual bool start(){
        std::ofstream resultF;
        std::ofstream reportF;
        prepare( resultF, reportF );

        using namespace unbMergeSort;
        alpha=_alpha;
        beta=_beta;
        for( auto &filename: inputFiles ){
            std::vector<T> V;
            
            {
                std::ifstream file;
                file.open(filename);
                for( T v; file >> v; V.emplace_back(v) );
                file.close();
            }

            if( thresholds.empty() )
                thresholds.emplace_back(subVector<T>::threshold);
            for( auto &threshold: thresholds ){
                for( auto &nThreads: threads ){
                    std::vector<T> V1(V), V2(V1.size());
                    subVector<T> fullRange(V1, V2, 0, V1.size()-1);
                    fullRange.threshold = threshold;
                    divImp::divImp<subVector<T>, subVector<T>> unbmergesort(divide<T>, impera<T>, base<T>, isBase<T>);
                    
                    newResultFile(resultF, filename, nThreads, threshold);
                    
                    if( result ){
                        for( auto &o: unbmergesort.start(fullRange, nThreads, logMe, chronometer).V )
                            resultF << o << ' ';
                        resultF.flush();
                    } else 
                        unbmergesort.start(fullRange, nThreads, logMe, chronometer);

                    printReport( reportF, std::to_string(nThreads), std::to_string(V.size()), std::to_string(threshold), 
                        chronometer?std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(unbmergesort.getDuration()).count()):"", "");
                }
            }
        }
        if( reportF.is_open() )
            reportF.flush();

        return true;
    }
};

template <class T>
struct quickSortTest : test{
    std::vector<int> thresholds;

    quickSortTest(){
         testName = algorithm::quicksort;
    }
    virtual void parseThreshold(std::string s, int i) override {
        if( readList<int>(thresholds, s, i)==i )
            thresholds.push_back(read<int>(s, i));
    }
    virtual void parseData(std::string s, int i) override {
        parseDataT<T>(s, i);
    }
    virtual bool start(){
        std::ofstream resultF;
        std::ofstream reportF;
        prepare( resultF, reportF );

        using namespace quickSort;
        for( auto &filename: inputFiles ){
            std::vector<T> V;
            
            {
                std::ifstream file;
                file.open(filename);
                for( T v; file >> v; V.emplace_back(v) );
                file.close();
            }
            if( thresholds.empty() )
                thresholds.emplace_back(subVector<T>::threshold);
            for( auto &threshold: thresholds ){
                for( auto &nThreads: threads ){
                    std::vector<T> V1(V);
                    subVector<T> fullRange(V1, 0, V1.size()-1);
                    fullRange.threshold = threshold;
                    divImp::divImp<subVector<T>, subVector<T>> quicksort(divide<T>, impera<T>, base<T>, isBase<T>);
                    
                    newResultFile(resultF, filename, nThreads, threshold);
                    
                    if( result ){
                        for( auto &o: quicksort.start(fullRange, nThreads, logMe, chronometer).V )
                            resultF << o << ' ';
                        resultF.flush();
                    } else 
                        quicksort.start(fullRange, nThreads, logMe, chronometer);

                    printReport( reportF, std::to_string(nThreads), std::to_string(V.size()), std::to_string(threshold), 
                        chronometer?std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(quicksort.getDuration()).count()):"", "");
                }
            }
        }
        if( reportF.is_open() )
            reportF.flush();

        return true;
    }
};

template <class T>
struct pMergeSortTest : test{
    std::vector<std::vector<int>> thresholds;

    pMergeSortTest() {
        testName = algorithm::pmergesort;
    }
    virtual void parseThreshold(std::string s, int i) override {
        std::vector<std::vector<int>> V( readVector<std::vector<int>>::get(s, i) );

        if( V.size()==1 && V[0].empty() )
            V.pop_back();
        if( V.empty() ){
            std::vector<int> threshold( readVector<int>::get(s, i) );

            if( threshold.empty() ){
                threshold.push_back( read<int>(s, i) );
            for( auto &t: threshold )
                thresholds.emplace_back(1, t);
            }
        } else {
            for( auto &v: V ){
                thresholds.emplace_back(v);
                for( auto &o: v )
                    std::cout << o << ' ';
                std::cout << '\n';
            }
            std::cout << std::endl;
        }
    }
    virtual void parseData(std::string s, int i) override {
        parseDataT<T>(s, i);
    }
    virtual bool start(){
        std::ofstream resultF;
        std::ofstream reportF;
        prepare( resultF, reportF );

        using namespace pMergeSort;
        for( auto &filename: inputFiles ){
            std::vector<T> V;
            
            {
                std::ifstream file;
                file.open(filename);
                for( T v; file >> v; V.emplace_back(v) );
                file.close();
            }

            if( thresholds.empty() )
                thresholds.emplace_back(1, subVector<T>::threshold);
            for( auto &threshold: thresholds ){
                for( auto &nThreads: threads ){
                    std::vector<T> V1(V), V2(V1.size());
                    subVector<T> fullRange(0, V1, V2, 0, V1.size()-1);
                    if( threshold.size()>0 ){
                        fullRange.threshold = threshold[0];
                        if( threshold.size()>1 ){
                            subVectors<T>::threshold = threshold[1];
                            if( threshold.size()>2 )
                                subVectors1<T>::threshold = threshold[2];
                        }
                    }


                    auto *parJob = new genericJob<subVector<T>, subVector<T>>(pMergeSortRoutine<T>, std::vector<job*>(), fullRange);

                    setTest<subVector<T>, subVector<T>> pmergesort(mergeSortRoutine<T>, parJob);
                    
                    newResultFile(resultF, filename, nThreads, threshold);
                    
                    if( result ){
                        for( auto &o: *(pmergesort.start(fullRange, nThreads, logMe, chronometer).V) )
                            resultF << o << ' ';
                        resultF.flush();
                    } else 
                        pmergesort.start(fullRange, nThreads, logMe, chronometer);

                    printReport( reportF, std::to_string(nThreads), std::to_string(V.size()), std::to_string(threshold), 
                        chronometer?std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(pmergesort.getDuration()).count()):"", "");
                }
            }
        }
        if( reportF.is_open() )
            reportF.flush();

        return true;
    }
};

void printHelp(char *argv[]);
void printAdditionalHelp();

bool match(std::string s, char argv[]){
    int i;
    for( i=0; i<s.size(); ++i )
        if( s[i]!=argv[i] )
            return false;
    if( argv[i]=='\0' )
        return true;
    return false;
}

/* TODO: choose type, i.e. here it is int by default    */
test* parser(int argc, char *argv[]){
    if( argc<2 ){
        printHelp(argv);
        return nullptr;
    }

    test* myTest;

    switch(argv[1][0]){
    case 'm':
    {
        auto *newTest = new mergeSortTest<int>();
        myTest = newTest; 
        break;
    }
    case 'u':
    {
        auto *newTest = new uMergeSortTest<int>();
        myTest = newTest;
        break;
    }
    case 'q':
    {
        auto *newTest = new quickSortTest<int>();
        myTest = newTest;
        break;
    }
    case 'p':
    {
        auto *newTest = new pMergeSortTest<int>();
        myTest = newTest;
        break;
    }
    case 'i':
    {
        printAdditionalHelp();
        return nullptr;
    }
    default:
        printAdditionalHelp();
        return nullptr;
    }

    for( int i=2; i<argc; ++i ){
        if( preMatch("threads=", argv[i]) ){
            if( argv[i][8]>='0' && argv[i][8]<='9' )
                myTest->threads.push_back( std::stoi(std::string(&argv[i][8])) );
            else 
                readList(myTest->threads, argv[i], 8);
        } else if( preMatch("threshold=", argv[i]) ){
            myTest->parseThreshold(argv[i], 10);
        } else if( preMatch("input=", argv[i]) ){
            myTest->parseData(argv[i], 6);
        } else if( preMatch("log=", argv[i] ) ){
            if( myTest->logFile.empty() )
                myTest->logFile = "test.log";
            std::vector<std::string> toBeLogged;
            if( readList<std::string>(toBeLogged, argv[i], 4) == 4 )
                toBeLogged.emplace_back(&argv[i][4]);
            for( auto &tbl: toBeLogged ){
                if( tbl=="jobSummary" ){
                    myTest->toBeLogged |= jobLogger::jobSummary;
                } else if( tbl=="jobDependency" ){
                    myTest->toBeLogged |= jobLogger::jobDependency;
                } else if( tbl=="jobReceiving" ){
                    myTest->toBeLogged |= jobLogger::jobReceiving;
                } else if( tbl=="jobDoing" ){
                    myTest->toBeLogged |= jobLogger::jobDoing;
                } else if( tbl=="workerAvailable" ){
                    myTest->toBeLogged |= jobLogger::workerAvailable;
                } else if( tbl=="queueFull" ){
                    myTest->toBeLogged |= jobLogger::queueFull;
                } else if( tbl=="jobDistributing" ){    
                    myTest->toBeLogged |= jobLogger::jobDistributing;
                } else if( tbl=="jobCompleted" ){
                    myTest->toBeLogged |= jobLogger::jobCompleted;
                } else if( tbl=="queueMoves" ){
                    myTest->toBeLogged |= jobLogger::queueMoves;
                } else if( tbl=="jobSummaryForQueueMoves" ){
                    myTest->toBeLogged |= jobLogger::jobSummaryForQueueMoves;
                }
            }
        } else if( preMatch("log.dim=", argv[i]) ){
            myTest->logSize = std::stoi(&argv[i][8]);
        } else if( preMatch("log.file=", argv[i]) ){
            myTest->logFile = std::string( &argv[i][9] );
        } else if( match("chrono", argv[i]) ){
            myTest->chronometer = true;
        } else if( match("result", argv[i]) ){
            myTest->result = true;
        } else if( preMatch("result.file=", argv[i]) ){
            myTest->result = true;
            myTest->uniqueResultFile = true;
            myTest->resultFile.emplace_back(&argv[i][12]);
        } else if( preMatch("report=[", argv[i]) ){
            for( int j=8; argv[i][j]!='\0'; ){
                if( preMatch("threads", &argv[i][j]) ){
                    myTest->reportSpec = test::reportable(uint32_t(test::reportable::threads) | uint32_t(myTest->reportSpec));
                    j+=8;
                } else if( preMatch("size", &argv[i][j]) ){
                    myTest->reportSpec = test::reportable(uint32_t(test::reportable::size) | uint32_t(myTest->reportSpec));
                    j+=5;
                } else if( preMatch("threshold", &argv[i][j]) ){
                    myTest->reportSpec = test::reportable(uint32_t(test::reportable::threshold) | uint32_t(myTest->reportSpec));
                    j+=10;
                } else if( preMatch("chrono", &argv[i][j]) ){
                    myTest->reportSpec = test::reportable(uint32_t(test::reportable::chrono) | uint32_t(myTest->reportSpec));
                    j+=7;
                } else if( preMatch("parameters", &argv[i][j]) ){
                    myTest->reportSpec = test::reportable(uint32_t(test::reportable::parameters) | uint32_t(myTest->reportSpec));
                    j+=11;
                }
            }
            if( myTest->reportFile.empty() )
                myTest->reportFile = myTest->getTestName();
        } else if( preMatch("report.file=", argv[i]) ){
            myTest->reportFile = std::string(&argv[i][12]);
        } else {
            if( !myTest->parseSpecificParameters(argv[i]) )
                std::cout << "what about " << argv[i] << "?!?!?!?!?!?!?!?!?" << std::endl;
        }
    }

    if( myTest->threads.empty() )
        myTest->threads.push_back(0);

    return myTest;
}

void printHelp(char *argv[]){
    std::cout << "                 \033[41m \033[43m \033[42m \033[44m\033[1;36m DEI: TESTER - (c) Michele Miccinesi 2018 - \033[44m \033[42m \033[43m \033[41m \033[0m\n";
    std::cout << "\033[1;36;44mUSAGE:\033[0m\n\033[41m \033[0m\n\033[41m \033[0m " <<   argv[0] << " \033[1mtest_name option1 option2\033[0m ...\n" << "\033[41m \033[43m        \033[0m where \033[1moption\033[0m is in \033[1moptions\033[0m or \033[1moptions(test_name)\033[0m:\n";
    std::cout << "\033[41m \033[0m\n\033[41m \033[0m \033[1mtest_name\033[0m ::=\n\033[41m \033[43m        \033[0m \033[1mmergesort\033[0m | \033[1mumergesort\033[0m | \033[1mpmergesort\033[0m | \033[1mquicksort\033[0m\n" <<
        "\033[41m \033[0m\n\033[41m \033[0m \033[1moptions\033[0m ::=\n\033[41m \033[43m        \033[0m \033[1mthreads\033[0m=\033[33mint\033[0m | \033[1mthreads\033[0m=[\033[33mints\033[0m] \n" <<
        "\033[41m \033[43m        \033[0m \033[1mthreshold\033[0m=\033[32mvalue\033[0m | \033[1mthreshold\033[0m=[\033[32mvalues\033[0m]\n" << 
        "\033[41m \033[43m        \033[0m \033[1minput\033[0m=\033[33mint\033[0m | \033[1minput\033[0m=[\033[33mints\033[0m] | \033[1minput\033[0m=\033[31minput_filename\033[0m | \033[1minput\033[0m=[\033[31minput_filenames\033[0m]\n" <<
        "\033[41m \033[43m        \033[0m \033[1mchrono \n" << 
        "\033[41m \033[43m        \033[0m \033[1mresult\033[0m \n" <<
        "\033[41m \033[43m        \033[0m \033[1mresult.file\033[0m=\033[31mresult_filename\033[0m \n" <<
        "\033[41m \033[43m        \033[0m \033[1mlog\033[0m=[\033[36mloggables\033[0m]" <<
        "\n\033[41m \033[43m        \033[42m        \033[0m where \033[36mloggable\033[0m ::= \033[1mjobSummary\033[0m | \033[1mjobDependency\033[0m | \033[1mjobReceiving\033[0m | \033[1mjobDoing\033[0m\n" <<
        "\033[41m \033[43m        \033[42m        \033[0m                  | \033[1mjobDistributing\033[0m |" <<
        " \033[1mjobCompleted\033[0m | \033[1mjobSummaryForQueueMoves\033[0m \n" <<
        "\033[41m \033[43m        \033[42m        \033[0m                  | \033[1mqueueMoves\033[0m | \033[1mqueueFull | \033[1mworkerAvailable\033[0m\033[0m\n" <<
        "\033[41m \033[43m        \033[0m \033[1mlog.file\033[0m=\033[31mlog_filename\033[0m\n" <<
        "\033[41m \033[43m        \033[0m \033[1mlog.dim\033[0m=\033[33mint\033[0m\n" << 
        "\033[41m \033[43m        \033[0m \033[1mreport\033[0m=[\033[34mreportables\033[0m]" <<
        "\n\033[41m \033[43m        \033[42m        \033[0m where \033[34mreportable\033[0m ::= \033[1mthreads\033[0m | \033[1msize\033[0m | \033[1mthreshold\033[0m | \033[1mchrono\033[0m | \033[1mparameters\033[0m\n" <<
        "\033[41m \033[43m        \033[0m \033[1mreport.file\033[0m=\033[31mreport_filename\033[0m\n" <<
        "\033[41m \033[0m\n\033[41m \033[0m \033[1moptions(umergesort)\033[0m ::=\n\033[41m \033[43m        \033[0m \033[1mratio\033[0m=\033[33mint\033[0m:\033[33mint\033[0m\n" <<
        "\033[41m \033[0m\n\033[41m \033[0m \033[1m[type]\033[0m ::= \n\033[41m \033[43m        \033[0m \033[1m[t0,t1,t2,t3]\033[0m | \033[1m[t0..t1]\033[0m | \033[1m[t0,t1..t2]\033[0m\n" <<
        "\033[41m \033[43m        \033[0m in which with   \033[1m,\033[0m   you specify a list of values, while \n" <<
        "\033[41m \033[43m        \033[0m with   \033[1m..\033[0m   you specify a range, and when the separators are mixed\n" <<
        "\033[41m \033[43m        \033[0m e.g. in   \033[1mt1,t2..t3   t1,t2\033[0m is specifying the step, so that from \033[1mt2\033[0m to \033[1mt3\033[0m\n" <<
        "\033[41m \033[43m        \033[0m are included all values in arithmetic progression with the specified step\n" <<
        "\033[41m \033[43m        \033[0m     ex. [10,11..16..17,19..24] -> [10,11,12,13,14,15,16,17,19,21,23]\n" <<
        "\033[41m \033[43m        \033[0m     ex. [10..13,9..0] -> [10,11,12,13,9,5,1]\n" <<
        "\033[41m \033[43m        \033[0m till now range is implemented only for integer values (not strings)\n" <<
        "\n\033[1;36;44mEXAMPLE:\033[0m\n" <<
        "\033[41m \033[0m " << argv[0] << " mergesort input=1000 threads=[2,4..16] report=[threads,size,chrono] chrono report.file=report.txt\n\n" <<
        " \033[1;44;36m  For additional details launch "+std::string(argv[0])+" info  \033[42m    \033[43m  \033[41m \033[0m" <<
        std::endl;
}

void printAdditionalHelp(){
    std::cout << 
    "TEST SPECIFIC INFO\n\n" <<
    
    "\033[1mmergesort\033[0m :: classical recursive mergesort, division in halves\n" << 
    "    |__ \033[1mthreshold\033[0m :: here is a negative number denoting the depth in the division tree\n" <<
    "\033[1mumergesort\033[0m :: unbalanced mergesort, division in two parts with a user-defined ratio\n" <<
    "    |__ \033[1mthreshold\033[0m :: here is a positive number denoting the size\n" <<
    "    |__ \033[1mratio\033[0m=int:int :: as explained above\n" <<
    "\033[1mpmergesort\033[0m :: mergesort with parallelised merge\n" <<
    "    |__ \033[1mthreshold\033[0m :: we have 3 thresholds to define here:\n" <<
    "                1 - the same as for mergesort\n" <<
    "                2 - the minimum size we want for dividing the merge process\n" <<
    "                3 - the minimum size we want for dividing the copy process\n" <<
    "                If you want, you can specify only the first with \033[1mthreshold=\033[0mt1; otherwise\n" <<
    "                you can specify them with a list, ex. [t1] or [t1,t2] or [t1,t2,t3];\n" <<
    "                as usual, if you want to loop over many of them, write down a list of lists,\n" <<
    "                ex. [[t11,t12,t13],[t21,t22],[t31,t32,t33],[t41]]; range operator  \033[1m..\033[0m  is not\n" <<
    "                defined and for simplicity all elements of the list must be of the same type,\n" <<
    "                so if you write [[t11,t12,t13],[t21,t22],[t31,t32,t33],\033[31mt41\033[0m] then t41 will not\n" <<
    "                be recognised\n" <<
    "                \033[1;31;43mWARNING:\033[0m for performance, there is not the check for corner cases, so\n" <<
    "                         minimum sizes for dividing merge/copy processes must be reasonably big\n" <<
    "\033[1mquicksort\033[0m :: classical recursive quicksort, elements equal to pivot are kept together in division\n" <<
    "    |__ \033[1mthreshold\033[0m :: here again is a positive number denoting the size\n" <<
    "\n\n" <<
    
    "RANGES\n\n" <<
    
    "Various options can be specified with a list of values, specified with square brackets [ ];\n" <<
    "there are two kinds of separator, \n" <<
    "\033[1m,\033[0m :: to indicate a list \n" <<
    "\033[1m..\033[0m :: to indicate a range (at the moment not implemented for strings)\n" <<
    "      when .. is preceded by , the range is calculated with the same step between the preceding two\n" <<
    "      values\n" <<
    "Usually a list of values means that the test will be repeated for all such values.\n" <<
    "\033[1;31;43mWARNING:\033[0m strings cannot begin with a number!!!\n" <<
    "\n" <<
    "OPTIONS DETAILS\n" <<
    "\n" <<
    "\033[1mthreads\033[0m=int\n" <<
    "\033[1mthreads\033[0m=[ints] :: specify number of threads - that is number of workers; 0 will give you purely\n" <<
    "                  sequential execution, without any overhead of the parallel implementation\n" <<
    "\033[1minput\033[0m=int\n" <<
    "\033[1minput\033[0m=[ints] :: specify the desired dimensions of input; input will be randomly generated and saved\n" <<
    "                respective files for later inspection\n" <<
    "\033[1minput\033[0m=filename\n" <<
    "\033[1minput\033[0m=[filenames] :: specify desired input files; they will be processed with std::cin, so use\n" <<
    "                     recognized separators such as space or newline\n" <<
    "\033[1mchrono\033[0m :: activate the high_resolution_clock of C++ std to measure execution time of the parallel\n" <<
    "          process only (works even for the sequential execution, threads=0)\n" <<
    "\033[1mresult\033[0m :: save results in distinct files; wheir names will be automatically generated by removing\n" <<
    "          everything after the first . in the input filename and adding the suffix output\n" <<
    "\033[1mresult\033[0m=filename :: save all results in the same specified file\n" <<
    "\033[1mlog\033[0m=[loggables] :: activate a separate thread, interacting with the user during the execution via\n" <<
    "        |          std::cin; when reading 1 it will print the recorded loggables in the specified\n" <<
    "        |          log.file; when reading any other value it will terminate\n" <<
    "        |__ \033[1mjobSummary\033[0m :: number of completed, distributable, waiting jobs per worker\n" <<
    "        |__ \033[1mjobDependency\033[0m :: which jobs is a given job waiting for\n" <<
    "        |__ \033[1mjobReceiving\033[0m :: log when a worker is receiving a job\n" <<
    "        |__ \033[1mjobDoing\033[0m :: log when a job is being done by a worker\n" <<
    "        |__ \033[1mjobDistributing\033[0m :: log when a job is in the distribution process\n" <<
    "        |__ \033[1mjobCompleted\033[0m :: log when a job is completed\n" <<
    "        |__ \033[1mjobSummaryForQueueMoves\033[0m :: log job summary when there is a move in the queue of\n" <<
    "        |                              available workers\n" <<
    "        |__ \033[1mqueueMoves\033[0m :: log all moves in the queue of available workers\n" <<
    "        |__ \033[1mqueueFull\033[0m :: log when the queue is seen complete; remember that we are in relaxed memory\n" <<
    "        |                order plus acquire/release sync, so this doesn't mean that all workers are \n" <<
    "        |                available, that is idle!\n" <<
    "        |__ \033[1mworkerAvailable\033[0m :: log when a worker is available for new jobs, that is idle\n" <<
    "\033[1mlog.file\033[0m=filename :: specify the logger filename\n" <<
    "\033[1mlog.dim\033[0m=int :: specify the size of the ring buffer in which loggables are recorded during execution\n" <<
    "\033[1mreport\033[0m=[reportables] :: activate the report after processing completion of the specified recordables\n" <<
    "            |           the record will be saved in csv format for later processing\n" <<
    "            |__ \033[1mthreads\033[0m :: number of threads\n" <<
    "            |__ \033[1msize\033[0m :: size of the input\n" <<
    "            |__ \033[1mthreshold\033[0m :: specified granularity (actually the value of threshold by now)\n" <<
    "            |__ \033[1mchrono\033[0m :: recorded time of execution\n" <<
    "            |__ \033[1mparameters\033[0m :: test specific parameters, by now unused... \n" <<
    "                              it could be used in umergesort\n" <<
    "\033[1mreport.file\033[0m=filename :: specify where the report will be saved\n" <<
    "\n" <<
    "\n" <<
    "NOTES OF THE AUTHOR\n" <<
    "\n" <<
    "I have written this framework with the idea of testing a different idea of scheduler (not strictly \n" <<
    "greedy) with minimal synchronization, so:\n" <<
    "1- certain choices will look absurd, like keeping all completed jobs: I tried to keep everything\n" <<
    "   of the process scheduling evident even during execution, so function with branches are just split\n" <<
    "   in different jobs\n" <<
    "2- the framework is rudimentary, many improvements would be needed for serious use, like memory \n" <<
    "   management, possibility to recycle each job, exception management...\n" <<
    "3- as you'll notice, most of the code is due to the intention to study the process itself...\n" <<
    "4- the framework is not energy efficient: my secondary purpose was to test the relaxed memory and I\n" <<
    "   decided to use only active waitings\n" <<
    "Nevertheless I left some possible choices at code level, i.e. if you want to play with settings of\n" << 
    "the scheduler, such as \"DFS_TO_ME_BFS_TO_YOU\", you will have to change the code and recompile.\n\n" <<
    "If after all that I will find out that there is interest, I could consider rewriting everything with\n" <<
    "a different purpose in mind. Otherwise I will keep to use the already existing frameworks :)\n" <<
    "\n" <<
    "\n" <<
    "LICENSE AND CONTACT INFO\n" <<
    "\n" <<
    "contact the author: Michele Miccinesi \n" <<
    "                    moc.liamg@iseniccim.elehcim\n" <<
    "\n" <<                    
    "I will be grateful for any comment and suggestion.\n" <<
    std::flush;
}
}


int main(int argc, char *argv[]){
    auto *myTest(parser::parser(argc, argv));
    if( myTest ){
        myTest->start();
        delete myTest;
    }
}
