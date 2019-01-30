#ifdef __GNUC__
#define LIKELY(COND) __builtin_expect(COND, true)
#define UNLIKELY(COND) __builtin_expect(COND, false)
#else
#define LIKELY(COND) COND
#define UNLIKELY(COND) COND
#endif
