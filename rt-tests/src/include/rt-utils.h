#ifndef __RT_UTILS_H
#define __RT_UTILS_H

#include <stdint.h>

#define _STR(x) #x
#define STR(x) _STR(x)
#define MAX_PATH 256

int check_privs(void);
char *get_debugfileprefix(void);
int mount_debugfs(char *);
int get_tracers(char ***);
int valid_tracer(char *);

int setevent(char *event, char *val);
int event_enable(char *event);
int event_disable(char *event);
int event_enable_all(void);
int event_disable_all(void);

const char *policy_to_string(int policy);
uint32_t string_to_policy(const char *str);

pid_t gettid(void);

#endif	/* __RT_UTILS.H */
