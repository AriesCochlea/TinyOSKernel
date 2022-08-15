#include "list.h"
#include "interrupt.h"

#define NULL 0


void list_init(struct list* list){
    list->head.prev = NULL;
    list->head.next = &list->tail;
    list->tail.prev = &list->head;
    list->tail.next = NULL;
}




void list_insert_before(struct list_elem* before, struct list_elem* elem){
    enum intr_status old_status = intr_disable();
       
    elem->next = before;
    elem->prev = before->prev;
    before->prev->next = elem;
    before->prev = elem;
    
    intr_set_status(old_status); 
}

void list_remove(struct list_elem* pelem){
    enum intr_status old_status = intr_disable();
    
    pelem->prev->next = pelem->next;
    pelem->next->prev = pelem->prev;
    pelem->next=NULL;
    pelem->prev=NULL;
    intr_set_status(old_status);
}





void list_push(struct list* plist, struct list_elem* elem){
    list_insert_before(plist->head.next, elem);
}


void list_append(struct list* plist, struct list_elem* elem){
    list_insert_before(&plist->tail, elem);
}


struct list_elem* list_pop(struct list* plist){
	if(list_empty(plist))
		return NULL;
    struct list_elem* ret = (plist->head).next;
    list_remove((plist->head).next);
    return ret;
}






int list_empty(struct list* plist){
    enum intr_status old_status = intr_disable();
    if((plist->head).next == &(plist->tail)){
    	intr_set_status(old_status); 
    	return 1;
    }
    else{
    	intr_set_status(old_status); 
    	return 0;
    }
}


unsigned int list_len(struct list* plist){
    unsigned int ret = 0;
    enum intr_status old_status = intr_disable();
    struct list_elem* next = (plist->head).next;
    while(next != &(plist->tail)){
    	next = next->next;
    	++ret;
    }
    intr_set_status(old_status); 
    return ret;
}





int elem_find(struct list* plist, struct list_elem* obj_elem){
    if(list_empty(plist))
    	return 0;
    enum intr_status old_status = intr_disable();
    struct list_elem* ptr = (plist->head).next;
    while(ptr != &(plist->tail)){
    	if(ptr == obj_elem){
    		intr_set_status(old_status); 
    		return 1; 
    	}
    	ptr = ptr->next;
    }
    intr_set_status(old_status); 
    return 0;
}

