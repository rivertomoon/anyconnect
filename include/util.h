#ifndef UTIL_H
#define UTIL_H

#include <pjlib.h>
#include <pjlib-util.h>
#include "config.h"

void app_perror(const char *msg, pj_status_t rc);

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define CHECK(expr) status=expr; \
      if (status!=PJ_SUCCESS) { \
          app_perror(#expr, status); \
          return status; \
      }

#define PRINT(...)      \
  printed = pj_ansi_snprintf(p, maxlen - (p-buffer),  \
           __VA_ARGS__); \
  if (printed <= 0 || printed >= (int)(maxlen - (p-buffer))) \
      return -PJ_ETOOSMALL; \
  p += printed

#ifdef HAVE_ANDROID_LOG

#include <android/log.h>
#define INFO(s...) \
      __android_log_print(ANDROID_LOG_INFO, __FILE__, s)

#define DEBUG(s...) \
      __android_log_print(ANDROID_LOG_DEBUG, __FILE__, s)

#define WARN(s...) \
      __android_log_print(ANDROID_LOG_WARN, __FILE__, s)

#define ERROR_LOG(s) \
      __android_log_print(ANDROID_LOG_ERROR, __FILE__, "%s: %s", s, strerror(errno))

#else

	/**
 		* @brief Debug level logging macro.
 		*
 		* Macro to expose function, line number as well as desired log message.
 		*/
#define DEBUG(...)    \
    {\
   	PJ_LOG(4, (__FILENAME__, __VA_ARGS__));  \
    }

#define INFO(...)    \
    {\
    PJ_LOG(3, (__FILENAME__, __VA_ARGS__));\
    }

#define WARN(...)  \
    { \
    PJ_LOG(2, (__FILENAME__, "WARN:  %s L#%d ", __PRETTY_FUNCTION__, __LINE__));  \
    PJ_LOG(2, (__FILENAME__, __VA_ARGS__)); \
    }

#define ERROR_LOG(...)  \
    {\
    PJ_LOG(1,(__FILENAME__, __VA_ARGS__)); \
    }

#define TRACE__(args) PJ_LOG(3,args)

#endif

typedef struct CommonUtil {
    /* Our global variables */
    pj_caching_pool	cp;
    FILE * log_fhnd;

}CommonUtil_t;

pj_status_t util_init(CONFIG_t * config, CommonUtil_t * util);

#endif
