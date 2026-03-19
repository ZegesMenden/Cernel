#include "klogging.h"
#include "kinternal.h"

#ifdef __cplusplus
extern "C" {
#endif

void __log_printf(int level, const char* tag, const char* format, ...) {
    if (__log_level_global >= level) {
        printf("%s", ANSI_DISABLE);

        #ifdef LOG_USE_TIMESTAMP

            printf("[%llu] ", thread_timetoms(thread_gettime())); // Print timestamp in milliseconds since boot

        #endif

        #ifdef LOG_USE_COLORS
            if ( level == LOG_LEVEL_ERROR ) {
                printf("%s", ANSI_COLOR_RED);
            }
            else if ( level == LOG_LEVEL_WARN ) {
                printf("%s", ANSI_COLOR_YELLOW);
            }
            else if ( level == LOG_LEVEL_INFO ) {
                printf("%s", ANSI_COLOR_GREEN);
            }
        #endif

        printf("%s: ", tag);

        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);

        printf("%s\n", ANSI_DISABLE);
    }
}

void _test_fn(const char *fname, int result, const char *_tag) {
    int fname_end = 0;
    while (fname[fname_end] != '(' && fname[fname_end] != '\0') fname_end++;
    char log_out[fname_end + 32];
    memcpy(log_out, "Running |", 10);
    memcpy(log_out + 9, fname, fname_end);
    log_out[fname_end + 9] = '\0';
    strcat(log_out, result ? "|...[OK]" : "|...[FAIL]");
    
    if (result) {
        LOGI(_tag, "%s", log_out);
    } else {
        LOGE(_tag, "%s", log_out);
        while(1);
    }    
};

#ifdef __cplusplus
}
#endif
