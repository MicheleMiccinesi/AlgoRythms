/* Author: Michele Miccinesi */

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <complex>
#include <tbb/blocked_range2d.h>
#include <tbb/tick_count.h>
#include <tbb/parallel_for.h>
#include <tbb/task_scheduler_init.h>
#include <tbb/enumerable_thread_specific.h>

#include "../include/parser.cpp"

template <typename T>
class matrix{
    T *_data;
    int size, rows, cols;
public:
    matrix(int rows, int cols) : size(rows*cols), rows(rows), cols(cols) {
        _data = new T[size];
    }
    matrix(matrix &&M): _data(M._data), size(M.size), rows(M.rows), cols(M.cols) {
        M._data=nullptr;
        M.cols=M.rows=M.size=0;
    }    
    T& operator[](const int& n){
        return _data[n];
    }
    T& operator()(const int& i, const int& j){
        return _data[j+i*cols];
    }
    T *data(){
        return _data;
    }
    int row(){
        return rows;
    }
    int col(){
        return cols;
    }
    ~matrix() {
        if( _data )
            delete[] _data;
    }
};

struct mandelbrot{
    explicit mandelbrot(int maxIterations, int threshold=1600): 
    threshold(threshold), maxIterations(maxIterations) {};
    
    const int threshold, maxIterations;
    
    int calculate(const double& x, const double& y){
        const std::complex<double> c(x, y);
        int i{0};
        for( std::complex<double> n(0., 0.); i<threshold && std::real((n*std::conj(n)))<=threshold; ++i )
            (n*=n)+=c;
        return i;
    }
};

template <class fractal>
class fractalSection{
    double sizeX, sizeY, stepX, stepY, X, Y;
    int pixelX, pixelY;
    fractal *fractalKind;
    matrix<int> M;
public:
    fractalSection(double X, double Y, double sizeX, double sizeY, int pixelX, int pixelY, fractal *fk):
    sizeX(sizeX), sizeY(sizeY), stepX(sizeX/pixelX), stepY(sizeY/pixelY), pixelX(pixelX), pixelY(pixelY),
    X(X), Y(Y), fractalKind(fk), M(pixelY, pixelX)
    {}
    
    int64_t operator()(const tbb::blocked_range2d<int, int>& range){
        int64_t totalWork{0};
        double bX{X + range.cols().begin()*stepX};
        double bY{Y + range.rows().begin()*stepY};
        int b{range.rows().begin()*pixelX + range.cols().begin()};
        int rangeRows{range.rows().end() - range.rows().begin()}, rangeCols{range.cols().end() - range.cols().begin()};
        int delta{pixelX-rangeCols};
        const int e{range.rows().end()*pixelX};
        for( ; b<e; b+=delta, bY+=stepY ){
            const int te{b+rangeCols};
            for( double tX{bX}; b<te; ++b, tX+=stepX )
                totalWork+=(M[b]=fractalKind->calculate(tX, bY));
        }  
        return totalWork;
    }

    int maxColor{255};
	operator matrix<char>() {
		matrix<char> image(pixelY, pixelX*3);
		for( int i=0, j=0; i!=pixelX*pixelY; ++i, ++j ){
			//some color effects
			if( M[i]<=maxColor ){
				image[j]=M[i];
				image[++j]=0;
				image[++j]=0;
			} else if( M[i]<2*(maxColor+1) ) {
				image[j]=0;
				image[++j]=M[i]-(maxColor+1);
				image[++j]=0;
			} else if( M[i]<3*(maxColor+1) ) {
				image[j]=0;
				image[++j]=0;
				image[++j]=M[i]-2*(maxColor+1);
			} else if( M[i]<4*(maxColor+1) ) {
				image[j]=M[i]-3*(maxColor+1);
				image[++j]=0;
				image[++j]=maxColor;
			} else if( M[i]<5*(maxColor+1) ) {
				image[j]=maxColor;
				image[++j]=M[i]-4*(maxColor+1);
				image[++j]=maxColor;
			} else {
				image[j]=maxColor;
				image[++j]=maxColor;
				image[++j]=maxColor;
			}
		}
		return image;
	}  
};

struct ppmImage{
	matrix<char> M;
	ppmImage(matrix<char>&& mat) : M(mat.row(), mat.col()){
        matrix<char> temp( std::move(mat) );
        for( int i=(temp.row()-1)*temp.col(), j{0}; i>=0; i-=(temp.col()<<1) )
            for( const int e=i+temp.col(); i<e; ++i, ++j )
                M[j]=temp[i];
    }

	void toFile(const std::string& filename, const int maxColor=255){
		std::ofstream imageFile;
		imageFile.open(filename, std::ios::trunc | std::ios::out | std::ios::binary);
		imageFile << "P6\n" << M.row() << ' ' << M.col()/3 << '\n' << maxColor << '\n';
		imageFile.write(M.data(), M.row()*M.col());
		imageFile.close();
	}
};    

template <class fractal>
class tbbFractalCalculator{
    fractalSection<fractal>* section;
    tbb::enumerable_thread_specific<std::vector<int64_t>>* counters;
public:
    tbbFractalCalculator(fractalSection<fractal>* section, tbb::enumerable_thread_specific<std::vector<int64_t>> *counters) 
    : section(section), counters(counters)
    {}
    void operator()(const tbb::blocked_range2d<int, int>& range) const {
        counters->local().push_back((*section)(range));
    }
};

#define THRESHOLD_DEF 1000
#define PIXELS_X 1000
#define PIXELS_Y 1000
const std::string threshold_{"threshold"}, thread_{"thread"}, sizeX_{"sizeX"}, sizeY_{"sizeY"}, pixelX_{"pixelX"}, pixelY_{"pixelY"}, X_{"X"}, Y_{"Y"}, grain_{"grain"};
typedef int thresholdType;
typedef int threadsType;
typedef int imageSizeType;
typedef int grainType;
bool test(const std::string& reportName, parser<thresholdType,threadsType,imageSizeType,imageSizeType,grainType,double,double,double,double>& myParser){
    std::vector<thresholdType>* thresholds;
    if( !get<4>::parserValuesByName(thresholds, threshold_, myParser) )
        return false;
    if( thresholds->empty() )
        thresholds->emplace_back(THRESHOLD_DEF);
    
    std::vector<threadsType>* threads;
    if( !get<4>::parserValuesByName(threads, thread_, myParser) )
    if( threads->empty() )
        threads->emplace_back(tbb::task_scheduler_init::default_num_threads());

    std::vector<imageSizeType>* pixelXs;
    if( !get<4>::parserValuesByName(pixelXs, pixelX_, myParser))
        return false;
    if( pixelXs->empty() )
        pixelXs->emplace_back(PIXELS_X);

    std::vector<imageSizeType>* pixelYs;
    if( !get<4>::parserValuesByName(pixelYs, pixelY_, myParser) )
        return false;
    if( pixelYs->empty() )
        pixelYs->emplace_back(PIXELS_Y);

    std::vector<grainType>* grains;
    if( !get<4>::parserValuesByName(grains, grain_, myParser) )
        return false;
    if( grains->empty() )
        grains->emplace_back(16);

    std::ofstream report;
    report.open(reportName, std::ios::app);
    report <<threshold_<<';'<<thread_<<';'<<pixelX_<<';'<<pixelY_<<';'<<grain_<<';'<<X_<<';'<<Y_<<';'<<sizeX_<<';'<<sizeY_<<";chrono;maxOfIterations;callsPerThread;countsPerThread\n";

    auto *subParser{get<5>::subParser(myParser)};
    if( !subParser )
        return false;

    for( auto &thres: *thresholds ){
        mandelbrot myMandel(thres);

        for( auto &threa: *threads ){
            tbb::task_scheduler_init sched(threa);

            for( auto &piX: *pixelXs ){
                for( auto &piY: *pixelYs ){
                    for( auto &gra: *grains ){
                        std::cout << "calling with" << thres << ' ' << threa << ' ' << piX << ' ' <<piY << ' ' << gra << std::endl;
                        std::function<bool(const double&, const double&, const double&, const double&)> subTest = 
                        [&thres, &threa, &piX, &piY, &gra, &report, &myMandel](const double& X, const double& Y, const double& sizeX, const double& sizeY)->bool{
                            tbb::enumerable_thread_specific<std::vector<int64_t>> counter;
                            fractalSection<mandelbrot> section(X, Y, sizeX, sizeY, piX, piY, &myMandel);
                            tbbFractalCalculator<mandelbrot> calculator(&section, &counter);

                            tbb::tick_count t0 = tbb::tick_count::now();
                            tbb::parallel_for(tbb::blocked_range2d<int, int>(0, piY, gra, 0, piX, gra), calculator);
                            tbb::tick_count t1 = tbb::tick_count::now();
                            
                            ppmImage myImage(section);
                            myImage.toFile("mandelbrot_thres"+std::to_string(thres)+"X"+std::to_string(X)+"Y"+std::to_string(Y)+"szX"+std::to_string(sizeX)+"szY"+std::to_string(sizeY)+".ppm");

                            int64_t maxSum{0}, tSum;
                            std::vector<int> callPerThread;
                            std::vector<int64_t> countPerThread;
                            //int64_t localMax, localMaxSum{0};
                            for( auto &count: counter ){
                                tSum=0;
                                int calls{0};
                                for( auto &val:count ){
                                    tSum+=val;
                                    ++calls;
                                }
                                callPerThread.emplace_back(calls);
                                countPerThread.emplace_back(tSum);
                                maxSum = std::max(maxSum,tSum);
                            }
                            
                            report <<thres<<';'<<threa<<';'<<piX<<';'<<piY<<';'<<gra<<';'<<X<<';'<<Y<<';'<<sizeX<<';'<<sizeY<<';'
                                   <<(t1-t0).seconds()<<';'<<maxSum<<(";"+std::to_string(callPerThread)+";"+std::to_string(countPerThread)+"\n");
                        };

                        subParser->passTo(subTest);
                    }
                }
            }
        }
    }
    report.flush();
}

int main(int argc, char *argv[]){
    if( argc==1 ){
        std::cout << "Options: " << threshold_ << ' ' << thread_ << ' ' << pixelX_ << ' ' << pixelY_ << ' ' << grain_ << ' ' << X_ << ' ' << Y_ << ' ' << sizeX_ << ' ' << sizeY_ << std::endl;
        return 0;
    }

    parser<thresholdType,threadsType,imageSizeType,imageSizeType,grainType,double,double,double,double> myParser(threshold_,thread_,pixelX_,pixelY_,grain_,X_,Y_,sizeX_,sizeY_);
    for( int i=1; i<argc; ++i )
        myParser.parse(argv[i]);
    test("report.txt", myParser);
    
    return 0;
}
