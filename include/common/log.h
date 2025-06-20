/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_COMMON_LOG_H
#define LABWC_COMMON_LOG_H

#include <wlr/util/log.h>

void labnag_show(void);
void _nag_log(enum wlr_log_importance verbosity, const char *format, ...) _WLR_ATTRIB_PRINTF(2, 3);

#if __STDC_VERSION__ >= 202311L

#define nag_log(verb, fmt, ...) \
	_wlr_log(verb, "[%s:%d] " fmt, _WLR_FILENAME, __LINE__ __VA_OPT__(,) __VA_ARGS__); \
	_nag_log(verb, fmt, __VA_OPT__(,) __VA_ARGS__)
#define nag_log_errno(verb, fmt, ...) \
	nag_log(verb, fmt ": %s" __VA_OPT__(,) __VA_ARGS__, strerror(errno))

#else

#define nag_log(verb, fmt, ...) \
	_wlr_log(verb, "[%s:%d] " fmt, _WLR_FILENAME, __LINE__, ##__VA_ARGS__); \
	_nag_log(verb, fmt, ##__VA_ARGS__)
#define nag_log_errno(verb, fmt, ...) \
	nag_log(verb, fmt ": %s", ##__VA_ARGS__, strerror(errno))

#endif

#endif /* LABWC_COMMON_LOG_H */
