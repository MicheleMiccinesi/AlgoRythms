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
