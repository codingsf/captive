/*
 * File:   string.h
 * Author: spink
 *
 * Created on 11 February 2015, 15:37
 */

#ifndef STRING_H
#define	STRING_H

#include <define.h>

extern void *memset(void *, int, size_t);
extern void *memcpy(void *, const void *, size_t);
extern void *bzero(void *, size_t);

extern int strlen(const char *);
extern char *strcpy(char *, const char *);
extern char *strncpy(char *, const char *, int n);
extern char *strcat(char *, const char *);
extern char *strncat(char *, const char *, int n);

#endif	/* STRING_H */

