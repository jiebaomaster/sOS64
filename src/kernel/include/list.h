#ifndef __LIST_H__
#define __LIST_H__

#include "lib.h"

/**
 * @brief ptr 指针指向了一个 type 类型的 member 成员，
 *        返回包裹 ptr 的 type 类型的起始地址
 *
 * struct type  <-- 返回值
 * {            |
 *   ...        | offset=(unsigned long)&(((type *)0)->member)
 *              |
 *   member x;  <-- ptr
 *   ...
 * }
 *
 * 1. (type *)0，将从 0 开始的内存区域看作是 type 类型
 * 2. (unsigned long)&(((type *)0)->member)
 *    当 &type=0 时，取得 type->member 的地址，即 member 与 type 起始地址的偏移
 * 3. ptr - offset，即外层的 type 类型起始地址
 *
 * @param ptr 指向 type 类型的某一成员的指针
 * @param type 外层的类型
 * @param member ptr 指向 type 类型的哪个成员
 */
#define container_of(ptr, type, member)                                        \
  ({                                                                           \
    typeof(((type *)0)->member) *p = (ptr);                                    \
    (type *)((unsigned long)p - (unsigned long)&(((type *)0)->member));        \
  })

struct List { // 带头结点的双向循环链表的链表节点
  struct List *prev;
  struct List *next;
};

/**
 * @brief 初始化双向循环链表的头节点
 * 
 * @param list 头结点指针
 */
static inline void list_init(struct List *list) {
  list->prev = list;
  list->next = list;
}

/**
 * @brief 将链表节点 new 加入到节点 entry 的后面
 * 
 * @param entry 
 * @param new 
 */
static inline void list_add_to_behind(struct List *entry, struct List *new) {
  new->prev = entry;
  new->next = entry->next;
  new->next->prev = new;
  entry->next = new;
}

/**
 * @brief 将链表节点 new 加入到节点 entry 的前面
 * 
 * @param entry 
 * @param new 
 */
static inline void list_add_to_before(struct List *entry, struct List *new) {
  new->next = entry;
  new->prev = entry->prev;
  new->prev->next = new;
  entry->prev = new;
}

/**
 * @brief 从链表中删除一个节点 entry
 * 
 * @param entry 待删除节点
 */
static inline void list_del(struct List *entry) {
  entry->next->prev = entry->prev;
  entry->prev->next = entry->next;
}

/**
 * @brief 通过链表的头结点 entry 判断链表是否为空
 * 
 * @param entry 
 * @return long empty=1
 */
static inline long list_is_empty(struct List *entry) {
  if (entry == entry->next && entry == entry->prev)
    return 1;
  else
    return 0;
}

static inline struct List *list_prev(struct List *entry) {
  if (entry->prev != NULL)
    return entry->prev;
  else
    return NULL;
}

static inline struct List *list_next(struct List *entry) {
  if (entry->next != NULL)
    return entry->next;
  else
    return NULL;
}

#endif