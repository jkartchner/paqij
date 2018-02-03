#ifndef __dbg_h__ // if we already defined somewhere else, 
#define __dbg_h__ // don't do anything

#ifndef NDEBUG    // TODO(jake): will later get logging code
#define Assert(Expression) if(!(Expression)) { *(int *)0 = 0; } // divby0!
#define debug(M, ...)
#define clean_errno() 
#define log_err(M, ...) 
#define log_warn(M, ...) 
#define log_info(M, ...) 
#define check(A, ...) if(!(A)) { Assert(A); }
#define sentinel(M, ...) 
#define check_mem(A) { check_debug(A); Assert(A); }
#define check_debug(A, ...) 

#else             // when debug mode, get console output rather than logs

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#define debug(M, ...) fprintf(stderr, "DEBUG %s:%d:" M "\n",\
        __FILE__, __LINE__, ##__VA_ARGS__)

#define clean_errno() (errno == 0 ? "none" : strerror(errno))

#define log_err(M, ...) fprintf(stderr,\
        "[ERROR] (%s:%d: errno: %s) " M "\n", __FILE__, __LINE__,\
        clean_errno(), ##__VA_ARGS__)

#define log_warn(M, ...) fprintf(stderr, "[WARN] (%s:%d: errno: %s) " M "\n",\
        __FILE__, __LINE__, clean_errno(), ##__VA_ARGS__)

#define log_info(M, ...) fprintf(stderr, "[INFO] (%s:%d) " M "\n",\
        __FILE__, __LINE__, ##__VA_ARGS__)

#define check(A, M, ...) if(!(A)) { \
            log_err(M, ##__VA_ARGS__);  errno=0; goto error; }

#define sentinel(M, ...) { log_err(M, ##__VA_ARGS__); \
    errno=0; }

#define check_mem(A) { check_debug((A), "Out of memory."); \
                    Assert(A); }

#define check_debug(A, M, ...) if(!(A)) { debug(M, ##__VA_ARGS__);\
    errno=0; }

#define Assert(Expression) assert(Expression)
                                 

#endif

#endif
