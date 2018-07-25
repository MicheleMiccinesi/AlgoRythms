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
