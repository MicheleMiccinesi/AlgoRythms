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
