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
