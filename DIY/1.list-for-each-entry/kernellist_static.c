#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "malloc.h"
#include "list.h"

/*
Linux内核为链表提供了一致的访问接口。
void INIT_LIST_HEAD(struct list_head *list)；
void list_add(struct list_head *new, struct list_head *head)；
void list_add_tail(struct list_head *new, struct list_head *head)；
void list_del(struct list_head *entry);
int list_empty(const struct list_head *head)；
*/

struct teacher_node {
	int		id;
	char	name;
	int		age;
	struct list_head list;
};

struct teacher_node2 {
	int     age3;
	int		id;
	char	name;
	int		age;
	struct list_head list;
};


//栈上静态链表 仅学习语法
int main()
{
	struct list_head head, *plist;
	struct teacher_node a, b;	
	a.id = 2;
	b.id = 3;
	
	INIT_LIST_HEAD(&head);
	//向链表头，添加第一个节点
	
	list_add(&a.list, &head);
	//向链表头，添加第2个节点
	 //list_add 是实现了链表算法 和 struct teacher_node a业务数据 进行了分离 
	list_add(&b.list, &head);
	
	list_for_each(plist, &head) {
		struct teacher_node *node = list_entry(plist, struct teacher_node, list);
		printf("id = %d\n", node->id);
	}

	return 0;
}
