/* Author: Michele Miccinesi */
/* License: contact the author */

#include <iostream>
#include <vector>
#include <atomic>
#include <functional>
#include <cassert>

#define __CL_ENABLE_EXCEPTIONS
#include "cl.hpp"
#include "util.hpp"
#include "err_code.h"

/* INCLUDE HERE YOUR ASSOCIATIVE BINARY OPERATION FFFF : TTTT(TTTT,TTTT) AND DEFINITION OF TYPE TTTT */
namespace userDefined{
#include "plus.op"	
}	//END NAMESPACE userDefined

template <typename T>
struct safeBuffer{
    std::vector<T> buffer;
    std::atomic<int> b, e, d;
    int64_t n;
    safeBuffer(int n) : buffer(n), b(0) {}
};

enum parallelization : int {sequential=0, openCL=1, openCL_OPT1=2};

template <typename T, parallelization Par> class kWindowReducer;

template <typename T>
class kWindowReducer<T, sequential>{
    std::atomic_flag busy = ATOMIC_FLAG_INIT;
    std::atomic<bool> ready;
    std::vector<T> window, reduction;
    std::atomic<T> result;
    int k;
    std::function<T(const T&, const T&)> assOp;

    void done(){
        busy.clear(std::memory_order_release);
    }
public:
    kWindowReducer( int k ): k(k), window(k), reduction(k), ready{false} {
        assert( k!=0 );
    }

    template <typename safeBufferI, typename safeBufferO>
    bool startReducePipe(const std::function<T(const T&, const T&)> &f, safeBufferI& inStream, safeBufferO& outStream, T zero=0){
        if( !busy.test_and_set(std::memory_order_acquire) ){
            assOp = f;

            for( int i=0; i<k; ++i ){
                if( !inStream.pop(window[i]) ){
                    done();
                    return true;
                }              
            }
            
            reduction.back() = window.back();
            for( int i=k-2; i>=0; --i ){
                reduction[i] = assOp(window[i], reduction[i+1]);
            }
            result.store(reduction[0], std::memory_order_relaxed);
            outStream.push(reduction[0]);

            while( true ){
                for( int i=0; i<k; i ){
                    if( !inStream.pop(window[i]) ){
                        done();
                        return true;
                    } else {
                        reduction[i] = zero;
                        // following: just a map!
                        for( int j=0; j<k; ++j ){
                            reduction[j] = assOp(reduction[j], window[i]);
                        }
                        result.store(reduction[++i], std::memory_order_relaxed);
                        outStream.push(reduction[i]);
                    }
                }
            }
        } else {
            return false;
        }
    }

    T read(){
        while( !ready.load(std::memory_order_acquire) );
        return result.load(std::memory_order_relaxed);
    }
};


/* the source you provide, which you should include, should contain a C implementation 	*/
/* of the associative binary operation and a definition (as typedef) of the 			*/
/* type called TTTT, i.e.    typedef float T;    etc...					    			*/
/* the name used for the function (in the kernel) is FFFF, so if you want to			*/
/* use another name, at the end of your code add a proper #define !						*/
template <typename T>
class kWindowReducer<T, openCL>{
    std::atomic_flag busy = ATOMIC_FLAG_INIT;
    std::atomic<bool> ready;
    std::vector<T> window, reduction;
    std::atomic<T> result;
    int k;
    void done(){
        busy.clear(std::memory_order_release);
    }

    std::string assOpFileName;

    struct _infoCL{
    	cl::Context context;
    	cl::Device device;
        cl::Program program;
        cl::Kernel kernel;
        cl::CommandQueue queue;
        cl::Buffer value;
        cl::Buffer reduction;
        cl::make_kernel<cl::Buffer, cl::Buffer> *pKernelFunc{nullptr};
        ~_infoCL(){
        	if( pKernelFunc!=nullptr )
        		delete pKernelFunc;
        }
    } _openCL;

    bool prepareCL(std::string& functionSource){
    	try{
			_openCL.context = cl::Context(CL_DEVICE_TYPE_GPU);
			{ 
				auto devices = _openCL.context.template getInfo<CL_CONTEXT_DEVICES>();
				if( devices.empty() )
					return false;
				else
					_openCL.device = devices[0];
			}
			_openCL.program = cl::Program(_openCL.context, functionSource+"\n\n"+util::loadProgram("mapper.cl"), false);
			_openCL.program.build();
			_openCL.kernel = cl::Kernel(_openCL.program, "map");
			_openCL.pKernelFunc = new cl::make_kernel<cl::Buffer, cl::Buffer>(_openCL.kernel);
			_openCL.queue = cl::CommandQueue(_openCL.context, _openCL.device);
		} catch( cl::Error &err ){
			if( err.err() == CL_BUILD_PROGRAM_FAILURE ){
				try{
					std::cerr << (_openCL.device.template getInfo<CL_DEVICE_TYPE>()) << ' ' 
						      << (_openCL.device.template getInfo<CL_DEVICE_VENDOR>()) << ' ' 
						      << (_openCL.device.template getInfo<CL_DEVICE_VERSION>()) << '\n'
							  << (_openCL.program.template getBuildInfo<CL_PROGRAM_BUILD_STATUS>(_openCL.device)) << '\n'
							  << (_openCL.program.template getBuildInfo<CL_PROGRAM_BUILD_LOG>(_openCL.device)) << std::endl;
				} catch(cl::Error &err){
					std::cerr << "Nested Error while trying to get info about CL program build..." << std::endl;
				}
			} else {
				std::cerr << "while preparing openCL\n";
				std::cerr << err_code(err.err()) << std::endl;
			}
			return false;
		}
		return true;
	}
public:
    kWindowReducer( int k ): k(k), window(k), reduction(k), ready{false} {
        assert( k!=0 );
    }

    template <typename safeBufferI, typename safeBufferO>
    bool startReducePipe(const std::string &filename, safeBufferI& inStream, safeBufferO& outStream, T zero=0){
        if( !busy.test_and_set(std::memory_order_acquire) ){
            assOpFileName = filename;
            std::string functionSource { util::loadProgram(filename) };
            if( !prepareCL(functionSource) )
            	return false;

            for( int i=0; i<k; ++i ){
                if( !inStream.pop(window[i]) ){
                    done();
                    return true;
                }              
            }
            
            reduction.back() = window.back();
            for( int i=k-2; i>=0; --i ){
                reduction[i] = userDefined::FFFF(window[i], reduction[i+1]);
            }
            result.store(reduction[0], std::memory_order_relaxed);
            outStream.push(reduction[0]);

            while( true ){
                for( int i=0; i<k-1; i ){
                    if( !inStream.pop(window[i]) ){
                        done();
                        return true;
                    } else {
                        reduction[i] = zero;

                        _openCL.reduction = cl::Buffer( _openCL.context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(T)*k, reduction.data() );
                        _openCL.value = cl::Buffer( _openCL.context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(T), &window[i] );
                        (*_openCL.pKernelFunc)(
                        	cl::EnqueueArgs( _openCL.queue, cl::NDRange(k) ),
                        	_openCL.reduction,
                        	_openCL.value
                        );
                        _openCL.queue.finish();
                        cl::copy( _openCL.queue, _openCL.reduction, reduction.begin(), reduction.end() );

                        result.store(reduction[++i], std::memory_order_relaxed);
                        outStream.push(reduction[i]);
                    }
                }
           		if( !inStream.pop(window[k-1]) ){
                    done();
                    return true;
                } else {
                    reduction[k-1] = zero;

                    _openCL.reduction = cl::Buffer( _openCL.context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(T)*k, reduction.data() );
                    _openCL.value = cl::Buffer( _openCL.context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(T), &window[k-1] );
                    (*_openCL.pKernelFunc)(
                    	cl::EnqueueArgs( _openCL.queue, cl::NDRange(k) ),
                    	_openCL.reduction,
                    	_openCL.value
                    );
                    _openCL.queue.finish();
                    cl::copy( _openCL.queue, _openCL.reduction, reduction.begin(), reduction.end() );

                    result.store(reduction[0], std::memory_order_relaxed);
                    outStream.push(reduction[0]);
                }
            }
        } else {
            return false;
        }
    }

    T read(){
        while( !ready.load(std::memory_order_acquire) );
        return result.load(std::memory_order_relaxed);
    }

};


template <typename T>
class kWindowReducer<T, openCL_OPT1>{
    std::atomic_flag busy = ATOMIC_FLAG_INIT;
    std::atomic<bool> ready;
    std::vector<T> window;
    std::atomic<T> result;
    int k;
    void done(){
        busy.clear(std::memory_order_release);
    }

    std::string assOpFileName;

    struct _infoCL{
    	cl::Context context;
    	cl::Device device;
        cl::Program program;
        cl::Kernel kernel;
        cl::CommandQueue queue;
        cl::Buffer value;
        cl::Buffer reduction;
        cl::make_kernel<cl::Buffer, cl::Buffer> *pKernelFunc{nullptr};
        ~_infoCL(){
        	if( pKernelFunc!=nullptr )
        		delete pKernelFunc;
        }
    } _openCL;

    bool prepareCL(std::string& functionSource){
    	try{
			_openCL.context = cl::Context(CL_DEVICE_TYPE_GPU);
			{ 
				auto devices = _openCL.context.template getInfo<CL_CONTEXT_DEVICES>();
				if( devices.empty() )
					return false;
				else
					_openCL.device = devices[0];
			}
			_openCL.program = cl::Program(_openCL.context, functionSource+"\n\n"+util::loadProgram("mapper.cl"), false);
			_openCL.program.build();
			_openCL.kernel = cl::Kernel(_openCL.program, "map");
			_openCL.pKernelFunc = new cl::make_kernel<cl::Buffer, cl::Buffer>(_openCL.kernel);
			_openCL.queue = cl::CommandQueue(_openCL.context, _openCL.device);
		} catch( cl::Error &err ){
			if( err.err() == CL_BUILD_PROGRAM_FAILURE ){
				try{
					std::cerr << (_openCL.device.template getInfo<CL_DEVICE_TYPE>()) << ' ' 
						      << (_openCL.device.template getInfo<CL_DEVICE_VENDOR>()) << ' ' 
						      << (_openCL.device.template getInfo<CL_DEVICE_VERSION>()) << '\n'
							  << (_openCL.program.template getBuildInfo<CL_PROGRAM_BUILD_STATUS>(_openCL.device)) << '\n'
							  << (_openCL.program.template getBuildInfo<CL_PROGRAM_BUILD_LOG>(_openCL.device)) << std::endl;
				} catch(cl::Error &err){
					std::cerr << "Nested Error while trying to get info about CL program build..." << std::endl;
				}
			} else {
				std::cerr << "while preparing openCL\n";
				std::cerr << err_code(err.err()) << std::endl;
			}
			return false;
		}
		return true;
	}
public:
    kWindowReducer( int k ): k(k), window(k), ready{false} {
        assert( k!=0 );
    }

    template <typename safeBufferI, typename safeBufferO>
    bool startReducePipe(const std::string &filename, safeBufferI& inStream, safeBufferO& outStream, T zero=0){
        if( !busy.test_and_set(std::memory_order_acquire) ){
            assOpFileName = filename;
            std::string functionSource { util::loadProgram(filename) };
            if( !prepareCL(functionSource) )
            	return false;

            for( int i=0; i<k; ++i ){
                if( !inStream.pop(window[i]) ){
                    done();
                    return true;
                }              
            }
            {
            	std::vector<T> reduction(k);
        		reduction.back() = window.back();
        		for( int i=k-2; i>=0; --i ){
            		reduction[i] = userDefined::FFFF(window[i], reduction[i+1]);
            	}
            	result.store(reduction[0], std::memory_order_relaxed);
            	outStream.push(reduction[0]);
                _openCL.reduction = cl::Buffer( _openCL.context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(T)*k, reduction.data() );
                _openCL.value = cl::Buffer( _openCL.context, CL_MEM_READ_ONLY, sizeof(T), nullptr );
            }

            T _result {zero};
            while( true ){
                for( int i=0; i<k-1; i ){
                    if( !inStream.pop(window[i]) ){
                        done();
                        return true;
                    } else {
                    	_openCL.queue.enqueueWriteBuffer( _openCL.reduction, CL_FALSE, i*sizeof(T), sizeof(T), &zero );
                    	_openCL.queue.enqueueWriteBuffer( _openCL.value, CL_FALSE, 0, sizeof(T), &window[i] );

                        (*_openCL.pKernelFunc)(
                        	cl::EnqueueArgs( _openCL.queue, cl::NDRange(k) ),
                        	_openCL.reduction,
                        	_openCL.value
                        );
//                        _openCL.queue.finish();
                        _openCL.queue.enqueueReadBuffer( _openCL.reduction, CL_TRUE, (++i)*sizeof(T), sizeof(T), &_result );

                        result.store(_result, std::memory_order_relaxed);
                        outStream.push(_result);
                    }
                }
           		if( !inStream.pop(window[k-1]) ){
                    done();
                    return true;
                } else {
                	_openCL.queue.enqueueWriteBuffer( _openCL.reduction, CL_FALSE, (k-1)*sizeof(T), sizeof(T), &zero );
                    _openCL.queue.enqueueWriteBuffer( _openCL.value, CL_FALSE, 0, sizeof(T), &window[k-1] );

                    (*_openCL.pKernelFunc)(
                    	cl::EnqueueArgs( _openCL.queue, cl::NDRange(k) ),
                    	_openCL.reduction,
                    	_openCL.value
                    );
//                    _openCL.queue.finish();
                    _openCL.queue.enqueueReadBuffer( _openCL.reduction, CL_TRUE, 0, sizeof(T), &_result );

                    result.store(_result, std::memory_order_relaxed);
                    outStream.push(_result);
                }
            }
        } else {
            return false;
        }
    }

    T read(){
        while( !ready.load(std::memory_order_acquire) );
        return result.load(std::memory_order_relaxed);
    }

};


template <typename T>
struct testBuffer{
	bool pop(T& variable){
		if(!std::cin.eof()){
			std::cin >> variable;
			return true;
		}
		return false;
	}
	bool push(T& variable){
		std::cout << variable << ' ';
		return true;
	}
};

int main(){
	kWindowReducer<int, openCL_OPT1> reducer(10);
	testBuffer<int> bufferIO;
	reducer.startReducePipe("plus.op", bufferIO, bufferIO, 0);
}
