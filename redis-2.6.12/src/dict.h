/* Hash Tables Implementation.
 *
 * This file implements in-memory hash tables with insert/del/replace/find/
 * get-random-element operations. Hash tables will auto-resize if needed
 * tables of power of two in size are used, collisions are handled by
 * chaining. See the source code for more information... :)
 *
 * Copyright (c) 2006-2012, Salvatore Sanfilippo <antirez at gmail dot com>
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

/* 字典是redis的最基本的数据结构之一，因为redis自身就是一个key-value的数据库，它的所有的
 * 存储的内容全都是key-value对，并且key值全部是string结构(sds)，value可以使数字、字符串、
 * 双端链表、集合甚至字典。
 * 所以字典的用途主要主要体现在两个：1、作为redis整个大框架的存储结构；2、作为具体的字典、
 * 压缩链表的存储结构
 * 插入操作需要判断当前是否正在进行rehash操作，如果正在进行优先从ht[1]中插入，当然还需要
 * 判断这个key是否仍然存在。而replace操作将在key存在的时候执行替换操作
 *
 * 字典在适当的时候会进行扩展，扩展的条件是：1、设置了dict_can_resize标记，在当前的entry
 * 数目大于bucket的时候;2、未设置该标记，当entry的数目大于bucket数目的
 * dict_force_resize_ratio(当前为5)倍之后也会强制执行扩展。
 * 扩展的执行是初始化ht[1]表，大小为当前大小的1倍(或者更多，但必须是2的倍数)。这样就会在
 * 之后的操作中渐进的迁移，这样就可以平摊压力，不至于一段时间的压力过大....
 * 其实，说过来说过去，无论多少个哈希表，总的entry都是不变的，
 * 所以即使遍历两个表时间复杂度也是相同的。
 * 
 * 另外，dict_can_resize标记是通过两个函数设置和取消的，至于什么时候设置和取消，再看...
 *
 * 只有在插入的时候才会检查判断是否需要扩展，而在插入、删除、查找和获取随机key-value的
 * 时候都会检查并执行渐进性rehash操作...
 */




#include <stdint.h>

#ifndef __DICT_H
#define __DICT_H

#define DICT_OK 0
#define DICT_ERR 1

/* Unused arguments generate annoying warnings... */
#define DICT_NOTUSED(V) ((void) V)

//使用链地址法解决冲突，每一个key-value项的节点
typedef struct dictEntry {
    void *key;      //key值，在查找的时候需要按照key值比较
    union {
        void *val;     //根据情况三者选一
        uint64_t u64;
        int64_t s64;
    } v;
    struct dictEntry *next;    //单链表
} dictEntry;

typedef struct dictType {
    unsigned int (*hashFunction)(const void *key);    //hash函数
    //节点被复制的时候调用回调函数
    void *(*keyDup)(void *privdata, const void *key);    
    //值被复制的时候回调
    void *(*valDup)(void *privdata, const void *obj);
    //键比较
    int (*keyCompare)(void *privdata, const void *key1, const void *key2);
    //key、value的析构函数
    void (*keyDestructor)(void *privdata, void *key);
    void (*valDestructor)(void *privdata, void *obj);
} dictType;

/* This is our hash table structure. Every dictionary has two of this as we
 * implement incremental rehashing, for the old to the new table. */
typedef struct dictht {
    //索引表，每一项是一个单链表的表头指针
    dictEntry **table;
    //上一项数组的长度
    unsigned long size;
    //指针数组长度的掩码值，用于计算索引值，相当于取余操作
    unsigned long sizemask;
    //当前已经保存的节点数
    unsigned long used;
} dictht;

typedef struct dict {
    //这里是特定字典的处理函数，一些回调函数
    dictType *type;
    //一个字典的处理函数对应的私有数据
    void *privdata;
    //每一个字典需要两个哈希表
    dictht ht[2];
    //rehash操作的标记
    int rehashidx; /* rehashing not in progress if rehashidx == -1 */
    //正在运行的安全迭代器数目
    int iterators; /* number of iterators currently running */
} dict;

/* If safe is set to 1 this is a safe iterator, that means, you can call
 * dictAdd, dictFind, and other functions against the dictionary even while
 * iterating. Otherwise it is a non safe iterator, and only dictNext()
 * should be called while iterating. */
//迭代器是否安全由safe字段标识，如果是安全(safe为1)迭代器，那么对于字典的插入、删除才周一
//不会影响迭代器（不会使迭代器失效），否则迭代器只能执行遍历的操作
typedef struct dictIterator {
    //归属字典的句柄
    dict *d;
    //一些标识和记录信息,哈希表项、bucket的index和是否安全
    int table, index, safe;
    //当前项、下一项
    dictEntry *entry, *nextEntry;
} dictIterator;

/* This is the initial size of every hash table */
//初始化哈希表指针数组的大小
#define DICT_HT_INITIAL_SIZE     4

/* ------------------------------- Macros ------------------------------------*/
//释放键值
#define dictFreeVal(d, entry) \
    if ((d)->type->valDestructor) \
        (d)->type->valDestructor((d)->privdata, (entry)->v.val)

//复制键值,将键值_val_拷贝到一个entry中
#define dictSetVal(d, entry, _val_) do { \
    if ((d)->type->valDup) \
        entry->v.val = (d)->type->valDup((d)->privdata, _val_); \
    else \
        entry->v.val = (_val_); \
} while(0)

//设置64位有符号整数作为键值
#define dictSetSignedIntegerVal(entry, _val_) \
    do { entry->v.s64 = _val_; } while(0)

//设置64位无符号整数作为键值
#define dictSetUnsignedIntegerVal(entry, _val_) \
    do { entry->v.u64 = _val_; } while(0)

//释放键
#define dictFreeKey(d, entry) \
    if ((d)->type->keyDestructor) \
        (d)->type->keyDestructor((d)->privdata, (entry)->key)

//设置键
#define dictSetKey(d, entry, _key_) do { \
    if ((d)->type->keyDup) \
        entry->key = (d)->type->keyDup((d)->privdata, _key_); \
    else \
        entry->key = (_key_); \
} while(0)

//键的比较,返回为bool类型
#define dictCompareKeys(d, key1, key2) \
    (((d)->type->keyCompare) ? \
        (d)->type->keyCompare((d)->privdata, key1, key2) : \
        (key1) == (key2))

//哈希函数
#define dictHashKey(d, key) (d)->type->hashFunction(key)
//一个entry的键
#define dictGetKey(he) ((he)->key)
//获取键值
#define dictGetVal(he) ((he)->v.val)
#define dictGetSignedIntegerVal(he) ((he)->v.s64)
#define dictGetUnsignedIntegerVal(he) ((he)->v.u64)
//一个字典全部的指针数组的项数，每一项称为一个bucket或者slot
#define dictSlots(d) ((d)->ht[0].size+(d)->ht[1].size)
//使用的entry的总数
#define dictSize(d) ((d)->ht[0].used+(d)->ht[1].used)
//是否正在rehash操作
#define dictIsRehashing(ht) ((ht)->rehashidx != -1)

//根据对字典的API阅读，发现以下疑问：
//1、安全迭代器体现在哪里？安全迭代器的含义应该是迭代器不会再插入和删除的时候失效，
//但是代码中没有这方面的考虑...
//在_dictRehashStep中可以看出，只有在没有安全迭代器的时候才会执行rehahs操作,
//所以安全迭代器的存在将导致不再执行rehash，所以它是安全的
//
//
//2、如何区分value的三种不同的类型（unsigned、signed和void*），难道API只提供void*类型，
//然后在通过raw相关的API，交给用户自己定义具体的value类型？
//这个字典的rehash操作的确让我感觉很不一样，并且rehash操作主要是在每次字典操作的时候
//分步进行的...
/* API */
//创建一个字典
dict *dictCreate(dictType *type, void *privDataPtr);
int dictExpand(dict *d, unsigned long size);
//下面四个是添加操作，区别在于是否区分key是否存在
int diceAdd(dict *d, void *key, void *val);
dictEntry *dictAddRaw(dict *d, void *key);
int dictReplace(dict *d, void *key, void *val);
dictEntry *dictReplaceRaw(dict *d, void *key);
int dictDelete(dict *d, const void *key);
int dictDeleteNoFree(dict *d, const void *key);
void dictRelease(dict *d);
dictEntry * dictFind(dict *d, const void *key);
void *dictFetchValue(dict *d, const void *key);
int dictResize(dict *d);
dictIterator *dictGetIterator(dict *d);
dictIterator *dictGetSafeIterator(dict *d);
dictEntry *dictNext(dictIterator *iter);
void dictReleaseIterator(dictIterator *iter);
dictEntry *dictGetRandomKey(dict *d);
void dictPrintStats(dict *d);
unsigned int dictGenHashFunction(const void *key, int len);
unsigned int dictGenCaseHashFunction(const unsigned char *buf, int len);
void dictEmpty(dict *d);
void dictEnableResize(void);
void dictDisableResize(void);
int dictRehash(dict *d, int n);
int dictRehashMilliseconds(dict *d, int ms);
void dictSetHashFunctionSeed(unsigned int initval);
unsigned int dictGetHashFunctionSeed(void);

/* Hash table types */
//三种具体的哈希表类型，回调函数
extern dictType dictTypeHeapStringCopyKey;
extern dictType dictTypeHeapStrings;
extern dictType dictTypeHeapStringCopyKeyValue;

#endif /* __DICT_H */
