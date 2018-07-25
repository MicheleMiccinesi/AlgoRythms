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