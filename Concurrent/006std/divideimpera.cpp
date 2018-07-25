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