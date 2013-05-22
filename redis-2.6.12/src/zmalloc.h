/* zmalloc - total amount of allocated memory aware version of malloc()
 *
 * Copyright (c) 2009-2010, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __ZMALLOC_H
#define __ZMALLOC_H

/* Double expansion needed for stringification of macro values. */
#define __xstr(s) __str(s)
#define __str(s) #s

//使用TMALLOC，google开发的一个代替malloc的方案,多线程安全的内存分配器
#if defined(USE_TCMALLOC)
#define ZMALLOC_LIB ("tcmalloc-" __xstr(TC_VERSION_MAJOR) "." __xstr(TC_VERSION_MINOR))
#include <google/tcmalloc.h>
#if (TC_VERSION_MAJOR == 1 && TC_VERSION_MINOR >= 6) || (TC_VERSION_MAJOR > 1)
#define HAVE_MALLOC_SIZE 1
#define zmalloc_size(p) tc_malloc_size(p)
#else
#error "Newer version of tcmalloc required"
#endif

//使用jemalloc，另外一个开源的内存分配器
#elif defined(USE_JEMALLOC)
#define ZMALLOC_LIB ("jemalloc-" __xstr(JEMALLOC_VERSION_MAJOR) "." __xstr(JEMALLOC_VERSION_MINOR) "." __xstr(JEMALLOC_VERSION_BUGFIX))
#include <jemalloc/jemalloc.h>
#if (JEMALLOC_VERSION_MAJOR == 2 && JEMALLOC_VERSION_MINOR >= 1) || (JEMALLOC_VERSION_MAJOR > 2)
#define HAVE_MALLOC_SIZE 1
#define zmalloc_size(p) je_malloc_usable_size(p)
#else
#error "Newer version of jemalloc required"
#endif

//mac机上还需要使用不同的malloc？？？
#elif defined(__APPLE__)
#include <malloc/malloc.h>
#define HAVE_MALLOC_SIZE 1
#define zmalloc_size(p) malloc_size(p)
#endif

//最后只好使用我们原生态的C库的malloc分配器了
#ifndef ZMALLOC_LIB
#define ZMALLOC_LIB "libc"
#endif

//之所以不直接使用malloc，而使用zmalloc接口，是为了统计分配的内存技术，在每一段分配的内存
//的开始出多分配sizeof(size_t)个字节，这样就能够再分配和释放的时候记录分配的内存大小
//分配size大小的内存
void *zmalloc(size_t size);
//分配size内存并清0
void *zcalloc(size_t size);
//追加分配
void *zrealloc(void *ptr, size_t size);
//释放，无论是通过以上那个接口分配的内存
void zfree(void *ptr);
//复制字符串
char *zstrdup(const char *s);
//统计已使用的内存大小，单位为字节
size_t zmalloc_used_memory(void);
//开启多线程安全标志
void zmalloc_enable_thread_safeness(void);
//设置分配失败的回调函数
void zmalloc_set_oom_handler(void (*oom_handler)(size_t));
float zmalloc_get_fragmentation_ratio(void);
//进程实际使用的物理内存
size_t zmalloc_get_rss(void);
size_t zmalloc_get_private_dirty(void);
void zlibc_free(void *ptr);

//HAVE_MALLOC_SIZE 只有在不适用C库的malloc的时候才设置，这里不考虑
#ifndef HAVE_MALLOC_SIZE
size_t zmalloc_size(void *ptr);
#endif

#endif /* __ZMALLOC_H */
