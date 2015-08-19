/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   stdarg.h
 * Author: s0457958
 *
 * Created on 19 August 2015, 11:45
 */

#ifndef STDARG_H
#define STDARG_H

#define va_start(v,l)   __builtin_va_start(v,l)
#define va_end(v)       __builtin_va_end(v)
#define va_arg(v,l)     __builtin_va_arg(v,l)

typedef __builtin_va_list __gnuc_va_list;
typedef __gnuc_va_list va_list;

#endif /* STDARG_H */

