
//
// node.c
//
// Copyright (c) 2010 TJ Holowaychuk <tj@vision-media.ca>
//

#include "list.h"
#include "dict.h"

/*
 * Allocates a new list_node_t. NULL on failure.
 */

list_node_t * list_node_new_string_entry(void *val)
{
	list_node_t *self;
	if (!(self = LIST_MALLOC(sizeof(list_node_t))))
		return NULL;
	self->prev = NULL;
	self->next = NULL;
	self->free = free_string_entry;
	self->val = val;
	return self;
}

list_node_t * list_node_new_index_entry(void *val)
{
	list_node_t *self;
	if (!(self = LIST_MALLOC(sizeof(list_node_t))))
		return NULL;
	self->prev = NULL;
	self->next = NULL;
	self->free = free_index_entry;
	self->val = val;
	return self;
}

list_node_t * list_node_new_struct(void *val)
{
	list_node_t *self;
	if (!(self = LIST_MALLOC(sizeof(list_node_t))))
		return NULL;
	self->prev = NULL;
	self->next = NULL;
	self->free = free;
	self->val = val;
	return self;
}

