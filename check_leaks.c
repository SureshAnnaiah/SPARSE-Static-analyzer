/*
 * sparse/check_leaks.c
 *
 * Copyright (C) 2010 Dan Carpenter.
 *
 * Licensed under the Open Software License version 1.1
 *
 */

/*
 * The point of this check is to look for leaks.
 * foo = malloc();  // <- mark it as allocated.
 * A variable becomes &ok if we:
 * 1) assign it to another variable.
 * 2) pass it to a function.
 *
 * One complication is dealing with stuff like:
 * foo->bar = malloc();
 * foo->baz = malloc();
 * foo = something();
 *
 * The work around is that for now what this check only
 * checks simple expressions and doesn't check whether
 * foo->bar is leaked.
 * 
 */

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "parse.h"
#include "smatch.h"
#include "smatch_test.h"
#include "smatch_slist.h"
#define IDENT_SIZE 30
#define ASSIGN 1
#define NOT_ASSIGNED 0

static int my_id;

STATE(freed);
STATE(assigned);
STATE(ok);
STATE(allocated);

//static struct symbol *this_func;

extern struct collect *collect_node;
static void set_parent(struct expression *expr, struct smatch_state *state);


static const char *allocation_funcs[] = {
	"malloc",
	"kmalloc",
	"kzalloc",
};


struct collect *add_to_func_list(struct collect **head,struct expression *expr_left,struct expression *expr_right)
{
	struct collect *temp=NULL;
	struct collect *cur=*head;
	struct expression *left;
	char *left_name;
	struct symbol *left_sym;

	
	left = strip_expr(expr_left);
	left_name = expr_to_str_sym(left, &left_sym);
	set_state_expr(my_id, expr_left, &allocated);
	printf("setting state for : %s\n",left_name);

	//creating new node for every malloc function call
	temp=(struct collect *)malloc(sizeof(struct collect));
	temp->func_name=(char *)malloc(IDENT_SIZE);
	strcpy(temp->func_name,expr_right->fn->symbol_name->name);
	temp->pos=expr_right->pos;
	temp->sym=left_sym;
	temp->next=NULL;
	
	//inserting a node at the end of the list
	if(*head==NULL)
		*head=temp;
	else
	{
 		while((cur->next)!=NULL)
			cur=cur->next;
		cur->next=temp;
	}

	cur=*head;
	return cur;

}



struct collect *search_list(struct collect **head,char *argument_name)
{
	
	struct collect *cur,*prev;
	cur=prev=*head;
	if(*head==NULL)
		return cur;
	else
	  {
		if(!strcmp(cur->sym->ident->name,argument_name))
		{
			prev=cur->next;
			free(cur);
			head=&prev;
		}
		else
		{
			while(cur!=NULL)
			{
				if(!strcmp(cur->sym->ident->name,argument_name))
				{	
					prev->next=cur->next;
					free(cur);
					break;
				}
				else	{
					prev=cur;
					cur=cur->next; }
			}
	  }       }
	prev=*head;
	return prev;
				
}
void display_list(struct collect **head)
{
	struct collect *cur;
	cur=*head;

	if(*head==NULL)
		printf("No memory leak\n");
	else{
		while(cur!=NULL){
			printf("possible leak of memory '%s' ,at position %d \n",(char *)cur->sym->ident->name,cur->pos.line);
			cur=(cur)->next;		
         	 }
			
        }
	
}

static char *alloc_parent_str(struct symbol *sym)
{
	static char buf[256];

	if (!sym || !sym->ident)
		return NULL;

	snprintf(buf, 255, "%s", sym->ident->name);
	buf[255] = '\0';
	return alloc_string(buf);
}

static char *get_parent_from_expr(struct expression *expr, struct symbol **sym)
{
	char *name;

	expr = strip_expr(expr);

	name = expr_to_str_sym(expr, sym);
	free_string(name);
	if (!name || !*sym || !(*sym)->ident) {
		*sym = NULL;
		return NULL;
	}
	return alloc_parent_str(*sym);
}

static int is_local(struct expression *expr)
{
	char *name;
	struct symbol *sym;
	int ret = 0;

	name = expr_to_str_sym(expr, &sym);
	if (!name || !sym)
		goto out;
	if (sym->ctype.modifiers & (MOD_NONLOCAL | MOD_STATIC | MOD_ADDRESSABLE))
		goto out;
	ret = 1;
out:
	free_string(name);
	return ret;
}

static int is_param(struct expression *expr)
{
	char *name;
	struct symbol *sym;
	struct symbol *tmp;
	int ret = 0;

	name = expr_to_str_sym(expr, &sym);
	if (!name || !sym)
		goto out;
	FOR_EACH_PTR(cur_func_sym->ctype.base_type->arguments, tmp) {
		if (tmp == sym) {
			ret = 1;
			goto out;
		}
	} END_FOR_EACH_PTR(tmp);
out:
	free_string(name);
	return ret;
	
}

static void match_alloc(const char *fn, struct expression *expr, void *unused)
{
	if (!is_local(expr->left))
		return;
	if (is_param(expr->left))
		return;
	if (expr->left->type != EXPR_SYMBOL)
		return;
	set_state_expr(my_id, expr->left, &allocated);
}

static void match_condition(struct expression *expr)
{
	struct sm_state *sm;

	expr = strip_expr(expr);

	switch (expr->type) {
	case EXPR_PREOP:
	case EXPR_SYMBOL:
	case EXPR_DEREF:
		sm = get_sm_state_expr(my_id, expr);
		if (sm && slist_has_state(sm->possible, &allocated))
			set_true_false_states_expr(my_id, expr, &allocated, &ok);
		return;
	case EXPR_ASSIGNMENT:
		 /* You have to deal with stuff like if (a = b = c) */
		match_condition(expr->left);
		return;
	default:
		return;
	}
}

static void set_parent(struct expression *expr, struct smatch_state *state)
{
	char *name;
	struct symbol *sym;

	name = get_parent_from_expr(expr, &sym);
	if (!name || !sym)
		goto free;
	if (state == &ok && !get_state(my_id, name, sym))
		goto free;
	set_state(my_id, name, sym, state);
free:
	free_string(name);
}




static int check_for_assignment(struct expression *expr)
{

		struct sm_state *sm;


		sm = get_sm_state_expr(my_id, expr);
		if (!sm)
			return 0;
		if (!slist_has_state(sm->possible, &allocated))
			return 0;
		return 1;
}

static void match_function_call2(struct expression *expr_left,struct expression *expr_right)
{
	
	if(!strcmp(expr_right->fn->symbol_name->name,"malloc"))
		collect_node=add_to_func_list(&collect_node,expr_left,expr_right);
		
}


static void match_assign(struct expression *expr)
{
	struct sm_state *sm_left,*sm_right;
	char *buf_left,*buf_right,*buf_dependent;
	//struct expression *assigned;

	if(expr->right->type==EXPR_CALL)	
		match_function_call2(expr->left,expr->right);
	else
	{
		
		if (expr->op != '=')
			return;
	        if(check_for_assignment(expr->right))
		{
			sm_right=get_sm_state_expr(my_id,expr->right);
			buf_right=show_sm(sm_right);
			printf("right %s\n",buf_right);
			

			sm_left=set_state_expr(my_id, expr->left, &assigned);
			sm_left= get_sm_state_expr(my_id, expr->left);
			sm_right->dependent=sm_left;

			
			
			buf_left=show_sm(sm_left);
			printf("left %s\n",buf_left);


			//sm_dependent=get_sm_state_expr(my_id,sm_right->dependent);
			buf_dependent=show_sm(sm_right->dependent);	
         		printf("dependent %s\n",buf_dependent);


		}	

	}
}



static void match_function_call(struct expression *expr)
{
	struct expression *tmp,*call;
	struct symbol *sym;
	char *func_name,*argument_name;
	struct sm_state *sm,*cur,*prev;
	char *free_buf;
	struct state_list *slist;
	
	
	
        func_name=expr->fn->symbol_name->name;


		FOR_EACH_PTR(expr->args, tmp) 
		{
			call = strip_expr(tmp);
			argument_name = expr_to_str_sym(tmp, &sym);
			if(!strcmp(func_name,"free"))	
			{
					
				printf("Matched free function\n");
				cur=get_sm_state_expr(my_id,tmp);
					
				free_buf=show_sm(cur);
				printf("cur : %s\n",free_buf);
	
				if(cur->dependent)
				{	
					//buf_dependent=show_sm(cur->dependent);	
					printf("dependent of pointer to be freed: %s\n",cur->dependent->name);
				}

                                set_state_expr(my_id, tmp, &freed);
				slist = get_all_states(my_id);
				FOR_EACH_PTR(slist,prev)
				 {
					if (!strcmp(prev->name,argument_name))
					{
							prev->state->name="freed";
							free_buf=show_sm(prev);
					     		printf("prev :%s\n",free_buf);

     								
					}
							

				}END_FOR_EACH_PTR(prev);

					//sm=get_sm_state_expr(my_id,tmp);
					//free_buf=show_sm(cur);
					//printf("free_buf :%s\n",free_buf);
					collect_node=search_list(&collect_node,argument_name);
				}
				else {

					printf("matched function\n");
					sm = get_sm_state_expr(my_id, tmp);
					if (!sm)
						return;
					if (!slist_has_state(sm->possible, &assigned))
						return;	
				      }
		
 		} END_FOR_EACH_PTR(tmp);		

}



	


static void check_for_allocated(void)
{
	struct state_list *slist;
	struct sm_state *tmp;

	slist = get_all_states(my_id);
	FOR_EACH_PTR(slist, tmp) {
		if (!slist_has_state(tmp->possible, &assigned))
			continue;
		sm_msg("warn: possible memory leak of '%s'", tmp->name);
	} END_FOR_EACH_PTR(tmp);
	free_slist(&slist);
}

static void match_return(struct expression *ret_value)
{
	if (__inline_fn)
		return;
	set_parent(ret_value, &ok);
	check_for_allocated();
}

static void match_end_func(struct symbol *sym)
{
	if (__inline_fn)
		return;
	check_for_allocated();
	printf("match end function %s\n",sym->ident->name);
	display_list(&collect_node);
}

void check_leaks(int id)
{
	int i;

	my_id = id;

	for (i = 0; i < ARRAY_SIZE(allocation_funcs); i++)
		add_function_assign_hook(allocation_funcs[i], &match_alloc, NULL);

	add_hook(&match_condition, CONDITION_HOOK);
	//add_hook(&match_function_def,FUNC_DEF_HOOK);

	add_hook(&match_function_call, FUNCTION_CALL_HOOK);
	add_hook(&match_assign, ASSIGNMENT_HOOK);

	add_hook(&match_return, RETURN_HOOK);
	add_hook(&match_end_func, END_FUNC_HOOK);
}
