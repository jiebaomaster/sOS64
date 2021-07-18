#ifndef __LINKAGE_H__
#define __LINKAGE_H__

#define ASM_NL ;

#define SYMBOL_NAME(X) X
#define SYMBOL_NAME_STR(X) #X
#define SYMBOL_NAME_LABEL(X) X##:

// 在 .S 中定义一个的全局变量
#define ENTRY(name) \
.globl SYMBOL_NAME(name) ASM_NL \
SYMBOL_NAME_LABEL(name)

#endif