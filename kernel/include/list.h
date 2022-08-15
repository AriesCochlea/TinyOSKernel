//双向链表
#pragma once
	
struct list_elem{
    struct list_elem* prev; 
    struct list_elem* next; 
};
struct list{
    struct list_elem head;
    struct list_elem tail;
};//头尾结点不属于链表本体



//初始化双向链表
void list_init(struct list*);

//头插
void list_push(struct list* plist, struct list_elem* elem);

//尾插
void list_append(struct list* plist, struct list_elem* elem);

//删除链表的第一个结点
struct list_elem* list_pop(struct list* plist);

//判空
int list_empty(struct list* plist);

//链表主体的长度
unsigned int list_len(struct list* plist);

//查找
int elem_find(struct list* plist, struct list_elem* obj_elem);





//以下函数供上面的接口函数调用：

void list_insert_before(struct list_elem* before, struct list_elem* elem);

void list_remove(struct list_elem* pelem);
