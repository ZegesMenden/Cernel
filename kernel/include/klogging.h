#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "kernel.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

const static uint8_t LOG_LEVEL_ERROR = 1;
const static uint8_t LOG_LEVEL_WARN = 2;
const static uint8_t LOG_LEVEL_INFO = 4;
const static uint8_t LOG_LEVEL_VERBOSE = 8;

const static char* ANSI_DISABLE = "\033[0m";
const static char* ANSI_COLOR_RED = "\033[31m";
const static char* ANSI_COLOR_GREEN = "\033[32m";
const static char* ANSI_COLOR_YELLOW = "\033[33m";

const static int __log_level_global = LOG_LEVEL;

void __log_printf(int level, const char* tag, const char* format, ...);

#define LOGV(tag, ...) __log_printf(LOG_LEVEL_VERBOSE, tag, __VA_ARGS__)
#define LOGI(tag, ...) __log_printf(LOG_LEVEL_INFO, tag, __VA_ARGS__)
#define LOGW(tag, ...) __log_printf(LOG_LEVEL_WARN, tag, __VA_ARGS__)
#define LOGE(tag, ...) __log_printf(LOG_LEVEL_ERROR, tag, __VA_ARGS__)

void _test_fn(const char *fname, int result, const char *_tag);

#define test_fn(fun) _test_fn(#fun, fun, TAG)

#ifdef __cplusplus
}
#endif
