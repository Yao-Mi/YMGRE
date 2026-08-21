#ifndef _YMGRE_LIST_H
#define _YMGRE_LIST_H
#include "../CONFIG/YMGRE_Mem.h"

//链表节点
typedef struct gre_listnode_
{
	uint16 len;
	void* data;
	struct gre_listnode_* next;
} gre_listnode;
typedef gre_listnode* GRE_ListNode;
//链表
typedef struct
{
	GRE_ListNode listhead;
	int len;
}gre_list;
typedef gre_list* GRE_List;

typedef void (*listNodeDataFree)(void* thisdatap);

static inline void YMGRE_List_Append(GRE_List mylist, int structLen, void* structData)
{
	GRE_ListNode isp,nsp;
	isp = (GRE_ListNode)GRE_malloc0(sizeof(gre_listnode));
	isp->data = structData; //直接挂载数据
	isp->len = structLen;
	isp->next = NULL;
	
	//表头
	nsp = mylist->listhead;
	if (nsp == NULL)
	{
		mylist->listhead = isp;
		mylist->len = 0;
	}
	//插入尾部
	else
	{
		GRE_ListNode lsp = mylist->listhead;//表头
		while (lsp != NULL) {
			nsp = lsp;
			lsp = lsp->next;
		}
		nsp->next = isp;
	}
	mylist->len++;
}

static inline void YMGRE_List_Remove(GRE_List mylist, int index, listNodeDataFree listfree)
{
	GRE_ListNode freethis = mylist->listhead;
	GRE_ListNode ls = NULL;
	int id = 0;
	//索引不存在
	if (index < 0)
		return;
	while ((freethis != NULL) && (id < index))
	{
		ls = freethis;
		freethis = freethis->next;
		id++;
	}
	//索引不存在
	if (freethis == NULL)
		return;
	//表头
	if (ls == NULL)
	{
		mylist->listhead = freethis->next;
	}
	//表身
	else
	{
		ls->next = freethis->next;
	}
	mylist->len--;
	//自定义释放内存
	(*listfree)(freethis->data);
	GRE_free0(freethis);
}

static inline void YMGRE_List_Clear(GRE_List mylist, listNodeDataFree listfree)
{
	GRE_ListNode lsp, nsp;
	lsp = mylist->listhead;
	nsp = mylist->listhead;
	if (lsp == NULL)
	{
		mylist->len = 0;
		return;
	}
	//释放
	while (lsp != NULL) {
		nsp = lsp;
		lsp = lsp->next;
		//自定义释放内存
		(*listfree)(nsp->data);
		GRE_free0(nsp);
	}
	mylist->listhead = NULL;
	mylist->len = 0;
}


#endif // !_YMGRE_LIST_H

