
/* IMPORTANT: THE FOLLOWING 2 CONSTANTS DECIDE HOW MUCH TO EXPAND TEMPLATES... 	*/
/* BE CAREFUL WITH THEM, OR BETTER: CHECK YOUR MEMORY WHILE COMPILING, BE READY */
/* WITH CTRL+C   :D 															*/
constexpr const unsigned int MAX_k {1};
constexpr const unsigned int MAX_D {1};

#define __CL_ENABLE_EXCEPTIONS
// Note: IMPROVEMENT... can easily become a parsed parameter... 
// I will add this when everything is rock-solid and efficient
#define IMPROVEMENT_RATIO_FOR_CONVERGENCE 0.01

//#define TBB_USE_DEBUG 1
#include <iostream>
#include <vector>
#include <string>
#include <functional>
#include <limits>
#include <fstream>
#include <sstream>
#include <iterator>
#include <random>
#include <atomic>
#include <memory>

#include <tbb/task_scheduler_init.h>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include <tbb/tick_count.h>
#include <tbb/enumerable_thread_specific.h>
#include <tbb/concurrent_queue.h>
#include <tbb/task_group.h>

#include "../include/parser.cpp"

#include "cl.hpp"
#include "util.hpp"
#include "err_code.h"

constexpr const unsigned char verbose {1};

namespace rnd{
std::random_device r;
std::seed_seq seed1{r(), r(), r(), r(), r(), r(), r(), r()};
std::mt19937 e1(seed1);
} //end namespace rnd

template <unsigned int n>
struct first1{
	static constexpr inline unsigned int get(){
		unsigned int p{1};
		while( (p<<1)<=n )
			p<<=1;
		return p;
	}
};

template <class type, unsigned int n, unsigned int p>
struct pwr{
	static constexpr inline void calculate(const type& x, type& result){
		if constexpr(p==0)
			return;
		result*=result;
		if constexpr(p&n)
			result *= x;
		pwr<type, n, (p>>1)>::calculate(x, result);
	}
};

template <class type, unsigned int n>
struct power{
	static constexpr inline type get(const type& x){
		type result(1);
		power<type, n>::calculate(x, result);
		return result;
	}

	static constexpr inline void calculate(const type& x, type& result){
		if constexpr( n==0 )
			return;
		pwr<type, n, first1<n>::get()>::calculate(x, result);
	}
};


template <class type, unsigned int n>
inline type pow(type x){
	return power<type, n>::get(x);
}

typedef float coorType;
// points are given with "count", so that we can keep together some points if needed
template <unsigned int D>
struct point{
	point() {
		for( size_t i=0; i<D; ++i )
			coor[i]=0;
	}
	point(const coorType& val) {
		for( size_t i=0; i<=D; ++i )
			coor[i]=val;
	}
	point(std::string& line){
		std::istringstream stream(line);
		for( unsigned int i=0; i<D; ++i )
			stream >> coor[i];
		coor[D]=1.;
	}
	coorType coor[D+1];
	inline coorType const& operator[](const size_t &i)const{
		return coor[i];
	}
	inline coorType& operator[](const size_t &i){
		return coor[i];
	}
	inline point<D> operator+(const point<D>& p) const {
		point<D> res;
		for( size_t i=0; i<=D; ++i )
			res[i]=coor[i]+p[i];
		return res;
	}
	inline point<D>& operator+=(const point<D>& p){
		for( size_t i=0; i<=D; ++i )
			coor[i]+=p[i];
		return *this;
	}
	inline point<D>& operator-=(const point<D>& p){
		for( size_t i=0; i<=D; ++i )
			coor[i]-=p[i];
		return *this;
	}
	inline point<D>& normalize(){
		auto fact{coor[D]};
		for( size_t i=0; i<D; ++i )
			coor[i]/=fact;
		coor[D]=1;
		return *this;
	}
	void print()const{
		for( unsigned int i=0; i<=D; ++i )
			std::cout << coor[i] << ' ';
	}
	void print(std::ofstream& out)const{
		for( unsigned int i=0; i<=D; ++i )
			out << coor[i] << ' ';
	}
	inline void clear(){
		for( int i=0; i<=D; ++i )
			coor[i]=0.;
	}
};

template <unsigned int n, unsigned int D>
inline coorType dLN(const point<D> &p, const point<D>& q){
	coorType d(0);
	if( p[D]!=1 ){
		auto factp=p[D];
		if( q[D]!=1 ){
			auto factq=q[D];
			for( size_t i=0; i<D; ++i )
				d += pow<coorType,n>(p[i]/factp-q[i]/factq);
		} else {
			for( size_t i=0; i<D; ++i )
				d += pow<coorType,n>(p[i]/factp-q[i]);
		}
	} else {
		if( q[D]!=1 ){
			auto factq=q[D];
			for( size_t i=0; i<D; ++i )
				d += pow<coorType,n>(p[i]-q[i]/factq);
		} else {
			for( size_t i=0; i<D; ++i )
				d += pow<coorType,n>((p[i]-q[i]));
		}
	}
	return d;
}

template <unsigned int n, unsigned int D>
inline coorType dL(const point<D> & p, const point<D>& q){
	coorType d(0);
	for( size_t i=0; i<D; ++i )
		d += pow<coorType,n>(p[i]-q[i]);
	return d;
}

struct nearestCentroid{
	unsigned int id;
	coorType dist;
	void print()const{
		std::cout << "id:" << id << " d²=" << dist;
	}
	void print(std::ofstream& out)const{
		out << "id:" << id << " d²=" << dist;
	}
};

template <unsigned int k, unsigned int D>
struct centroids{
	point<D> cl[k];
	inline point<D>& operator[](const unsigned int& i){
		return cl[i];
	}
	inline point<D> const & operator[](const unsigned int& i) const{
		return cl[i];
	}
	centroids<k, D>& operator+=(const centroids<k, D>& clu){
		for( unsigned int i=0; i<k; ++i )
			cl[i]+=clu.cl[i];
		return *this;
	}
	centroids<k, D> operator+(const centroids<k, D>& clu) const {
		centroids<k, D> res;
		for( unsigned int i=0; i<k; ++i )
			res[i] = cl[i]+clu[i];
		return res;
	}
	void normalize(){
		for( unsigned int i=0; i<k; ++i )
			cl[i].normalize();
	}
	void print(){
		std::cout << "centroid:\n";
		for( unsigned int i=0; i<k; ++i ){
			cl[i].print();
			std::cout << '\n';
		}
		std::cout << std::endl;
	}
	void print(std::ofstream& out){
		out << "centroid:\n";
		for( unsigned int i=0; i<k; ++i ){
			cl[i].print(out);
			out << '\n';
		}
		out << '\n';
	}
	inline void clear(){
		for( int i=0; i<k; ++i )
			cl[i].clear();
	}
};


/* Note: with a single execution, the subsequent struct was used 	*/
/* to keep info about points nearest as possible points itself		*/	
/* However, with parallel executions with starting with different 	*/
/* centroids I thought it could be better, for false sharing issues,*/
/* aside, namely in kMeansCalcData 									*/
template <unsigned int D>
struct assPoint{
	point<D> pt;
	mutable nearestCentroid cl;
	inline coorType& operator[](const unsigned int& i){
		return pt[i];
	}
	inline coorType const& operator[](const unsigned int& i) const {
		return pt[i];
	} 
	void print()const{
		pt.print();
		cl.print();
	}
	void print(std::ofstream& out)const{
		pt.print(out);
		cl.print(out);
	}
};

template <unsigned int D> struct kMeans;

template <unsigned int k, unsigned int D>
struct kMeansCalcData{
	int optimization;
	typedef tbb::enumerable_thread_specific<centroids<k, D>> localCentroidType;
	localCentroidType newClu;
	kMeansCalcData( const kMeans<D>& kmean, const unsigned int grainReduce=16, const unsigned int grainMap=16 , int optimization=0) 
	: pkmean(&kmean), cls(kmean.Pts.size()), grainReduce{grainReduce}, grainMap{grainMap}, optimization{optimization} {}
	
	kMeans<D> const *pkmean;
	std::vector<nearestCentroid> cls;
	unsigned int grainReduce{16};
	unsigned int grainMap{16};
	centroids<k, D> myCentroids;
	
	coorType convCheckSum;
	tbb::enumerable_thread_specific<coorType> localConvCheckSum;
	struct _clInfo{
		std::vector<cl::Device> devices;
		cl::CommandQueue queue;
		cl::make_kernel<cl::Buffer, cl::Buffer, cl::Buffer, int, int> *func {nullptr};
		cl::Buffer cls;
		cl::Buffer myCentroids;
		~_clInfo(){
			if(func)
				delete func;
		}
	} _openCL;

	bool prepareCL( const kMeans<D>& myMeans ){
		try {
			_openCL.devices = myMeans._openCL.context.template getInfo<CL_CONTEXT_DEVICES>();
			if( _openCL.devices.empty() )
				return false;

			_openCL.queue = cl::CommandQueue(myMeans._openCL.context, _openCL.devices[0]);
	// ACHTUNG: ELSEWHERE I AM ASSUMING THAT NUMBER OF POINTS CAN VARY, SAY FOR ANY REASON...
	// HERE INSTEAD NUMBER OF POINTS SHOULD BE FIXED!!! CHANGE IMPLEMENTATION IF NEEDED				
			_openCL.cls = cl::Buffer(myMeans._openCL.context, CL_MEM_WRITE_ONLY, sizeof(nearestCentroid)*myMeans.Pts.size());
//			_openCL.myCentroids = cl::Buffer(myMeans._openCL.context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR , sizeof(centroids<k, D>), &myCentroids);
			_openCL.func = new cl::make_kernel<cl::Buffer, cl::Buffer, cl::Buffer, int, int>(myMeans._openCL.program, "assignPoint");
		} catch( cl::Error &err ){
			std::cerr << err_code(err.err()) << std::endl;
			return false;
		}
		return true;
	}

    bool importCentroids( const kMeans<D>& myMeans ){
        try{
            _openCL.myCentroids = cl::Buffer(myMeans._openCL.context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR , sizeof(centroids<k, D>), &myCentroids);
        } catch( cl::Error &err ){
            std::cerr << "OpenCL: Errore mentre cercavo di importare dall'host i centroidi:\n" << err_code(err.err()) << std::endl;
            return false;
        }
        return true;
    }


	inline void prepare(){
		auto assign = [&](const unsigned int& i){
			this->cls[i].id = i%k;
		};

		tbb::parallel_for( static_cast<unsigned int>(0), static_cast<unsigned int>(cls.size()), assign );
	}

	inline void prepare(std::function<unsigned int(const unsigned int&)> f){
		auto assign = [&](const unsigned int& i){
			this->cls[i].id = f(i);
		};

		tbb::parallel_for( static_cast<unsigned int>(0), static_cast<unsigned int>(cls.size()), assign );
	}

	inline void clear(){
		newClu.clear();
		myCentroids.clear();
		isChanged.clear();
	}

	inline void adaptTo(const kMeans<D>& kmean){
		pkmean=&kmean;
		cls.resize(kmean.Pts.size());
	}

	tbb::enumerable_thread_specific<bool> isChanged;
	
	struct farPt{
		unsigned int ptId;
		coorType dist{ -1. };
	};

	tbb::enumerable_thread_specific<farPt> farthPts;
	bool inline repair(const unsigned int& id){
		farthPts.clear();
		tbb::parallel_for( tbb::blocked_range<unsigned int>(static_cast<unsigned int>(0), static_cast<unsigned int>(cls.size()), 64),
			[this]( const tbb::blocked_range<unsigned int>& range ){
				for( unsigned int i=range.begin(); i<range.end(); ++i ){
					if( this->myCentroids[this->cls[i].id][D] > 1. )
						if( this->cls[i].dist > this->farthPts.local().dist ){
							this->farthPts.local().ptId = i;
							this->farthPts.local().dist = this->cls[i].dist;
						}
				}
			}
		);
		if( farthPts.size()==0 )
			return false;
		auto ptId{ farthPts.combine( [](const farPt& a, const farPt& b)->farPt { return a.dist>b.dist ? a : b; } ).ptId };
		myCentroids[cls[ptId].id] -= pkmean->Pts[ptId];
		myCentroids[id] += pkmean->Pts[ptId];
		cls[ptId].id = id;
		cls[ptId].dist = -1.;	//because in subsequent repairings it MUST NOT be reassigned
		return true;
	}
};

template <unsigned int D>
struct kMeans{
	kMeans(const std::string& filename) : inputFilename(filename) {}
	std::string inputFilename;
	std::vector<point<D>> Pts;

	struct _clInfo{
		cl::Context context;
		cl::Program program;
		cl::Buffer ptsBuffer;
		size_t ptsBufferSize;
// needed by other constexpr branches  still not implemented
//		float (* tempPtsPtr)[D+1] {nullptr};
//		~_clInfo(){
//			if( tempPtsPtr )
//				delete[] *tempPtsPtr;
//		}
	} mutable _openCL;

	template <unsigned int k>
	inline bool calculateCentroids(kMeansCalcData<k, D>& data) const{
		data.newClu.clear();
		auto calcReduce = [this, &data](const tbb::blocked_range<unsigned int>& range){
			for( auto i=range.begin(); i<range.end(); ++i )
				data.newClu.local()[data.cls[i].id] += this->Pts[i];
		};

		tbb::parallel_for( tbb::blocked_range<unsigned int>(static_cast<unsigned int>(0), static_cast<unsigned int>(data.cls.size()), data.grainReduce), calcReduce );
		
		data.myCentroids = data.newClu.combine([](const centroids<k, D>& a, const centroids<k, D>& b){ return a+b; });

		for( unsigned int i=0; i<k; ++i )
			if( data.myCentroids[i][D]==0. ){
				if constexpr (verbose>1) { std::cout << "Repairing centroid " << i << ": "; data.myCentroids[i].print(); std::cout << std::endl; }
				if( !data.repair(i) )
					return false;
			}

		if constexpr (verbose>1) data.myCentroids.print();
		data.myCentroids.normalize();
		return true;
	}

	template <unsigned int k, bool GPU=false>
	inline bool assignPoints(kMeansCalcData<k, D>& data) const{

		if constexpr(!GPU){
			data.isChanged.clear();
			auto calcMap = [this, &data]( const tbb::blocked_range<unsigned int>& range ){
				for( auto i=range.begin(); i<range.end(); ++i ){
					auto oldId{data.cls[i].id};
					data.cls[i].id = 0;
					data.cls[i].dist = dL<2, D>(this->Pts[i], data.myCentroids[0]);
					for( unsigned int j=1; j<k; ++j ){
						auto tempDist{ dL<2, D>(this->Pts[i], data.myCentroids[j]) };
						if( tempDist < data.cls[i].dist ){
							data.cls[i].id = j;
							data.cls[i].dist = tempDist;
						}
					}
					if( data.cls[i].id != oldId )
						data.isChanged.local() = true;
				}
			};

			tbb::parallel_for( tbb::blocked_range<unsigned int>(static_cast<unsigned int>(0), static_cast<unsigned int>(data.cls.size()), data.grainMap), calcMap );
			return data.isChanged.size()!=0;
		} else {
            if( !data.importCentroids(*this) ){
                calculate<k, 0>(data);
                return false;
            }

			try {
				(*data._openCL.func)( cl::EnqueueArgs(data._openCL.queue, cl::NDRange(Pts.size())), _openCL.ptsBuffer, data._openCL.myCentroids, data._openCL.cls, k, D );
				data._openCL.queue.finish();		//needed?!
				cl::copy(data._openCL.queue, data._openCL.cls, data.cls.begin(), data.cls.end());
			} catch(cl::Error &err){
				std::cerr << err_code(err.err()) << " inside AssignPoint" << std::endl;
			}

			data.localConvCheckSum.clear();
			tbb::parallel_for( tbb::blocked_range<int>(0, data.cls.size(), 64),
				[&data]( const tbb::blocked_range<int>& range ){
					for( int i=range.begin(); i<range.end(); ++i )
						data.localConvCheckSum.local() += data.cls[i].dist;
				}
			);
			coorType tempConvCheckSum { data.localConvCheckSum.combine( [](coorType const& a, coorType const& b) -> coorType { return a+b; } ) };

			bool goOn {(data.convCheckSum-tempConvCheckSum)/tempConvCheckSum > IMPROVEMENT_RATIO_FOR_CONVERGENCE || data.convCheckSum < 0};
			data.convCheckSum = tempConvCheckSum;
			return goOn;
		}
	}

	template <unsigned int k, int optimization=0>
	inline bool calculate(kMeansCalcData<k, D>& data) const {
		if constexpr( optimization==2 ){
			data.convCheckSum = -1;
			do{
				if( !calculateCentroids(data) ){
					if constexpr (verbose>0) std::cout << "\n           CLUSTERS NOT REPAIRABLE!     \n\n";
					return false;
				}
			} while( assignPoints<k, true>(data) );
		} else {
			do{
				if( !calculateCentroids(data) ){
					if constexpr (verbose>0) std::cout << "\n           CLUSTERS NOT REPAIRABLE!     \n\n";
					return false;
				}
			} while( assignPoints(data) );
		}
//		finalCentroids.push(data.myCentroids);
		return true;
	}

	void readPts( std::ifstream& input ){
		std::string line;
		while( std::getline( input, line ) )
			Pts.emplace_back(line);
	}

	bool prepareCL() const {
		try {
			_openCL.context = cl::Context(CL_DEVICE_TYPE_GPU);
			_openCL.program = cl::Program(_openCL.context, util::loadProgram("001beta.cl"), true);

			if constexpr(sizeof(point<D>)==sizeof(coorType[D+1])){
				coorType *ptr { const_cast<coorType *>(reinterpret_cast<const coorType *>(Pts.data())) };
				_openCL.ptsBufferSize = sizeof(point<D>)*Pts.size();
// WARNING: the OCL IMPLEMENTATION REQUIRES a void* even if it is only CL_MEM_READ_ONLY... soo....
// rather than destroying all the consts etc... of C++, I'll just copy pts to a mutable vector.
// GOSH! why are you coding in C then just offering a C++ binding :)
				_openCL.ptsBuffer = cl::Buffer(_openCL.context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, _openCL.ptsBufferSize, ptr);
				return true;
			} else {
				// to be implemented... I think it should never happen
				return false;
			}
		} catch (cl::Error &err){
			if( err.err() == CL_BUILD_PROGRAM_FAILURE ){
				for( auto &device: _openCL.context.template getInfo<CL_CONTEXT_DEVICES>() ){
					std::cerr << (device.template getInfo<CL_DEVICE_TYPE>()) << ' ' << (device.template getInfo<CL_DEVICE_VENDOR>()) << ' ' << (device.template getInfo<CL_DEVICE_VERSION>()) << '\n'
							  << (_openCL.program.template getBuildInfo<CL_PROGRAM_BUILD_STATUS>(device)) << '\n'
							  << (_openCL.program.template getBuildInfo<CL_PROGRAM_BUILD_LOG>(device)) << std::endl;
				}
			} else {
				std::cerr << err_code(err.err()) << std::endl;
			}
			return false;
		}
	}
};

template <unsigned int k, unsigned int D>
struct cluster{
	cluster() {}
	cluster(std::vector<nearestCentroid>&& _cls, const centroids<k, D>& _myCentroids) : cls(std::move(_cls)), myCentroids(_myCentroids) {}
	std::vector<nearestCentroid> cls;
	centroids<k, D> myCentroids;
	void print(std::ofstream& out){
		myCentroids.print(out);
		for( auto & nc: cls ){
			nc.print(out);
			out << '\n';
		}
		out << '\n';
	}
};

template <unsigned int k, unsigned int D>
struct possibleClusters{
	possibleClusters(const kMeans<D> *pkmean) : startingData(pkmean) {} 
	kMeans<D> const *startingData;
	tbb::concurrent_queue<cluster<k, D>> clusters;
	void print(std::ofstream& out){
		for( auto it{clusters.unsafe_begin()}; it!=clusters.unsafe_end(); ++it ){
			it->print(out);
		}
	}
};

template <class T>
unsigned int getNumTokens(const std::string& line){
	std::istringstream stream(line);
	std::vector<T> V{ std::istream_iterator<T>(stream), std::istream_iterator<T>() };
	return V.size();
}

struct toBeDeleted{
	virtual ~toBeDeleted() {};
};

template <unsigned int D>
struct meanD : toBeDeleted{
	static inline std::unique_ptr<toBeDeleted> make(const std::string& filename, std::ifstream& input, const unsigned int& _D){
		if constexpr( D>MAX_D )
			return nullptr;
		else {
			if( _D>D )
				return meanD<D+1>::make(filename, input, _D);
			else{
				auto* pmyMeanD = new meanD<D>;
				pmyMeanD->ptr = new kMeans<D>(filename);
				pmyMeanD->ptr->readPts( input );
				return std::unique_ptr<toBeDeleted>(std::move(static_cast<toBeDeleted *>(pmyMeanD)));
			}
		}
	}
	kMeans<D>& operator*(){
        return *ptr;
    }
    kMeans<D>* const operator->(){
        return ptr;
    }
	virtual ~meanD() override{
		delete ptr;
	}
	kMeans<D>* ptr{nullptr}; 
};

struct make{
	static inline std::unique_ptr<toBeDeleted> mean(const std::string& filename, std::ifstream& input, const unsigned int& _D){
		if( _D>MAX_D )
			return nullptr;
		return meanD<1>::make(filename, input, _D);
	}
};

// I will implement this better, without recursion, after I will have read
// something more (ok.. they are 2000 pages) about variadics
template <unsigned int k, unsigned int D>
struct starter{
	template <class typeArg1, class...typeArgs>
	bool static start(const unsigned int& _k, const unsigned int& _D, parser<typeArg1, typeArgs...>& myParser, toBeDeleted* kMeansUPtr){
		if constexpr( k>MAX_k || D>MAX_D )
			return false;
		else {				// ACHTUNG: this "constexpr else" is ESSENTIAL for the compiler to understand
			if( _k>k )		// which classes are to be examined at compile time, at least by now... Try remove it :D
				return starter<k+1, D>::start(_k, _D, myParser, kMeansUPtr);
			else if( _D>D )
				return starter<k, D+1>::start(_k, _D, myParser, kMeansUPtr);
			else
				return starter<k, D>::calculate(myParser, kMeansUPtr);
		}
	}

// for performance, I am assuming the order given below, so the first parameter
// in the parser is thread etc... Otherwise implement passToByName, easy but less efficient!
	template <class typeArg1, class...typeArgs>
	bool static calculate(parser<typeArg1, typeArgs...>& myParser, toBeDeleted* kMeansBaseUPtr){
		auto myMeans = **static_cast<meanD<D>*>(kMeansBaseUPtr);
		myParser.passTo(starter<k, D>::setAndCalculate, myMeans);

		return true;
	}
// optimization: 0, standard... 1: invert then fuse map-reduce ... 2: use GPU
	inline bool static setAndCalculate(const kMeans<D>& myMeans, const int& thread, const int& trials, const int& parallel, const int& grain, const int &_optimization){
		tbb::tick_count t0= tbb::tick_count::now();
		int optimization{_optimization};
		if( _optimization==2 )
			if( !myMeans.prepareCL() ){
				optimization = 0;
				if (verbose > 0)
					std::cout << "Something went WRONG in the GPU inizialization..." << std::endl;
			}

		tbb::task_scheduler_init init(thread);

		possibleClusters<k, D> results(&myMeans);

		std::uniform_int_distribution<unsigned int> uniformDist(0, k-1);
		auto assigner = [&uniformDist](const unsigned int&) -> unsigned int{
			return uniformDist(rnd::e1);
		};

		std::atomic<int> repetitions(trials);

		auto job = [&assigner, &repetitions, &myMeans, &grain, &optimization, &results](){
			kMeansCalcData<k, D> calcData(myMeans, static_cast<unsigned int>(grain), static_cast<unsigned int>(grain), optimization);
			while(repetitions.fetch_sub(1, std::memory_order_relaxed) > 0){
				calcData.adaptTo(myMeans);
				calcData.prepare(assigner);

				if( myMeans.calculate( calcData ) )
//*OLD VERSION OF TBB ON TITANIC*/					results.clusters.emplace(std::move(calcData.cls), calcData.myCentroids);
					results.clusters.push(cluster<k, D>(std::move(calcData.cls), calcData.myCentroids));

				calcData.clear();
			}
		};

		auto jobGPU = [&assigner, &repetitions, &myMeans, &grain, &optimization, &results](){
			kMeansCalcData<k, D> calcData(myMeans, static_cast<unsigned int>(grain), static_cast<unsigned int>(grain), optimization);

			if( calcData.prepareCL(myMeans) ){
				while(repetitions.fetch_sub(1, std::memory_order_relaxed) > 0){
					calcData.adaptTo(myMeans);
					calcData.prepare(assigner);


					if( myMeans.template calculate<k, 2>( calcData ) )
//*OLD VERSION OF TBB ON TITANIC*/						results.clusters.emplace(std::move(calcData.cls), calcData.myCentroids);
						results.clusters.push(cluster<k, D>(std::move(calcData.cls), calcData.myCentroids));
					calcData.clear();
				}				
			} else {
				while(repetitions.fetch_sub(1, std::memory_order_relaxed) > 0){
					calcData.adaptTo(myMeans);
					calcData.prepare(assigner);

					if( myMeans.calculate( calcData ) )
//*OLD VERSION OF TBB ON TITANIC*/						results.clusters.emplace(std::move(calcData.cls), calcData.myCentroids);
						results.clusters.push(cluster<k, D>(std::move(calcData.cls), calcData.myCentroids));

					calcData.clear();
				}
			}
		};

		tbb::task_group taskGroup;
		if( optimization==2 ){
			for( int i=0; i<parallel; ++i )
				taskGroup.run(jobGPU);
		} else {
			for( int i=0; i<parallel; ++i )
				taskGroup.run(job);
		}
		taskGroup.wait();

		tbb::tick_count t1 = tbb::tick_count::now();

		std::ofstream report;
		report.open( myMeans.inputFilename+".report", std::ios::app );
		report  << "threads;trials;parallel;grain;optimization;chrono\n"
				<< thread << ';' << trials << ';' << parallel << ';' << grain << ';' << optimization << ';' << (t1-t0).seconds() << '\n';
		report << "\n RESULTS: \n";
		results.print( report ); 
		report.flush();
		if constexpr (verbose>0) std::cout << "FINITO IN " << (t1-t0).seconds() << "s ...olé" << std::endl;
		return true;
	}	
};

struct genericStarter{
	template <class typeArg1, class...typeArgs>
	bool static start(const unsigned int& _k, const unsigned int& _D, parser<typeArg1, typeArgs...>& myParser, toBeDeleted* kMeansUPtr){
		if( _k>MAX_k || _D>MAX_D )
			return false;
		return starter<1,1>::start(_k, _D, myParser, kMeansUPtr);
	}
};

int main(int argc, char *argv[]){
	if( argc==1 ){
		if constexpr (verbose>0) std::cout << "Options: \n\t " <<
									"input=[filename] centroids=[int] threads=[int] optimization=[0|1->fuse|2->GPU] parallel=[int] grain=[int] trials=[int]" << 
									"\n\t\t please note that fuse is still not implemented" << std::endl;
		return 0;
	}
	parser<std::string, int, int, int, int, int, int> myParser("input", "centroids", "threads", "trials", "parallel", "grain", "optimization");
	for( int i=1; i<argc; ++i )
		myParser.parse(argv[i]);

	if( myParser.values.empty() ){
		if constexpr (verbose>0) std::cout << "Specify input=[filename]" << std::endl;
		return 0;
	}
	// setting default values
	{
		auto &threads{*get<2>::parserValues(myParser)};
		if( threads.empty() ){
			threads.emplace_back(tbb::task_scheduler_init::default_num_threads());
			if constexpr (verbose>0) std::cout << "Number of threads not specified, using " << threads.back() << " as suggested by TBB" << std::endl;
		}
		auto &grain{*get<5>::parserValues(myParser)};
		if( grain.empty() ){
			grain.emplace_back(16);
			if constexpr (verbose>0) std::cout << "Grain unspecified, using 16" << std::endl;
		}
		auto &optimization{*get<6>::parserValues(myParser)};
		if( optimization.empty() ){
			optimization.emplace_back(0);
			if constexpr (verbose>0) std::cout << "Not fusing reduce-map nor GPU by default" << std::endl;
		}
		auto &parallel{*get<4>::parserValues(myParser)};
		if( parallel.empty() ){
			parallel.emplace_back(1);
			if constexpr (verbose>0) std::cout << "Just 1 execution per time" << std::endl;
		}
		auto &trials{*get<3>::parserValues(myParser)};
		if( trials.empty() ){
			trials.emplace_back(1);
			if constexpr (verbose>0) std::cout << "Just trying once, since you did not specify trials" << std::endl;
		}
	}

	// starting calculations on various parameters combination 
	for( auto &filename: myParser.values ){
		unsigned int D;
		auto readFile = [&D](const std::string& filename) -> std::unique_ptr<toBeDeleted>{
			std::ifstream input;
			input.open( filename );
			if( !input.is_open() ){
				if constexpr (verbose>0) std::cout << "Cannot open " << filename << std::endl;
				return nullptr;
			}
			std::string line;
			if( !std::getline( input, line ) ){
				if constexpr (verbose>0) std::cout << filename << " is empty" << std::endl;
				return nullptr;
			}
			D = getNumTokens<coorType>(line);
			input.clear();
			input.seekg( 0, input.beg );

			return make::mean(filename, input, D);	//To be used, it must first be cast to the appropriate meanD<D>
		};

		auto kMeansUPtr{ readFile(filename) };
		if( !kMeansUPtr )
			continue;

		auto &centroids{*get<1>::parserValues(myParser)};
		if( centroids.empty() )
			if constexpr (verbose>0) std::cout << "Specify centroids=[int]" << std::endl;
		for( auto &k: centroids ){
			if( k<1 )
				if constexpr (verbose>0) std::cout << "Must have at least 1 centroid" << std::endl;
			if( !genericStarter::start( k, D, *get<2>::subParser(myParser), kMeansUPtr.get()) )
				if constexpr (verbose>0) std::cout << "You'll have to recompile with higher MAX_k or MAX_D ... check it out!" << std::endl;
		}
	}
}

/*
	kMeans<1> myMean("fattoAmano");
	myMean.Pts.resize(6);
	myMean.Pts[0][0]=0.;myMean.Pts[0][1]=1.;//myMean.Pts[0][2]=1.;
	myMean.Pts[1][0]=0.2;myMean.Pts[1][1]=1.;//myMean.Pts[1][2]=1.;
	myMean.Pts[2][0]=3.;myMean.Pts[2][1]=1.;//myMean.Pts[2][2]=1.;
	myMean.Pts[3][0]=3.4;myMean.Pts[3][1]=1.;//myMean.Pts[3][2]=1.;
	myMean.Pts[4][0]=-0.2;myMean.Pts[4][1]=1.;//myMean.Pts[4][2]=1.;
	myMean.Pts[5][0]=0.;myMean.Pts[5][1]=1.;//myMean.Pts[5][2]=1.;
	kMeansCalcData<2,1> data1(myMean);
	data1.prepare();
	tbb::task_scheduler_init init(2);
	myMean.calculate(data1);
*/

/* I decided to avoid virtual functions for the starter, even if the implementation */
/* would have been a little bit nicer... but the virtual table for big k and D would*/
/* be too big 																		*/
/*
struct genericStarter;
template <unsigned int k, unsigned int D>
struct starter;

struct unspecStarter{

	virtual ~unspecStarter(){};
};

template <unsigned int k, unsigned int D>
struct starter : unspecStarter{
	unspecStarter *getMyStarter(const unsigned int& _k, const unsigned int& _D){
		if( _k>k ){
			starter<k+1, D> nextStart;
			return nextStart.getMyStarter(_k, _D);
		} else if( _D>D ){
			starter<k, D+1> nextStart;
			return nextStart.getMyStarter(_k, _D);
		} else {
			return new starter<k, D>;
		}
	};
	virtual ~starter() override final {}
};

struct genericStarter{
	virtual unspecStarter *getMyStarter(const unsigned int& _k, const unsigned int& _D){
		if( _k*_D==0 )
			return nullptr;
		starter<1,1> nextStart;
		return nextStart.getMyStarter(_k, _D);		
	}
	virtual ~genericStarter(){};
}; 

*/
