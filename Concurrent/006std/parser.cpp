/* License : Michele Miccinesi 2018 -   */
/* Little Parser to generate the test for the DEI framework */
/* from command line parameters     */

/* WARNING: name of files shouldn't begin with a number,    */
/* so that the parser can separate between lists of         */
/* filenames and lists of numbers ...                       */
#include "reader.cpp"

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