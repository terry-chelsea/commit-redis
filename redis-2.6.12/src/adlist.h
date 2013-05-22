/* adlist.h - A generic doubly linked list implementation
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

#ifndef __ADLIST_H__
#define __ADLIST_H__

/* Node, List, and Iterator are the only data structures used currently. */

//list中的节点
typedef struct listNode {
    struct listNode *prev;
    struct listNode *next;
    void *value;
} listNode;

//list的迭代器
typedef struct listIter {
    listNode *next;
    int direction;    //向前or向后
} listIter;

//链表结构
typedef struct list {
    listNode *head;    //表头
    listNode *tail;    //表尾
    void *(*dup)(void *ptr);    //节点被复制节点的回调函数
    void (*free)(void *ptr);    //节点被释放节点的回调函数
    int (*match)(void *ptr, void *key);    //节点比较的时候回调函数
    unsigned long len;    //当前节点的个数
} list;

/* Functions implemented as macros */
#define listLength(l) ((l)->len)
#define listFirst(l) ((l)->head)
#define listLast(l) ((l)->tail)
#define listPrevNode(n) ((n)->prev)
#define listNextNode(n) ((n)->next)
#define listNodeValue(n) ((n)->value)

#define listSetDupMethod(l,m) ((l)->dup = (m))
#define listSetFreeMethod(l,m) ((l)->free = (m))
#define listSetMatchMethod(l,m) ((l)->match = (m))

#define listGetDupMethod(l) ((l)->dup)
#define listGetFree(l) ((l)->free)
#define listGetMatchMethod(l) ((l)->match)

/* Prototypes */
//创建 & 释放
list *listCreate(void);
void listRelease(list *list);
//在头部添加 &　在尾部添加
list *listAddNodeHead(list *list, void *value);
list *listAddNodeTail(list *list, void *value);
//根据after决定在old_node之前或者之后插入一个节点
list *listInsertNode(list *list, listNode *old_node, void *value, int after);
//删除特定节点
void listDelNode(list *list, listNode *node);
//根据方向得到链表的迭代器
listIter *listGetIterator(list *list, int direction);
//迭代器下一项
listNode *listNext(listIter *iter);
//释放迭代器
void listReleaseIterator(listIter *iter);
//复制链表
list *listDup(list *orig);
//查找指定值的节点
listNode *listSearchKey(list *list, void *key);
//根据index找到节点
listNode *listIndex(list *list, long index);
//迭代器转向
void listRewind(list *list, listIter *li);
//迭代器从尾部转向
void listRewindTail(list *list, listIter *li);
//链表翻转
void listRotate(list *list);

/* Directions for iterators */
#define AL_START_HEAD 0
#define AL_START_TAIL 1

#endif /* __ADLIST_H__ */
