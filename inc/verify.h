/*
 * File:   verify.h
 * Author: spink
 *
 * Created on 06 March 2015, 15:28
 */

#ifndef VERIFY_H
#define	VERIFY_H

extern int verify_prepare(int tid);
extern void *verify_get_shared_data();
extern int verify_get_tid();
extern bool verify_enabled();

#endif	/* VERIFY_H */

