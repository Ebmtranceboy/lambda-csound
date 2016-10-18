#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>

//gcc -Wall -O1 -std=c99 -o ace ace.c

#define HISTORY_ENTRIES 100
#define HISTORY_CHARACTERS_PER_ENTRY 200
#define HISTORY_FILE ".ace_history"

#define SEED 10102016

typedef short bool;

bool verbose = 0;
        
static struct termios initial_settings, new_settings;

void init_keyboard(){
    tcgetattr(0,&initial_settings);
    new_settings = initial_settings;
    new_settings.c_lflag &= ~ICANON;
    new_settings.c_lflag &= ~ECHO;
    new_settings.c_cc[VMIN] = 1;
    new_settings.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &new_settings);
}

void close_keyboard(){
    tcsetattr(0, TCSANOW, &initial_settings);
}

typedef union _primitive primitive;
typedef struct _atom atom;

typedef struct _stack{
        atom* head;
        struct _stack* rest;
}stack;

typedef struct _application{
        atom* f;
        atom* x;
}application;

typedef struct _function{
        atom* (*f)(stack*);
        int arity;
}function;

typedef struct _recursive{
        atom* (*rec)(stack*);
        atom* arg;
        atom* _;
        atom* iter;
        stack* cache;
}recursive;

union _primitive{
        recursive r;
        function f;
        application a;
        int n;
        bool b;
        char c;
};

#define        CHAR     0
#define        INT      1
#define        BOOL     2
#define        CMBNTR_S 3
#define        CMBNTR_K 4
#define        CMBNTR_I 5
#define        CMBNTR_B 6
#define        CMBNTR_C 7
#define        APP      8
#define        FUN      9
#define        ITEM     10
#define        LIST     11
#define        STRING   12
#define        REC      13

struct _atom{
        primitive p;
        int k;
};

atom _S = {.k = CMBNTR_S}, _K = {.k = CMBNTR_K}, _I = {.k = CMBNTR_I};
atom _B = {.k = CMBNTR_B}, _C = {.k = CMBNTR_C};
atom *S = &_S, *K = &_K, *I = &_I, *B = &_B, *C = &_C;

atom _emptyString = {.k = CMBNTR_K};
atom* emptyString = &_emptyString;

atom _nil = {.k = CMBNTR_K};
atom* nil = &_nil;

atom _true = {{.b=1}, BOOL};
atom _false = {{.b=0}, BOOL};
atom *true = &_true;
atom *false = &_false;

int natoms = 0;
int nlinks = 0;

atom* new(){
        natoms ++;
        atom*a = (atom*) malloc(sizeof(atom));
        return a;
}

atom* boolc(bool b){return b?true:false;}

atom* intc(int c){
        atom* a = new();
        a->k = INT;
        a->p.n = c;
        return a;
}

atom* charc(char c){
        atom* a = new();
        a->k = CHAR;
        a->p.c = c;
        return a;
}

atom *undefined, *error, *as;
atom *tail, *length, *cons, *isEmpty, *head;
atom *plus, *times, *intPower, *quot, *rem, *subtract, *negate;
atom *foldr, *foldl, *scanl, *map, *take, *drop, *takeWhile;
atom *repeat, *cycle, *insert, *permutations;
atom *ifte, *equal, *equiv, *lessThan, *greaterThan, *not, *and;
atom *factorial, *fibonacci;
atom *filter, *elem, *nth, *transpose;
atom *uncurry, *fix;
atom *randint;

atom *append, *reverse, *init, *last, *concat;
atom *minus, *even, *odd, *fromEnum;
atom *zip, *zipWith;

atom *pair, *fst, *snd;
atom *or, *predicate, *any, *all;
atom *scanr;
atom* dummy;

stack* push(atom *a, stack*list){
        nlinks++;
        stack*new = (stack*) malloc(sizeof(stack));
        new->head=a;
        new->rest = list;
        return new;
}

stack* push2(atom *a, atom *b){return push(a,push(b,NULL));}

void empty(stack*s){
        stack* next = s;
        while(next){
                s = next->rest;
                free(next);
                nlinks--;
                next = s;
        }
}

stack* pushBefore(stack* after, atom* sup, stack* list){
        stack* before = NULL;
        while(list != after){
                before = list;
                list = list->rest;
        }
        stack* new = push(sup,after);
        before->rest = new;
        return new;
}

stack* removeStack(stack*item,stack*list){
        if(item == list) list = list->rest;
        else{
                stack*pos = list, *before = NULL;
                while(pos != item){
                        before = pos;
                        pos=pos->rest;
                }
                before->rest = pos->rest;
        }
        free(item);
        nlinks--;
        item = NULL;
        return list;
}

stack* substituteStack(stack* node, atom* sup, stack*list){
        stack* new = pushBefore(node,sup,list);
        removeStack(node,list);
        return new;
}

int lengthStack(stack*list){
        int n=0;
        while(list){
                list=list->rest;
                n++;
        }
        return n;
}

atom* nthStack(int n, stack*list){
        while(n>0){
                list=list->rest;
                n--;
        }
        return list->head;
}

stack* reverseStack(stack*list){
        stack* r = NULL;
        while(list){
                r=push(list->head,r);
                list=list->rest;
        }
        return r;
}

stack* appendStacks(stack* s1, stack *s2){
        stack *r1 = reverseStack(s1);
        
        while(r1){
                s2 = push(r1->head,s2);
                r1 = r1->rest;
        }
        return s2;
}

stack* subStack(stack*b, stack*e, stack*list){
        stack*copy = NULL;
        while(list && list!=b) list=list->rest;
        while(list && list!=e){
                copy=push(list->head,copy);
                list=list->rest;
        }
        stack*result=reverseStack(copy);
        empty(copy);
        return result;
}

stack* replaceStack(stack*b, stack*e, stack*implant, stack*list){
        return appendStacks(subStack(list,b,list),appendStacks(implant,subStack(e,NULL,list)));
}

stack* dictionary = NULL;

// Application:

atom* eval(atom* f, atom* g);

atom* apply(atom* f, atom* x){
        if(!f || !x) return NULL;

        stack* list = NULL;
        list = push(x,list);
        int n = 1;
        atom* f0 = f;
        
        atom* result = NULL;
  
         while(f->k == APP){
              list = push(f->p.a.x, list);
                n++;
                f = f->p.a.f;
        }
    
        if( f->k==FUN && n == f->p.f.arity && (result = f->p.f.f(list)) ) return result;
        else if(f->k==REC && n == 1) result = eval(f,x);
        
        else if(f->k==CMBNTR_I && n==1) result = x;
        else if(f->k==CMBNTR_K && n==2) result = list->head;
        
        else if(f->k==CMBNTR_B && n==3) result = apply(list->head,
                                        apply(list->rest->head,
                                               list->rest->rest->head));
        else if(f->k==CMBNTR_C && n==3) result = apply(apply(list->head,
                                                      list->rest->rest->head)
                                        ,list->rest->head);
        else if(f->k==CMBNTR_S && n==3) result = apply(apply(list->head,
                                                      list->rest->rest->head)
                                        ,apply(list->rest->head,
                                               list->rest->rest->head));
        else{
                result = new();
                result->k=APP;
        
                result->p.a.f = f0;
                result->p.a.x = x;
        }
        
        empty(list);
        return result;
}

atom* apply2(atom* f, atom* x, atom* y){
        return apply(apply(f,x),y);
}
 
atom* apply3(atom* f, atom* x, atom* y, atom* z){
        return apply(apply2(f,x,y),z);
}

// Util:

int stringSize(atom*);
int listSize(atom*,bool);
char* showItem(atom*);
char* showString(atom*);
char* showList(atom*,bool);
atom* parse(char*);
atom* parseParenthesis(char*);
atom* dryApply(atom*, atom*);

// WARNING about toString : this function canNOT
// use the regular head and tail functions.

char* toString(atom*a){
        int n = 0;
        atom* b = a;
        while(b->k != CMBNTR_K){
                n++;
                b = b->p.a.x->p.a.x;
        }
        char* str = (char*) malloc((1 + n) * sizeof(char));
        for(int i = 0; i<n; i++){
                str[i] = a->p.a.x->p.a.f->p.a.x->p.a.x->p.c;
                a = a->p.a.x->p.a.x;
        }
        str[n] = '\0';
        return str;
}

atom* fromString(const char* str){
        atom*a = emptyString;
        for(int i=strlen(str)-1; i>=0; i--)
                a = apply2(cons,charc(str[i]),a);
        return a;
}

// Recursion

atom* getCache(atom* x, stack*image){
        while(image && !apply2(equal,x,image->head->p.a.f)->p.b) 
                image = image->rest;
        if(image) return image->head->p.a.x;
        return NULL;
}

atom* newRec(atom* (*f) (stack*), atom* _) {
        atom* x = new();
        x->p.r.rec = f;
        x->p.r._ = _;       /* closure, sort of */
        x->p.r.arg = new();
        return x;
}

atom* call(atom* f, atom* g) {
        return f->p.r.rec(push2(f, g));
}

atom* arg(atom* n) {
        atom* x = newRec(0, 0);
        x->p.r.arg = n;
        return x;
}

atom* pre_delayCall(stack*s) {return call(s->head->p.r._, s->rest->head);}
atom delayCall = {{{ pre_delayCall }}};

char* show(atom*,bool);
atom* interpret (atom*,stack*,bool);
void println(atom*);

atom* dummy_fct(stack*list){
        if(!list) return fromString(__func__);
        return NULL;
}

atom* fix_fct(stack*list) {
        if(!list) return fromString(__func__);
        
        atom* _(stack*s){
                        atom* self = s->head->p.r._;
                        atom* x = s->rest->head->p.r.arg;
                        atom* iter = s->head->p.r.iter;
                        atom*image;

                        atom*cache = getCache(x,s->head->p.r.cache);
                        if(cache) return arg(cache);
             
             if(verbose) {printf("x: ");println(x);}
                        
                        char *magic = show(apply2(iter,dummy,x),2);
             if(verbose) {printf("iter: ");println(iter);}
             if(verbose) printf("magic: %s\n",magic);
                        char *trick = (char*) malloc((strlen(magic)+10)*sizeof(char));
                        strcpy(trick, "\\dummy-> ");
                        strcat(trick, magic);
             if(verbose)printf("trick: %s\n",trick);
                        free(magic);
                
              if(verbose)println(parseParenthesis(trick));
                        image = interpret(parseParenthesis(trick),NULL,1);
             if(verbose){printf("preimage: ");println(image);}
                        free(trick);
                        
                        image = interpret(apply(image,self),NULL,0);
             if(verbose){printf("image: ");println(image);}
                        s->head->p.r.cache = push(dryApply(x,image),s->head->p.r.cache);
                        return arg(image);
        }
        
        atom* pre_f_fct(stack*sublist){return newRec(_,sublist->head);}
        atom pre_f = {{{pre_f_fct}}};

        atom* g = call(newRec(pre_f_fct, &pre_f), &delayCall);
        g->k = REC;
        g->p.r._ = g;
        g->p.r.iter = list->head;
        
        return g;
}

atom* eval(atom* f, atom* x){return call(f,arg(x))->p.r.arg;}

// Display

atom* getName(atom*f){
        stack* e = dictionary;
        while(e && e->head->p.a.x != f) e = e->rest;
        if(!e) printf("\n\n PANIC!! function not found\n\n");
        return e->head->p.a.f;
}

int showSize(atom*a,bool spaces){        
        int size=0;
        if(!a) size = strlen("Invalid syntax.");
        else if(a == undefined) size = strlen("undefined");
        else if(a == nil) size = strlen("[]");
        else if(a == emptyString) size = strlen("\"\"");

        else{
        
        switch(a->k){
                case ITEM: return stringSize(a);break;
                case STRING: return stringSize(a);break;
                case LIST: return listSize(a,spaces);break;
                case CHAR:size=3;break;
                case INT:size=13;break;
                case BOOL:size = (a->p.b?
                        strlen("true"):
                        strlen("false"));break;
                case APP:
                        size = strlen("(");
                        size += showSize(a->p.a.f, spaces);
                        if(spaces) size+=strlen(" ");
                        size += showSize(a->p.a.x, spaces);
                        size += strlen(")");
                        break;
                case CMBNTR_S:
                case CMBNTR_K:
                case CMBNTR_I:
                case CMBNTR_B:
                case CMBNTR_C:
                case FUN:size = apply(length,getName(a))->p.n;break;
                default:break;
               }
        }
        return size;
}

char* show(atom* a, bool spaces){
        char* result = (char*) malloc((1+showSize(a,spaces))*sizeof(char));
        char* sub;

        if(!a) sprintf(result, "Invalid syntax.");
        else if(a == undefined) sprintf(result, "undefined");
        else if(a == nil) sprintf(result, "[]");
        else if(a == emptyString) sprintf(result, "\"\"");

        else{
        
        switch(a->k){
                case ITEM: 
                        sub = showItem(a);
                        strcpy(result, sub);
                        free(sub);
                        break;
                case LIST: 
                        sub = showList(a,spaces-1);
                        strcpy(result, sub);
                        free(sub);
                        break;
                case STRING: 
                        sub = showString(a);
                        strcpy(result, sub);
                        free(sub);
                        break;
                case CHAR:sprintf(result,"'%c'",a->p.c);break;
                case INT:
                        if(a->p.n<0) sprintf(result,"(%d)",a->p.n); 
                        else sprintf(result,"%d",a->p.n);
                        break;
                case BOOL:(a->p.b?
                        sprintf(result,"true"):
                        sprintf(result,"false"));break;
                case APP:
                        strcpy(result,"(");
                        sub = show(a->p.a.f,spaces);
                        strcat(result, sub);
                        free(sub);
                        if(spaces) strcat(result," ");
                        sub = show(a->p.a.x,spaces);
                        strcat(result, sub);
                        free(sub);
                        strcat(result,")");
                        break;
                case CMBNTR_S:
                case CMBNTR_K:
                case CMBNTR_I:
                case CMBNTR_B:
                case CMBNTR_C:
                case FUN: 
                        sub = toString(getName(a));
                        strcpy(result, sub);
                        free(sub);
                        break;
                default:break;
               }
        }
        return result;
}

void print(atom *a){
        char* str = show(a,1);
        printf("%s",str);
        free(str);
}

void println(atom* a){
        print(a);
        printf("\n");
}

void compile(atom* a){
        printf("%s",show(a,0));
}

atom* name(atom*(*f)(stack*)){
        char* self = toString(f(NULL));
        char* str;
        if(self[0] == '_'){
                int i=1,j,n=0;
                int c;
                while(i<strlen(self)){
                        if(self[i]=='_') n++;
                        i++;
                }
                str = (char*) malloc((n+1)*sizeof(char));
                i=1;
                j=0;
                c=0;
                while(j<n){
                        if(self[i] ==  '_'){
                                str[j] = (char)c;
                                c = (char)0;
                                j++;
                        } else c = 10*c + self[i] - '0'; 
                        i++;
                }
                str[n] = '\0';
        } else{
                str = (char*) malloc((strlen(self)-3)*sizeof(char));
                strncpy(str, self, strlen(self)-4);
                str[strlen(self)-4] = '\0';
        }
        return fromString(str);
}

void stamp(atom *a, atom* method_name){
        atom* item = new();
        item->k = APP;
        item->p.a.f = method_name;
        item->p.a.x = a;
        dictionary = push(item,dictionary);
}

void declareFunction(atom **a, int arity, atom* (*method)(stack*)){
        (*a)->k = FUN;
        (*a)->p.f.arity = arity;
        (*a)->p.f.f = method;
        stamp(*a, name(method));
}

// Ordinary functions: 

atom* ifte_fct(stack*list){
        if(!list) return fromString(__func__);
        atom* result=NULL;
        if(list->head->k == BOOL){
                if(list->head->p.b) result = list->rest->head;
                else result = list->rest->rest->head;
        }
        empty(list);
        return result;
}

// (&&)
atom* _40_38_38_41_fct(stack* list){
        if(!list) return fromString(__func__);
        atom* p1 = list->head;
        atom* p2 = list->rest->head;
        atom* result = p1->p.b ? p2 : false;
        empty(list);
        return result;
}

atom* not_fct(stack* list){
        if(!list) return fromString(__func__);
        atom* p = list->head;
        if(p->k == APP) return NULL;
        atom* result = p->p.b ? false : true;
        empty(list);
        return result;
}

// (+)
atom* _40_43_41_fct (stack* list){
        if(!list) return fromString(__func__);
        atom *a1 = list->head;
        atom *a2 = list->rest->head;
        if(a1->k == APP || a2->k == APP) return NULL;
        atom* result = new();
        switch(a1->k){
                case INT:
                        switch(a2->k){
                                case INT:
                                        result->k = INT;
                                        result->p.n = a2->p.n + a1->p.n;
                                        break;
                                case CHAR:
                                        result->k = CHAR;
                                        result->p.c = a2->p.c + a1->p.n;
                                        break;
                        }
                        break;
               case CHAR:
                        switch(a2->k){
                                case INT:
                                        result->k = CHAR;
                                        result->p.c = a2->p.n + a1->p.c;
                                        break;
                                case CHAR:
                                        result = undefined;
                                        break;
                        }
                        break;
        }
        
        empty(list);
        return result;
}

atom* subtract_fct (stack* list){
        if(!list) return fromString(__func__);
        atom *a1 = list->head;
        atom *a2 = list->rest->head;
        if(a1->k == APP || a2->k == APP) return NULL;
        atom* result = new();
        switch(a1->k){
                case INT:
                        switch(a2->k){
                                case INT:
                                        result->k = INT;
                                        result->p.n = a2->p.n - a1->p.n;
                                        break;
                                case CHAR:
                                        result->k = CHAR;
                                        result->p.c = a2->p.c - a1->p.n;
                                        break;
                        }
                        break;
               case CHAR:
                        switch(a2->k){
                                case INT:
                                        result = undefined;
                                        break;
                                case CHAR:
                                        result->k = INT;
                                        result->p.n = (int)(a2->p.c - a1->p.c);
                                        break;
                        }
                        break;
        }
        
        empty(list);
        return result;
}

// (*)
atom* _40_42_41_fct (stack* list){
       if(!list) return fromString(__func__);
       atom *a1 = list->head;
       atom *a2 = list->rest->head;
       if(a1->k == APP || a2->k == APP) return NULL;
       atom* result = new();
       switch(a1->k){
                case INT:
                        switch(a2->k){
                                case INT:
                                        result->k = INT;
                                        result->p.n = a2->p.n * a1->p.n;
                                        break;
                                case CHAR:
                                        result->k = INT;
                                        result->p.n = (int)a2->p.c * a1->p.n;
                                        break;
                        }
                        break;
               case CHAR:
                        switch(a2->k){
                                case CHAR:
                                        result = undefined;
                                        break;
                                case INT:
                                        result->k = INT;
                                        result->p.n = a2->p.n * (int)a1->p.c;
                                        break;
                        }
                        break;
        }
        empty(list);
        return result;
}

// (^)
atom* _40_94_41_fct (stack* list){
        if(!list) return fromString(__func__);
        int x = list->head->p.n;
        int p = list->rest->head->p.n;
        int prod = 1;
        for(int i = 0; i < p; i++) prod *= x;
        
        atom* result = new();
        result->k = INT;
        result->p.n = prod;
        empty(list);
        return result;
}

atom* quot_fct (stack* list){
        if(!list) return fromString(__func__);
        if(list->head->k != INT || list->rest->head->k != INT) return NULL;
        if(list->rest->head->p.n == 0) return undefined;
        int quot = list->head->p.n / list->rest->head->p.n;
        
        atom* result = new();
        result->k = INT;
        result->p.n = quot;
        empty(list);
        return result;
}

atom* rem_fct(stack* list){
        if(!list) return fromString(__func__);
        if(list->head->k != INT || list->rest->head->k != INT) return NULL;
        if(list->rest->head->p.n == 0) return undefined;
        int rest = list->head->p.n % list->rest->head->p.n;
        
        atom* result = new();
        result->k = INT;
        result->p.n = rest;
        empty(list);
        return result;
}

// (<)
atom* _40_60_41_fct(stack*list){
        if(!list) return fromString(__func__);
        atom x = *list->head;
        atom y = *list->rest->head;
        atom* result = x.p.n < y.p.n ? true : false;
        empty(list);
        return result;
}

// (>)
atom* _40_62_41_fct(stack*list){
        if(!list) return fromString(__func__);
        atom x = *list->head;
        atom y = *list->rest->head;
        atom* result = x.p.n > y.p.n ? true : false;
        empty(list);
        return result;
}

// (==)
atom* _40_61_61_41_fct(stack*list){
        if(!list) return fromString(__func__);
        atom *a1 = list->head;
        atom *a2 = list->rest->head;
        if(!a1 || !a2) return false;
        if(a1 == undefined || a2 == undefined) return false;
        atom*result = false;
        
        switch(a1->k){
                case INT:
                        result = boolc(a1->p.n == a2->p.n);
                        break;
                case CHAR:
                        result = boolc(a1->p.c == a2->p.c);
                        break;
                case BOOL:
                        result = boolc(a1->p.b == a2->p.b);
                        break;
                case APP:
                        if (!a1->p.a.f || !a1->p.a.x || !a2->p.a.f || !a2->p.a.x)
                                return false;
                        result = apply2(and,
                                        apply2(equal,a1->p.a.f, a2->p.a.f),
                                        apply2(equal,a1->p.a.x, a2->p.a.x));
                        break;
                case CMBNTR_S:
                case CMBNTR_K: // for all K, const, "" are the same
                case CMBNTR_I:
                case CMBNTR_B:
                case CMBNTR_C:
                        result = boolc(a1->k == a2->k);
                        break;
                case FUN:
                        result = boolc(a1 == a2);
                        break;
                case LIST:
                case STRING:
                        result = boolc(apply(length,a1)->p.n == 
                                      apply(length,a2)->p.n && 
                                apply2(all,apply(uncurry,equal),
                                        apply(transpose,
                                                apply2(cons,
                                                        a2,
                                                        apply2(cons,
                                                                a1,
                                                                nil))))->p.b);
                        break;
                
        }
        empty(list);
        return result;
}

// (~)
atom* _40_126_41_fct(stack*list){
        if(!list) return fromString(__func__);
        atom*a1 = list->head;
        atom*a2 = list->rest->head;
        
        switch(a1->k){
                case STRING:
                        if(a2->k != STRING) return false;
                        return apply2(equal,apply(length,a1),apply(length,a2));
                        break;
                case LIST:
                        if(a2->k != LIST) return false;
                        return boolc(apply(length,a1)->p.n == 
                                      apply(length,a2)->p.n && 
                                apply2(all,apply(uncurry,equiv),
                                        apply(transpose,
                                                apply2(cons,
                                                        a2,
                                                        apply2(cons,
                                                                a1,
                                                                nil))))->p.b);
                        break;
        }
        return boolc(a1->k == a2->k);
}
 
// Recursive functions

atom* factorial_fct(stack *list) {
        if(!list) return fromString(__func__);
        atom* n = list->head;
        atom* result = n->p.n>1 ? 
                apply2(times, n, apply(factorial,intc(n->p.n - 1))) :
                intc(1);
        empty(list);
        return result;
}

atom* fibonacci_fct(stack* list) {
        if(!list) return fromString(__func__);
        int n = list->head->p.n;
        atom*result = n>1 ? 
                apply2(plus, 
                        apply(fibonacci,intc(n-1)), 
                        apply(fibonacci,intc(n-2))) : 
                intc(n);
        empty(list);
        return result;
}

// lists:

atom* head_fct(stack*list){ // head xs = xs undefined K
        if(!list) return fromString(__func__);
        if(list->head == undefined) return undefined;
        atom* result;
        if(list->head->k == CMBNTR_K) result = undefined;
        else result = list->head->p.a.x->p.a.f->p.a.x->p.a.x;
//        else result = apply2(list->head,undefined,K);
        empty(list);
        return result;
}

atom* tail_fct(stack*list){ // tail xs = xs undefined (K I)
        if(!list) return fromString(__func__);
        if(list->head == undefined) return undefined;
        atom* result;
        if(list->head->k == CMBNTR_K) result = undefined;
        else result = list->head->p.a.x->p.a.x;
        empty(list);
        return result;
}

// (:)
atom* _40_58_41_fct(stack* list){ // (:) x xs = \n c -> c h t
        if(!list) return fromString(__func__);
        atom*x = list->head;
        atom*xs = list->rest->head;
        
        atom* result = 
                apply(K,
                      apply(apply(C,apply(apply(C,I),x)),
                            xs));
        if(xs == emptyString || xs->k == STRING) 
                result-> k = STRING;
        else if(xs == nil || xs->k == LIST)
                result-> k = LIST;
        else if(x->k == ITEM || xs->k == ITEM)
                result-> k = ITEM;
        empty(list);
        return result;
}

atom* length_fct(stack*list){
        if(!list) return fromString(__func__);
        atom* xs = list->head;
        atom* result;
        if(xs->k == CMBNTR_K) result = intc(0);
        else if(xs->k != LIST && xs->k != STRING && xs->k != ITEM) 
                result = undefined;
        else result = apply2(plus,intc(1), 
                apply(length,apply(tail,xs)));
        
        empty(list);
        return result;
}

atom* null_fct(stack*list){
        if(!list) return fromString(__func__);
        atom* result = boolc(list->head->k == CMBNTR_K);
        empty(list);
        return result;
}

atom* foldr_fct(stack*list){
        if(!list) return fromString(__func__);
        atom* f = list->head;
        atom* acc = list->rest->head;
        atom* xs = list->rest->rest->head;
        atom*result = apply(isEmpty,xs)->p.b ? 
                acc : 
                apply2(f,
                        apply(head,xs),
                                apply3(foldr, f, acc, apply(tail,xs)));
        empty(list);
        return result;
}

atom *foldl_fct(stack*list){
        if(!list) return fromString(__func__);
        atom* f = list->head;
        atom* acc = list->rest->head;
        atom* xs = list->rest->rest->head;
        atom*result = apply(isEmpty,xs)->p.b ? 
                        acc : 
                        apply3(foldl,
                                f, 
                                apply2(f, acc, apply(head,xs)), 
                                apply(tail,xs));
        empty(list);
        return result;
}

atom* scanl_fct(stack*list){
        if(!list) return fromString(__func__);
        atom* f = list->head;
        atom* z = list->rest->head;
        atom* xs = list->rest->rest->head;
        atom* result = apply(isEmpty,xs)->p.b ? 
                apply2(cons,z,nil) : 
                apply2(cons,
                        z,
                        apply3(scanl, f,
                                apply2(f,
                                        z,
                                        apply(head,xs)),
                                apply(tail,xs)));
        empty(list);
        return result;
}

atom* map_fct(stack*list){
        if(!list) return fromString(__func__);
        atom* f = list->head;
        atom* xs = list->rest->head;
        if(xs->k == APP) return NULL;
        atom* result = apply(isEmpty,xs)->p.b ? 
                nil : 
                apply2(cons,
                        apply(f,apply(head,xs)),
                        apply2(map,f,apply(tail,xs)));
        empty(list);
        return result;
}

atom *take_fct(stack*list){
        if(!list) return fromString(__func__);
        atom* n = list->head;
        atom* xs = list->rest->head;
        atom* result = apply(isEmpty,xs)->p.b ?
                nil :
                (n->p.n == 0 ?
                        nil :
                        apply2(cons, 
                                apply(head,xs),
                                apply2(take,
                                        intc(n->p.n-1), 
                                        apply(tail,xs))));
        empty(list);
        return result;
}

atom *drop_fct(stack*list){
        if(!list) return fromString(__func__);
        atom* n = list->head;
        atom* xs = list->rest->head;
        atom* result = apply(isEmpty,xs)->p.b ?
                nil :
                (n->p.n == 0 ?
                        xs : apply2(drop,
                                        intc(n->p.n-1), 
                                        apply(tail,xs)));
        empty(list);
        return result;
}

atom* takeWhile_fct(stack*list){
        if(!list) return fromString(__func__);
        atom* f = list->head;
        atom* xs = list->rest->head;
        atom* result = apply(isEmpty,xs)->p.b ? 
                nil : 
                (!apply(f,apply(head,xs))->p.b ? 
                        nil : 
                        apply2(cons,
                                apply(head,xs), 
                                apply2(takeWhile,f, apply(tail,xs))));
        empty(list);
        return result;
}

atom* insert_fct(stack*list){
        if(!list) return fromString(__func__);
        atom* n = list->head;
        atom* x = list->rest->head;
        atom*xs = list->rest->rest->head;
        atom*result = apply2(append,apply2(append,apply2(take,n,xs),
                                    apply2(cons,x,nil)),
                                    apply2(drop,n,xs));
        empty(list);
        return result;
}

atom* permutations_fct(stack*list){
        if(!list) return fromString(__func__);
        atom*xs = list->head;
        
        atom* sow = parse(
                "\\ x xs -> map (\\ n -> insert n x xs) $ "
                                 "scanl (+) 0 $ take (length xs) $ repeat 1");
        atom* result = apply(isEmpty,xs)->p.b ? apply2(cons,nil,nil) : 
                apply(concat, apply2(map,apply(sow,apply(head,xs)),
                        apply(permutations,apply(tail,xs))));
        empty(list);
        return result;
}

atom* filter_fct(stack*list){
        if(!list) return fromString(__func__);
        atom* f = list->head;
        atom* xs = list->rest->head;
        atom* result = apply(isEmpty,xs)->p.b ? 
                nil : 
                apply((apply(f,apply(head,xs))->p.b ? 
                                apply(cons,apply(head,xs)) : 
                                I), 
                        apply2(filter,f,apply(tail,xs)));
        empty(list);
        return result;
}

atom* elem_fct(stack*list){
        if(!list) return fromString(__func__);
        atom* x = list->head;
        atom* comp = list->rest->head;
        atom* xs = list->rest->rest->head;
        atom* result = apply(isEmpty,xs)->p.b ? 
                false : 
                (apply2(comp,x,apply(head,xs))->p.b ? 
                        true : 
                        apply3(elem,x,comp,apply(tail,xs)));
        empty(list);
        return result;
}

// (!!)
atom* _40_33_33_41_fct(stack*list){
        if(!list) return fromString(__func__);
        atom* xs = list->head;
        //if(xs->k == APP) return NULL;
        int n = list->rest->head->p.n;
        
        
        atom* result;
        if(xs->k == CMBNTR_K) result = undefined;
        else if(xs->k != LIST && xs->k != STRING) result = undefined;
        else if(n >= apply(length,xs)->p.n) result = undefined;
        else result = apply(last,apply2(take,intc(n+1),xs));
        
        empty(list);
        return result;
}       

atom* transpose_fct(stack*list){
        if(!list) return fromString(__func__);
        atom*xs = list->head;
        atom* result = apply2(any,isEmpty,xs)->p.b ? nil : apply2(cons,
                apply2(map,head,xs),
                  apply(transpose,apply2(map,tail,xs)));
        empty(list);
        return result;
}

atom* uncurry_fct(stack*list){
        if(!list) return fromString(__func__);
        atom* f = list->head;
        atom* xs = list->rest->head;
        while(xs -> k != CMBNTR_K){
                f = apply(f, apply(head,xs));
                xs = apply(tail,xs);
        }
        empty(list);
        return f;
}

int listSize(atom*list, bool spaces){
        int size;
        if(apply(isEmpty,list)->p.b) {
                return strlen("[]");
        }
        
        atom *prompt = list; 
        size = strlen("[");
        do{
                size += showSize(apply(head,prompt),spaces+1);
                prompt = apply(tail,prompt);
                if(! apply(isEmpty,prompt)->p.b) size+=strlen(",");
                if(spaces) size += strlen(" ");
        } while(! apply(isEmpty,prompt)->p.b);
        size+=strlen("]");
        return size;
}

char* showList(atom* list, bool spaces){
        char* result = (char*) malloc((1+listSize(list,spaces))*sizeof(char));
        if(apply(isEmpty,list)->p.b) {
                sprintf(result,"[]");
                return result;
        }
        
        atom *prompt = list; 
        sprintf(result,"[");
        do{
                strcat(result,show(apply(head,prompt),spaces+1));
                prompt = apply(tail,prompt);
                if(! apply(isEmpty,prompt)->p.b) strcat(result,",");
                if(spaces) strcat(result," ");
        } while(! apply(isEmpty,prompt)->p.b);
        strcat(result,"]");
        return result;
}

// streams:

atom* repeat_fct(stack*list){
        if(!list) return fromString(__func__);
        atom* arg = list->head;        
        atom* p_repeat = 
                apply(K,apply(apply(C,apply(apply(C,I),arg)),undefined));
        p_repeat->p.a.x->p.a.x = p_repeat;
        empty(list);
        return p_repeat;
}

atom* cycle_fct(stack*list){
        if(!list) return fromString(__func__);
        atom* xs = list->head;
        atom *sx = apply(reverse,xs);
        
        atom* p_cycle = apply(K,apply(apply(C,apply(apply(C,I),
                                        apply(head,sx))),undefined));
        atom* tie = p_cycle->p.a.x;
        
        sx = apply(tail,sx);
            
        while(!apply(isEmpty,sx)->p.b){    
                p_cycle = apply(K,apply(apply(C,apply(apply(C,I),
                                apply(head,sx))),p_cycle));
                sx = apply(tail,sx);
          } 
          
        tie->p.a.x = p_cycle;
        empty(list);
        return p_cycle;
}

atom* randint_fct(stack*list){
        if(!list) return fromString(__func__);
        atom* max = list->head;
        empty(list);
        atom* result = intc(rand() % max->p.n);
        return result;
}

// strings :

int stringSize(atom*list){
        int size = 0;
        if(apply(isEmpty,list)->p.b) {
                return strlen("\"\"");
        }
        
        atom *prompt = list; 
        size = strlen("\"");
        do{
                size += strlen((char[2]) { apply(head,prompt)->p.c, '\0' });
                prompt = apply(tail,prompt);
        } while(! apply(isEmpty,prompt)->p.b);
        size += strlen("\"");
        return size;
}

char* showString(atom*list){
        char* result = (char*) malloc((1+stringSize(list))*sizeof(char));
        if(apply(isEmpty,list)->p.b) {
                sprintf(result,"\"\"");
                return result;
        }
        
        atom *prompt = list; 
        sprintf(result,"\"");
        do{
                strcat(result,(char[2]) { apply(head,prompt)->p.c, '\0' });
                prompt = apply(tail,prompt);
        } while(! apply(isEmpty,prompt)->p.b);
        strcat(result,"\"");
        return result;
}

char* showItem(atom*list){
        char* result = (char*) malloc((1+stringSize(list))*sizeof(char));
        if(apply(isEmpty,list)->p.b) {
                sprintf(result,"``");
                return result;
        }
        
        atom *prompt = list; 
        sprintf(result,"`");
        do{
                strcat(result,(char[2]) { apply(head,prompt)->p.c, '\0' });
                prompt = apply(tail,prompt);
        } while(! apply(isEmpty,prompt)->p.b);
        strcat(result,"`");
        return result;
}

// TEXT PARSING:

void displayStack(stack*list){
        printf("[");
        do{
                print(list->head);
                list=list->rest;
                if(list) printf(",");
        } while(list);
        printf("]");
}

bool isparen(char c){ return c==')' || c=='(';}
bool isbracket(char c){ return c==']' || c=='[';}
bool iscomma(char c){ return c==',';}
bool isdollar(char c){ return c=='$';}
bool isbslash(char c){ return c=='\\';}
bool issyntax(char c) {
        return isparen(c) || isbracket(c) || iscomma(c) ||
               isdollar(c) || isbslash(c);
}
bool isoperatoreligible(char c){ return ispunct(c)
        && !issyntax(c) 
        && c!='"'
        && c!='\n'
        && c!='_'
        && c!='`'
        && c!='\'';}

// normal ending with NULL, otherwise error
atom* next(int *n, char*str){
        while(*n>=0 && (isspace(str[*n]) || str[*n]=='`' || str[*n]=='\n')) (*n)--;
        if (*n<0) return NULL;
        if(issyntax(str[*n])) return charc(str[*n]);
        
        atom* item = nil;
        
        if(str[*n]=='\'' && str[*n-1]!='\'' && str[*n-2]=='\''){
                do {   
                        item = apply2(cons,charc(str[*n]),item);
                        (*n)--;
                } while(*n>=0 && str[*n]!='\'');
                item = apply2(cons,charc(str[*n]),item);
                if(*n>=0) (*n)--;
        }
        
        else if(str[*n]=='"'){
                 do {   
                        if(str[*n] != '\\') 
                                item = apply2(cons,charc(str[*n]),item);
                        (*n)--;
                } while(!((*n>0 && str[*n-1]!='\\' && str[*n]=='"') 
                        || (*n==0 && str[*n]=='"')));
                item = apply2(cons,charc(str[*n]),item);
                if(*n>=0) (*n)--;
        }
        
        else if(isoperatoreligible(str[*n])) {
                while(*n>=0 && isoperatoreligible(str[*n])) {
                        item = apply2(cons,charc(str[*n]),item);
                        (*n)--;
                }
         }
        
        else if(isdigit(str[*n]) || isalnum(str[*n]) || str[*n]=='\''
                            || str[*n]=='_') {
                while(*n>=0 && (isdigit(str[*n]) || 
                        isalnum(str[*n]) || str[*n]=='\'' || str[*n]=='_')){
                        item = apply2(cons,charc(str[*n]),item);
                        (*n)--;
                }
        }
        else return error;

        if(*n>=0) (*n)++;
        item->k = ITEM;
        return item;
}

bool isClosing(atom* a){return a->k==CHAR && a->p.c==')';}
bool isOpening(atom* a){return a->k==CHAR && a->p.c=='(';}
bool isNotParen(atom* a){return !isClosing(a) && !isOpening(a);}
bool isDigit(atom* a){return isdigit(a->p.c);}
bool isMinus(atom* a){return a->k==CHAR && a->p.c == '-';}
bool isQuoteChar(atom* a){return a->k==CHAR && a->p.c == '\'';}
bool isQuoteString(atom* a){return a->k==CHAR && a->p.c == '"';}
bool isBackSlash(atom* a){return a->k==CHAR && a->p.c == '\\';}
bool isOpeningSquare(atom* a){return a->k==CHAR && a->p.c == '[';}
bool isClosingSquare(atom* a){return a->k==CHAR && a->p.c == ']';}
bool isComma(atom* a){return a->k==CHAR && a->p.c == ',';}

bool isString(char*str,atom*a){
        int i=0;
        while(i<strlen(str) && a->k != CMBNTR_K && apply(head,a)->p.c == str[i]){
                i++;
                a=apply(tail,a);
        }
        return i==strlen(str) && a->k == CMBNTR_K;
}

bool isItem(char*str,atom*a){
        if(a->k != ITEM) return 0;
        return isString(str,a);
}

bool beginsWith(char* str, atom* a){
        int i=0;
        while(i<strlen(str) && apply(head,a)->p.c == str[i]){
                a = apply(tail,a);
                i++;
        }
        return i == strlen(str);
}

bool areSameStrings(atom*s1,atom*s2){
        while(s1->k != CMBNTR_K 
           && s2->k != CMBNTR_K 
           && apply(head,s1)->p.c == apply(head,s2)->p.c){
                s1=apply(tail,s1);
                s2=apply(tail,s2);
        }
        return s1->k == CMBNTR_K && s2->k == CMBNTR_K;
}

bool areSameItems(atom*s1,atom*s2){
        if(s1->k != ITEM) return 0;
        if(s2->k != ITEM) return 0;
        return areSameStrings(s1,s2);
}

atom* toItem(const char* str){
        atom*a = fromString(str);
        a->k = ITEM;
        return a;
}

stack* itemCorrespondingTo(char* ref, char* token, stack*list){
        int nr=1, nt=0;
        while(list->rest && nr != nt){
                list = list->rest;
                if(isItem(ref,list->head)) nr++;
                if(isItem(token,list->head)) nt++;
        }
        return list;
}

stack* characterCorrespondingTo(char ref, char token, stack*list){
        int nr=1, nt=0;
        while(list->rest && nr != nt){
                list = list->rest;
                if(list->head->k == CHAR && ref==list->head->p.c) nr++;
                if(list->head->k == CHAR && token == list->head->p.c) nt++;
        }
        return list;
}

stack*endScope(stack*pos){
        int no=0, nc=0;
        do {
                pos = pos->rest;
                if(pos && isOpening(pos->head)) no++;
                if(pos && isClosing(pos->head)) nc++;
        }while(pos && no>=nc);
        return pos;
}

atom* open;
atom* clos;

stack* unsugarDollars(stack*list){
        stack*pos=list;
        while(pos && (pos->head->k != CHAR || pos->head->p.c != '$'))
                pos = pos->rest;

        if(pos){
                pushBefore(endScope(pos),clos,list);
                // this ends recursion too:
                pos->head->p.c = '(';
                return unsugarDollars(list);
        }
        return list;
}

stack* unsugarIfs(stack*list){
        stack*pos = list;
        while(pos && !isItem("if",pos->head))
                pos = pos->rest;
        
        if(pos){
                // to end recursion
                pos->head = apply2(cons,charc('i'),
                            apply2(cons,charc('f'),
                            apply2(cons,charc('t'),
                            apply2(cons,charc('e'),nil))));
                pos->head->k = ITEM;
                pushBefore(pos->rest,open,list);
                stack* th = itemCorrespondingTo("if","then",pos);
                pushBefore(th->rest,open,list);
                substituteStack(th,clos,list);
                stack* el = itemCorrespondingTo("if","else",pos);
                pushBefore(el->rest,open,list);
                // because of the temporary added parens:
                pushBefore(endScope(el->rest),clos,list);
                substituteStack(el,clos,list);
                
                return unsugarIfs(list);
        }
        return list;
}

atom* dress(atom* a){
         a = apply2(cons,open,a);
         a = apply2(append,a,apply2(cons,clos,nil));
         a->k = ITEM;
         return a;
}

atom* undress(atom *a){
        a = apply(init,a);
        if(a->k == CMBNTR_K) return NULL;
        a = apply(tail,a);
        if(a->k == CMBNTR_K) return NULL;
        a->k = ITEM;
        return a;
}

bool isUndressedOperator(atom*a){
        if(!a || a->k != ITEM) return 0;
        while((a->k == ITEM || a->k != CMBNTR_K) 
                && isoperatoreligible(apply(head,a)->p.c)
                && isNotParen(apply(head,a)))
                a = apply(tail,a);
        return a->k == CMBNTR_K;
}

bool isDressedOperator(atom*a){
        if(a->k != ITEM) return 0;
        return isOpening(apply(head,a)) 
                && isUndressedOperator(undress(a))
                && isClosing(apply(last,a));
}

int correspondingOpen(stack*back,int b,int e){
        int no = 0, nc = 1;
        b--;
        do{
                b++;
                if(isOpening(nthStack(b,back))) no++;
                if(isClosing(nthStack(b,back))) nc++;
        }while(b<e-1 && (!isOpening(nthStack(b,back)) || no != nc));
        return b;
}

atom* dryApply(atom* f, atom* x){
        atom* a = new();
        a->k = APP;
        a->p.a.f = f;
        a->p.a.x = x;
        return a;
}

atom* unsugarOperators(atom*a){
        if(a->k != APP) return a;
        atom* tmp;
        // now it's APP
        
        // special case : negative numbers
        if(isUndressedOperator(a->p.a.f) && isMinus(apply(head,a->p.a.f))){
                atom*neg = apply2(cons,charc('-'),a->p.a.x);
                return neg;
        }
        
        if(isUndressedOperator(a->p.a.f)){
                a->p.a.f = dryApply(C,dress(a->p.a.f));
                a->p.a.x = unsugarOperators(a->p.a.x);
                return a;
        }

        if(isUndressedOperator(a->p.a.x)){
                tmp = a->p.a.f;
                a->p.a.f = dress(a->p.a.x);
                a->p.a.x = unsugarOperators(tmp);
                return a;
        }

        a->p.a.f = unsugarOperators(a->p.a.f);
        a->p.a.x = unsugarOperators(a->p.a.x);
                
        return a;
}

atom* splitText(stack* back, int b, int e){
        if(isNotParen(nthStack(b,back))){
                if(e>b+1) return dryApply(splitText(back,b+1,e),
                                          nthStack(b,back));
                return nthStack(b,back);
         }
        // it is closing now
        int co = correspondingOpen(back,b+1,e);
        atom*a = splitText(back,b+1,co);
        if(isUndressedOperator(a))  a=dress(a);
        if(e>co+1) return dryApply(splitText(back,co+1,e),a);
        return a;
}

atom* toInt(atom* a){
        int n = 0;
        int sign = 1;
        if(isMinus(apply(head,a))){
                sign = -1;
                a = apply(tail,a);
        }
        while(a->k != CMBNTR_K && isdigit(apply(head,a)->p.c)){
                n = 10 * n + apply(head,a)->p.c - '0';
                a = apply(tail,a);
        }
        if(a->k != CMBNTR_K) return NULL;
        return intc(n*sign);
}

atom * parseParenthesis(char*);
atom* FreeFrom(atom* , atom* );

atom* aggregate(stack* pos, stack* end){
        atom* conc = nil;
        while(pos != end) {
                if(pos->head->k == CHAR)
                        conc = apply2(cons,
                                apply2(cons,pos->head,nil),
                                conc);
                else conc = apply2(cons,pos->head,conc);
                conc = apply2(cons,apply2(cons,charc(' '),nil),conc);
                pos = pos->rest;
        } 

        atom* isol = apply(concat,apply(reverse,conc));
        isol->k = ITEM;
        return isol;
}

char* contextualize(atom *exp, stack* ctx){
        char* expression = toString(exp);
        if(ctx){
                char* vars = toString(aggregate(ctx,NULL));
                int len = strlen(expression) + strlen(vars) + 7;
                char* abstraction = (char*) malloc(len*sizeof(char));
                strcpy(abstraction,"\\ ");
                strcat(abstraction,vars);
                strcat(abstraction," -> ");
                strcat(abstraction,expression);
                return abstraction;
       }
       return expression;
}

atom* translate(atom*str){
        if(isDigit(apply(head,str)) || isMinus(apply(head,str))) 
                return toInt(str);
        else if(isQuoteChar(apply(head,str))) 
                return charc(apply(head,apply(tail,str))->p.c);
        else if(isQuoteString(apply(head,str)))
        {
                if(isQuoteString(apply(head,apply(tail,str)))) 
                        return emptyString;
                atom* string = apply(tail,apply(init,str));
                atom*sub = string;
                while(sub->k != CMBNTR_K){
                        sub->k = STRING;
                        sub = sub->p.a.x->p.a.x;
                }
                return string;
        }
        else{
                stack* fun = dictionary;
                while(fun && !areSameStrings(str, fun->head->p.a.f)) fun = fun->rest;
                if(!fun) {print(str); printf(" not found.\n");}
                if(fun) return fun->head->p.a.x;
        }
        return NULL;
}

atom* interpret(atom*a, stack* ctx, bool soft){

        if(!a) return NULL;
        if(a->k == ITEM) return soft ? a : translate(a);
        if(a->k == APP)
               return apply(interpret(a->p.a.f,ctx,soft), 
                             interpret(a->p.a.x,ctx,soft));
        return a;
}

stack* okParenthesis(stack*list){
        int no=0, nc=0;
        
        stack* pos = list;
        stack*last = NULL;
        while(pos && no>=nc){
                
                if(isOpening(pos->head)) no++;
                if(isClosing(pos->head)){
                        nc++;
                        if(last && isOpening(last->head))
                                 return NULL;
                }
                last = pos;
                pos = pos->rest;
        }
        if(pos || no != nc) return NULL;
        return list;
}

stack* flattenAtom(atom* a){
        if(a->k == APP){
                stack* app = push(charc(')'),NULL);
                app = appendStacks(flattenAtom(a->p.a.x), app);
                app = appendStacks(flattenAtom(a->p.a.f), app);
                app = push(charc('('),app);
                return app;
        }
        return push(a,NULL);
}

atom* atomize(stack* , stack*, bool);

stack* unsugarLets(stack* list, stack*context, bool soft){        
        stack*components = list;
        while(components && !isItem("let",components->head))
                components=components->rest;
                
        if(components){
                stack*end = endScope(components);
                stack* in = itemCorrespondingTo("let", "in", components);
                if(!in) return NULL;
                
                stack* variables = NULL;
                stack* expressions = NULL;
        
                stack *before = components->rest;
                stack* pos = before->rest;
                stack* tagb = NULL, *tage;
                int nl = 0, ni = 0;
        
                do {
                        if(isItem("let", pos->head)) nl++;
                        if(isItem("in", pos->head)) ni++;
                
                        if(nl<=ni){
                                if(isItem("=",pos->head)){
                                        if(pos->rest == in) return NULL;
                                        if(pos == components->rest->rest)
                                                tagb = pos->rest;
                                        else {
                                                tage = before;
                                                if(!tagb || tagb == in) return NULL;
                                                expressions = push(aggregate(tagb,tage),
                                                           expressions);
                                                tagb = pos->rest;
                                        }
                                        variables = push(before->head,variables);
                                        tage = NULL;
                                }
                                if(pos->rest == in){
                                        tage = in;
                                        if(!tagb) return NULL;
                                        expressions = push(aggregate(tagb,tage),
                                                   expressions);
                                }
                        }
                
                        before = pos;
                        pos = pos->rest;
                        if(!pos) return NULL;
                } while(pos != in);
        
                if(!variables || !expressions) return NULL;
                stack* newcontext = appendStacks(variables, context);

                // this is what's after the 'in':
                if(!pos->rest) return NULL;
                char* abstraction = contextualize(aggregate(pos->rest,end), newcontext);
 
                atom* result;
                result = interpret(parseParenthesis(abstraction),newcontext,soft);
                free(abstraction);
                if(!result) return NULL;

                atom* subst;
                // shallow finalizing
                if(!context){
                        while(expressions){
                                // just apply arguments
                                subst =  interpret(parseParenthesis(toString(expressions->head)),newcontext,0);
                                if(subst) result = apply(result,subst); 
                                else return NULL;
                                expressions = expressions->rest;
                        }
                        return replaceStack(components,end,flattenAtom(result),list);
                }
                
                // deep finalizing
                stack *ctx, *nctx;  
                char* subexp;
                atom* arg;
                nctx = newcontext;
                // for each variable
                while(nctx){
                        // either it has an expression attached
                        if(expressions){
                                subexp = contextualize(expressions->head,context);
                                arg = interpret(parseParenthesis(subexp),context,soft);
                                if(!arg) return NULL;
                
                                ctx = context;
                                while(ctx){
                                        // in which case it must be used instead
                                        arg = dryApply(arg,ctx->head);
                                        ctx = ctx->rest;
                                }
                        // or not
                        } else arg = nctx->head;

                        result = dryApply(result, arg);
                        if(expressions) expressions = expressions->rest;
                        nctx = nctx->rest;
                }
                return replaceStack(components,end,flattenAtom(result),list);
        }
        
        return list;
}

stack* unsugarSquareBrackets(stack* components, stack*context, bool soft){
        
        atom*item;
        stack*pos = components;
        while(pos && (pos->head->k != CHAR || !isOpeningSquare(pos->head))) 
                pos=pos->rest;
                
        if(pos){
                stack* end = characterCorrespondingTo('[',']',pos);
                 if(!end) return NULL;
                stack*beg = pos;
 if(verbose){printf("\nbeg: ");displayStack(beg);printf("\n");}     
 if(verbose){printf("\nend: ");displayStack(end);printf("\n");}     
                atom* list = soft ? toItem("[]") : nil;
                if(pos != end){
                        int nc = 0, no = 1;
                                
                        do{     
                                pos=pos->rest;
                                if(pos && isOpeningSquare(pos->head)) no++;
                                if(pos && isClosingSquare(pos->head)) nc++;
                                
                        } while(pos && !((isComma(pos->head) && nc == no-1) || 
                                         (isClosingSquare(pos->head) && no == nc) ));

                        if(pos != beg->rest){
                                stack* sub = subStack(beg->rest, pos, components); 
                                atom*temp = atomize(sub,context,soft); 
                                item =  interpret(temp,context,soft);
    if(verbose){printf("\nitem: ");println(item);}     
                     
                                if(!isClosingSquare(pos->head)){
                                        pos = substituteStack(pos,charc('['),components);
  if(verbose){printf("\n`,`->`[`: ");displayStack(components);printf("\n");}     
         
                                        sub = subStack(pos,end->rest,components);
   if(verbose){printf("\n[sub]: ");displayStack(sub);printf("\n");}
                                        temp = atomize(sub,context,soft);
   if(verbose){printf("\ntemp: ");println(temp);}
                                        list = interpret(temp,context,soft);
   if(verbose){printf("\nlist: ");println(list);}
                                }
                        /*if(soft)*/ list = dryApply(dryApply(toItem("(:)"),item),list);
//                        else list = apply2(cons,item,list);
                        }
                 }
                stack*finally = replaceStack(beg,end->rest,flattenAtom(list),components);
   if(verbose){printf("\nfinally: ");displayStack(finally);printf("\n");}
                 
                return finally;
        } 
        return components;
}

stack* unAbstracted(stack* components, stack*context){
        stack* variables;
        atom* exp;
        stack*pos = components;
        stack*beg;
        while(pos && (pos->head->k != CHAR || !isbslash(pos->head->p.c))) pos=pos->rest;
        
        if(pos){
                beg=pos;
                pos=pos->rest;
                variables = NULL;
        
                while(pos && !isItem("->",pos->head)){
                        variables = push(pos->head,variables);
                        pos=pos->rest;
                }
                
                if(pos && isItem("->", pos->head)) pos = pos->rest;
                else return NULL;

if(verbose){printf("\nvars: ");displayStack(variables);printf("\n");}               
               stack* end = endScope(beg);
               
               stack* sub = subStack(pos, end, components);
if(verbose){printf("\nsub: ");displayStack(sub);printf("\n");}               
               
               atom*temp = atomize(sub,variables,1);
if(verbose){printf("\ntemp: ");println(temp);} 
               exp =  interpret(temp,variables,1);
               if(!exp) return NULL;
if(verbose){printf("\nexp: ");println(exp);} 
                while(variables){
                        exp = FreeFrom(variables->head,exp);
                        variables = variables->rest;
                }
if(verbose){printf("\nfreed: ");println(exp);} 
                empty(variables);
                return replaceStack(beg,end,flattenAtom(exp),components);
        }
        return components;
}

stack*itemize(char* str){
        int n = strlen(str)-1;
        stack* components = NULL;
        atom* component;
        while(n>=0){
                component = next(&n,str);
    if(verbose && component != error && component != NULL) println(component);
                if(component != error && component != NULL) 
                        components = push(component,components);
                        
                if(component == error) return NULL;
                n--;
        }
        
        if(!okParenthesis(components)) return NULL;
        return components;
}

atom* atomize(stack* components, stack* context, bool soft){
        
        if(verbose) {printf("\ncomponents: ");displayStack(components); printf("\nun$: ");}

        stack* unDollared = unsugarDollars(components);
        if(verbose) {displayStack(unDollared); printf("\nunIf: ");}
        
        stack* unIfed = unsugarIfs(unDollared);
        if(verbose) {displayStack(unIfed); printf("\n");}
        
        stack*draft = unIfed;
        stack*pos;
        bool dirty = 1;
        while(dirty){
if(verbose) {printf("\nnew turn: ");displayStack(draft); printf("\n");}
                pos=draft;
                while(pos){
                        if(pos->head->k == CHAR && isOpeningSquare(pos->head)){
                                draft = unsugarSquareBrackets(draft, context, soft);
                                if(verbose) {printf("unBracket: ");displayStack(draft); printf("\n");}
                                break;
                        } else if(pos->head->k == CHAR && isBackSlash(pos->head)){
                                draft = unAbstracted(draft, context);
                                if(verbose) {printf("concrete: ");displayStack(draft); printf("\n");}
                                break;
                        } else if(pos->head->k == ITEM && isItem("let",pos->head)){
                                draft = unsugarLets(draft, context, soft);
                                if(verbose) {printf("unLetted: ");displayStack(draft); printf("\n");}
                                break;
                        }
                        pos = pos->rest;
                }
                if(!pos) dirty = 0;
        }
        
        stack* back = reverseStack(draft);
        if(verbose) {printf("\nreverse: ");displayStack(back); printf("\n");}
        
        atom* result = splitText(back,0,lengthStack(back));
        if(verbose) {printf("\nsplit: "); println(result);  printf("\n");}
        
        result = unsugarOperators(result);
        if(verbose){printf("\nunOp: "); println(result); printf("\n");}

        empty(draft);
        empty(back);
        return result;
}

atom * parseParenthesis(char* str){
        stack* components = itemize(str);
        return atomize(components,NULL,0);
}

bool onlySpace(char*command){
        for(int i = 0; i< strlen(command); i++)
                if (!isspace(command[i])) return 0;
        return 1;
}

bool Bound(atom* x, atom* exp){
        if(exp->k == APP) return Bound(x, exp->p.a.f) || Bound(x, exp->p.a.x);
        if(exp->k == FUN) return 0;
        return areSameItems(x,exp);
}

atom* FreeFrom(atom* x, atom* exp){
        if(exp->k == APP){
                if(!Bound(x, exp->p.a.f) && !Bound(x, exp->p.a.x)) 
                        return apply(toItem("const"),exp);
                if(!Bound(x, exp->p.a.f) && areSameItems(x, exp->p.a.x)) 
                        return exp->p.a.f;
                if(!Bound(x, exp->p.a.f) && Bound(x, exp->p.a.x)) 
                        return apply2(toItem("(.)"), exp->p.a.f, FreeFrom(x, exp->p.a.x));
                if(Bound(x, exp->p.a.f) && !Bound(x, exp->p.a.x)) 
                        return apply2(toItem("flip"), FreeFrom(x, exp->p.a.f), exp->p.a.x);
                return apply2(toItem("ap"), 
                        FreeFrom(x, exp->p.a.f), 
                        FreeFrom(x, exp->p.a.x));
        }
        if(areSameItems(x, exp)) return toItem("id");
        return apply(toItem("const"),exp);
}

atom* parse(char* expression){
        return interpret(parseParenthesis(expression), NULL, 0);
}

FILE* history_hndl;
char*history[HISTORY_ENTRIES+1];
char history_path[200];
int history_read_index;
int history_write_index = 0;
int checkIndex(int x){return (x+HISTORY_ENTRIES) % HISTORY_ENTRIES;}

void initialize(){ // !!ORDER MATTERS!!

        // some constants
        
        error = new(); 
        open = charc('(');
        clos = charc(')');

        // _cons_IN_FIRST_POSITION_ :
        // ------------------------

        cons = new();        declareFunction(&cons,        2,  _40_58_41_fct);

        // primitive fonctions :
        // -------------------

        tail=new();          declareFunction(&tail,        1,  tail_fct);
        length=new();        declareFunction(&length,      1,  length_fct);
        plus=new();          declareFunction(&plus,        2,  _40_43_41_fct);
        times=new();         declareFunction(&times,       2,  _40_42_41_fct);
        isEmpty = new();     declareFunction(&isEmpty,     1,  null_fct);
        head=new();          declareFunction(&head,        1,  head_fct);
        foldr=new();         declareFunction(&foldr,       3,  foldr_fct);
        foldl=new();         declareFunction(&foldl,       3,  foldl_fct);
        scanl=new();         declareFunction(&scanl,       3,  scanl_fct);
        map=new();           declareFunction(&map,         2,  map_fct); 
        take=new();          declareFunction(&take,        2,  take_fct);
        drop=new();          declareFunction(&drop,        2,  drop_fct);
        takeWhile=new();     declareFunction(&takeWhile,   2,  takeWhile_fct);
        repeat=new();        declareFunction(&repeat,      1,  repeat_fct);
        cycle=new();         declareFunction(&cycle,       1,  cycle_fct);
        insert=new();        declareFunction(&insert,      3,  insert_fct);
        permutations=new();  declareFunction(&permutations,1,  permutations_fct);
        ifte=new();          declareFunction(&ifte,        3,  ifte_fct);
        equal=new();         declareFunction(&equal,       2,  _40_61_61_41_fct);
        lessThan=new();      declareFunction(&lessThan,    2,  _40_60_41_fct);
        greaterThan=new();   declareFunction(&greaterThan, 2,  _40_62_41_fct);
        not=new();           declareFunction(&not,         1,  not_fct);
        and=new();           declareFunction(&and,         2,  _40_38_38_41_fct);
        intPower=new();      declareFunction(&intPower,    2,  _40_94_41_fct);
        quot=new();          declareFunction(&quot,        2,  quot_fct);
        rem=new();           declareFunction(&rem,         2,  rem_fct);
        subtract=new();      declareFunction(&subtract,    2,  subtract_fct);
        factorial=new();     declareFunction(&factorial,   1,  factorial_fct);
        fibonacci=new();     declareFunction(&fibonacci,   1,  fibonacci_fct);
        filter=new();        declareFunction(&filter,      2,  filter_fct);
        elem=new();          declareFunction(&elem,        3,  elem_fct);
        nth=new();           declareFunction(&nth,         2,  _40_33_33_41_fct);
        transpose=new();     declareFunction(&transpose,   1,  transpose_fct);
        uncurry=new();       declareFunction(&uncurry,     2,  uncurry_fct);
        fix=new();           declareFunction(&fix,         1,  fix_fct);
        randint=new();       declareFunction(&randint,     1,  randint_fct);
        equiv=new();         declareFunction(&equiv,       2,  _40_126_41_fct);
        dummy = new();       declareFunction(&dummy,     999,  dummy_fct);
        
        // basic combinators (!!! WARNING : fromString always after cons definition !!!)
        // -----------------
        
        stamp(S, fromString("ap"));
        stamp(K, fromString("const"));
        stamp(I, fromString("id"));
        stamp(B, fromString("(.)"));
        stamp(C, fromString("flip"));
        stamp(true, fromString("true"));
        stamp(false, fromString("false"));
        stamp(nil, fromString("[]"));
        undefined = new(); stamp(undefined, fromString("undefined"));
        

        // combinate building functions:
        // ----------------------------
        
        append = apply(C,apply(foldr,cons));                         stamp(append, fromString("(++)"));
        reverse = apply2(foldl,apply(C,cons),nil);                   stamp(reverse, fromString("reverse"));
        init = apply2(B,reverse,apply2(B,tail,reverse));             stamp(init, fromString("init"));
        concat = apply2(foldr,append,nil);                           stamp(concat, fromString("concat"));
        
        // interpreted functions (!!! WARNING : parse always after 
        // ---------------------
        // append, 
        // (reverse), init 
        // concat definitions !!!):

        pair = parse("\\ a b -> [a, b]");                                    stamp(pair, fromString("(,)"));
        or = parse("\\ p q -> if p then true else q");                       stamp(or, fromString("(||)"));
        scanr = apply(parse("\\ aux f acc -> foldr (aux f) [acc]"), 
                parse("\\ f x xs -> (f x (head xs)) : xs"));                 stamp(scanr, fromString("scanr"));
        predicate = parse("\\ cond p q x -> if cond x then p x else q x");   stamp(predicate, fromString("predicate"));
        any = parse("\\ p -> foldr ((||) . p) false");                       stamp(any, fromString("any"));
        all = parse("\\ p -> foldr ((&&) . p) true");                        stamp(all, fromString("all"));
        zipWith = parse("\\ f xs ys -> map (uncurry f) $ transpose [xs,ys]");stamp(zipWith, fromString("zipWith"));
        //map = apply2(S,apply2(S,apply(K,foldr),apply(B,cons)),apply(K,nil)); stamp(map, fromString("map"));
        
        // other combinate functions:
        // -------------------------

        fst = head;                                                  stamp(fst, fromString("fst"));
        snd = apply2(B,head,tail);                                   stamp(snd, fromString("snd"));
        last = apply2(B,head,reverse);                               stamp(last, fromString("last"));
        minus = apply(C,subtract);                                   stamp(minus, fromString("(-)"));
        even = apply2(B,apply(equal,intc(0)),apply2(C,rem,intc(2))); stamp(even, fromString("even"));
        odd = apply2(B,not,even);                                    stamp(odd, fromString("odd"));
        fromEnum = apply(times,intc(1));                             stamp(fromEnum, fromString("fromEnum"));
        zip = apply(zipWith, pair);                                  stamp(zip, fromString("zip"));
        negate = apply(minus,intc(0));                               stamp(negate, fromString("negate"));
        as = apply(foldr,cons);                                      stamp(as, fromString("as"));
        
        // inits

        srand(SEED);

        // HiSTORY SETUP
        
        strcpy(history_path, getenv("HOME"));
        strcat(history_path, "/");
        strcat(history_path, HISTORY_FILE);
        
        history_hndl = fopen(history_path, "r");
        if(!history_hndl){
                history_hndl = fopen(history_path, "w");
                for(int i=0; i<=HISTORY_ENTRIES; i++)
                        fputs("\n",history_hndl);
                fclose(history_hndl);
                history_hndl = fopen(history_path, "r");
        }
        for(int i=0; i<HISTORY_ENTRIES; i++){
                history[i] = 
                        (char*) malloc((1+HISTORY_CHARACTERS_PER_ENTRY) * sizeof(char));
                if(!fgets(history[i], HISTORY_CHARACTERS_PER_ENTRY, history_hndl))
                        printf("\nHistory restoring error.\n");
                for(int j=0; j<HISTORY_CHARACTERS_PER_ENTRY; j++) 
                        if(history[i][j] == '\n') history[i][j] = '\0';
                
                        
                if(!onlySpace(history[i])) history_write_index = i;
        }
        
        for(int i=0; i<=history_write_index/2; i++){
                char tmp[HISTORY_CHARACTERS_PER_ENTRY];
                strcpy(tmp, history[i]);
                strcpy(history[i],history[checkIndex(history_write_index-i)]);
                strcpy(history[checkIndex(history_write_index-i)] , tmp);
        }
        
        history_read_index = checkIndex(history_write_index+1);
       
}

FILE*out;
void fprintln(atom*a){
        fputs(show(a,1),out);
        fprintf(out,"\n");
}
void fprint(atom*a){
        fputs(show(a,1),out);
}

void tests(){
        initialize();
        out = fopen("tests/output", "w");
                
        fprintf(out,"\n ~~~~~~~~ APPLICATIONS ~~~~~~~~~~~~ \n");

        // basic numeric applications:        
        atom* num1 = apply2(plus,
                apply2(plus,intc(2),intc(3)),
                apply2(times,intc(2),intc(4)));
        fprintf(out,"\n(+(+ 2 3) (* 2 4)) => "); fprintln(num1);
        
        atom* plus4 = apply(plus,intc(4));
        atom* times3 = apply(times,intc(3));
        
        // composition:
        atom* comp1 = apply3(B,plus4,times3,intc(2));
        fprintf(out,"\n((+4).(*3)) 2      => "); fprintln(comp1);
        
        atom* comp2 = apply3(B,times3,plus4,intc(2));
        fprintf(out,"\n((*3).(+4)) 2      => "); fprintln(comp2);
        
        // commutation:
        atom* comm1 = apply2(subtract,intc(7),intc(10));
        fprintf(out,"\nsubtract 7 10      => "); fprint(comm1);
        atom* comm2 = apply2(minus,intc(7),intc(10));
        fprintf(out,"\n7 - 10             => "); fprintln(comm2);
        
        // tests and conditions:
        atom* cond1 = apply3(ifte, 
                apply(apply(equal,intc(0)),apply2(minus,intc(2),intc(4))), 
                intc(0), 
                intc(1));
        fprintf(out,"\n2-4==0 ? 0 : 1     => "); fprintln(cond1);
        
        atom* cond2 = apply3(ifte, 
                apply(apply(equal,intc(0)),apply2(minus,intc(4),intc(4))), 
                apply3(ifte,apply2(equal,intc(5),intc(6)),intc(0),intc(2)), 
                intc(1));
        fprintf(out,"\n4-4==0?(5==6?0:2):1=> "); fprintln(cond2);
        
        // recursion:
        atom* fact10 = apply(factorial,intc(10));
        fprintf(out,"\nfactorial 10       => "); fprintln(fact10);
        
        atom* fib10 = apply(fibonacci,intc(10));
        fprintf(out,"\nfibonacci 10       => "); fprintln(fib10);
        
        // functional combinators:
        char pair_str[] = "flip . (flip id)"; 
        atom* PAIR = parse(pair_str);
        atom* comb = apply3(PAIR,charc('a'),charc('b'),charc('c'));
        fprintf(out,"\nPAIR 'a' 'b' 'c'   => "); fprintln(comb);
        
        fprintf(out,"\n\\ a b c .  (ca)b   => "); 
        fprintln(parse("\\ a b c -> c a b"));
       
        fprintf(out,"\n ~~~~~~~~ LISTS ~~~~~~~~~~~~ \n");
       
        // elementary lists:
        atom* none = nil;
        atom* test1 = apply3(ifte,apply(isEmpty,none),charc('0'),charc('+'));
        fprintf(out,"\nempty []? '0':'+'  => "); fprintln(test1);
        atom* list1 = apply2(cons,intc(7),none);
        atom* test2 = apply3(ifte,apply(isEmpty,list1),charc('0'),charc('+'));
        fprintf(out,"\nempty [7]? '0':'+' => "); fprintln(test2);
        atom* test3 = apply(head, list1);
        fprintf(out,"\nhead [7]           => "); fprintln(test3);

        // streams
        atom* stream1 = apply(repeat,intc(2));
        fprintf(out,"\ns1 = repeat 2      => stream of 2's");
        atom* five2s = apply2(take,intc(5),stream1);
        fprintf(out,"\nl1 = take 5 s1     => ");fprintln(five2s);
        atom* fofol = apply3(foldr,times,intc(1),five2s);
        fprintf(out,"\nfoldr (*) 1 l1     => "); fprintln(fofol);
        
        // strings:
        atom* stream2 = apply(repeat,charc('H'));
        fprintf(out,"\ntake 8 (repeat 'H')=> ");
        atom* eightH = apply2(take,intc(8),stream2);
        fprintf(out,"\n");//fprintln(eightH);
        fprintf(out,"\nfoldr (:) \"\" it    => ");
        fprintln(apply3(foldr,cons,emptyString,eightH));

        // ususal lists functions:
        atom* list2 = apply2(cons,intc(66),apply2(cons,charc('x'),list1));
        atom* test4 = apply(head,apply(tail,list2));
        fprintf(out,"\nl2= 66:('x':(7:[]))=> "); fprintln(list2);
        fprintf(out,"\nhead $ tail l2     => "); fprintln(test4);
        atom* list3 = apply(reverse,list2);
        fprintf(out,"\nl3 = reverse l2    => "); fprintln(list3);
        atom* list4 = apply2(cons,
                apply2(pair,intc(4),intc(2)),
                apply2(cons,
                        apply2(pair,intc(2),intc(3)),
                        apply2(cons,
                                apply2(pair,intc(6),intc(8)),nil)));
        atom* list5 = apply2(map,fst,list4);
        fprintf(out,"\nl5                 => ");fprintln(list5);
        fprintf(out,"\ntakeWhile (< 5) l5 => ");
        fprintln(apply2(takeWhile,apply(apply(C,lessThan),intc(5)),list5));
        
        atom* list6 = apply2(cons,
                list5,
                apply2(cons,
                        list3,
                        apply2(cons,list2,nil)));
        atom* list7 = apply(concat,list6);
        fprintf(out,"\nconcat [l5,l3,l2]  => "); fprintln(list7);
        atom* list8 = apply2(map,
                apply3(predicate,apply(equiv,charc('z')),
                        apply(K,intc(0)),
                        apply(plus,intc(1))),
                list7);
        fprintf(out,"\nl8 = transform it  => "); fprintln(list8);
        atom* list9 = apply2(filter,odd,list8);
        fprintf(out,"\nl9 = filter odd l8 => "); fprintln(list9);
        atom* list10 = apply2(take,intc(4),list9);
        fprintf(out,"\nl10 = take 4 l9    => "); fprintln(list10);
    
        atom* list11 = apply3(scanr,plus,intc(0),list5);
        fprintf(out,"\nl11=scanr (+) 0 l5 => "); fprintln(list11);
        atom* list12 = apply2(map,apply(times,intc(2)),list5);
        fprintf(out,"\nl12 = map (*2) l5  => "); fprintln(list12);
        atom* list13 = apply3(scanl,plus,intc(0),list5);
        fprintf(out,"\nl13 = scanl + 0 l5 => "); fprintln(list13);
        fprintf(out,"\nlast l10           => "); fprintln(apply(last,list10));
        fprintf(out,"\ninit l10           => "); fprintln(apply(init,list10));
        
        atom* stream3 = apply(cycle,list10);
        fprintf(out,"\ns3 = cycle l10     => stream of concat'd l10's\n");
        atom* list14 = apply2(take,intc(7),stream3); 
        fprintf(out,"\ntake 7 s3          => "); fprintln(list14);
 
        char abs1[] = "\\x-> x(\\y-> y x y)";
        atom*test5 = parse(abs1);
        fprintf(out,"\n%s => ", abs1); fprintln(test5);
        
        fprintf(out,"\n ~~~~~~~~ LET CONSTRUCTS ~~~~~~~~~~~~ \n");

        char let0[] = "as \"\" $ let toUpper = map (subtract 32) in toUpper \"abcd\"";
        atom*test6 = parse(let0);
        fprintf(out,"\n%s            => ", let0); fprintln(test6);

        char fix1[] = "fix (\\f n -> if n<2 then 1 else n * (f $ n-1)) 12";
        atom*test7 = parse(fix1);
        fprintf(out,"\n%s                    => ", fix1); fprintln(test7);

        char let1[] = 
                "as [] $ let test = fix (\\ test n -> if n<0 then [] else n:(test $ n-1))\n"
                "       in test 13";
        atom* list15 = parse(let1);
        fprintf(out,"\n%s                                            => ", let1); 
        fprintln(list15);

        char fib2[] = 
                "let fib = fix (\\fib n -> if n<2 then n else (fib $ n-2) + (fib $ n-1)) \n"
                "          in fib 7";
        atom* list16 = parse(fib2);
        fprintf(out,"\n%s                                                   => ", fib2); fprintln(list16);

        char let2[] = 
                "let trans = (\\ mp pair -> let f = fst pair xs = snd pair \n"
                "                               in  (f ( head xs)) : (mp [f, tail xs]) ) \n"
                " in let mp = fix (\\mp pair ->  if null (snd pair) then [] else trans mp pair ) \n"
                "        in as [] $ mp [(+2), [3,4,5]]";
        atom* list17 = parse(let2);
        fprintf(out,"\n%s                                => ", let2); fprintln(list17);


       
        char let3[] = 
                "let mp = fix (\\mp pair -> let f = fst pair \n"
                "                               xs = snd pair \n"
                "                         in if null xs then \"\" else (f $ head xs) : (mp [f, tail xs])) \n"
                "    in as \"\" $ mp [subtract 32,\"abcd\"]";
        atom* list18 = parse(let3);
        fprintf(out,"\n%s                               => ", let3); fprintln(list18);

        char fix3[] = 
                "let mp = fix (\\mp pair -> let f = fst pair \n"
                "                               xs = snd pair \n"
                "                         in if null xs then \"\" else (f $ head xs) : (mp [f, tail xs])) \n"
                "   in let toLower = ((as \"\") . mp . ((+32) :) . (:[])) \n"
                "          in toLower \"WXYZ\"";
        atom*test8 = parse(fix3);
        fprintf(out,"\n%s                                          => ", fix3); fprintln(test8);

      char ack1[] = 
        "let ack = fix (\\f xy -> let x = fst xy    y = snd xy in \n"
        "                          if x==0 then y+1 \n"
        "                          else if y==0 then f[x-1,1] \n"
        "                               else f[x-1,f[x,y-1]]) \n"
        "                in ack [1,6] ";
        atom*test10 = parse(ack1);
        fprintf(out,"\n%s                                        => ", ack1); fprintln(test10);

        char fix4[] = 
                "as [] $ let oddEven = fix (\\ oe n -> if n == 0 then [false, true] \n"
                "               else  map not $ oe (n-1)) \n"
                "               in oddEven 21";
        atom*test9 = parse(fix4);
        fprintf(out,"\n%s                                         => ", fix4); fprintln(test9);
        
        char prog1[] = 
                "as [] $ let trans = (\\ set -> let m = set!!0  u = set!!1  v = set!!2 \n"
                "                                   n = set!!3 ku = set!!4 kv = set!!5 \n"
                "                       in let r = rem m n  d = quot m n \n"
                "                                in  [n,       ku ,      kv, \n"
                "                                     r, u-(d*ku), v-(d*kv)]) \n"
                "        in let algo = fix (\\algo set -> if (set!!3)==0 then set else algo (trans set)) \n"
                "            in let euclid = (\\ x y -> algo [x, 1, 0, y, 0, 1]) \n"
                "                 in euclid 53 15 ";
        atom*test11 = parse(prog1);
        fprintf(out,"\n%s                                   => ", prog1); fprintln(test11);
        
        char prog2[] = 
                "let liftM2 = (((.) ((.) ap)) (.))  \n"
                "    loeb = fix (\\ loeb x -> map (\\ f -> f $ loeb x) x) \n"
                "  in loeb [ (!!5), const 3, liftM2 (+) (!!0) (!!1), (*2) . (!!2), length, const 17]";
        atom*test12 = parse(prog2);
        fprintf(out,"\n%s          => ", prog2); fprintln(test12);
        
        fclose(out);
}

void repl(){
        initialize();
        struct winsize w;
        init_keyboard();
 
        int control_action = -1;
        int control_key = -1;
        int r = 0, lastr = 0, butlast = 0, butbutlast;
        char command[HISTORY_CHARACTERS_PER_ENTRY];
        int len = 0, pos = 0, i;
        char c;

        printf("\nPrelude> ");
        do{
                do{
                        butbutlast = butlast;
                        butlast = lastr;
                        lastr = r;
                        c = getchar();
                        r = c;
                        // printable
                        if(r != 27 
                                && r != 127
                                && r != '\n'
                                && lastr != 27 
                                && butlast != 27) control_key = 0;
                        // arrow
                        if(lastr == 91 && butlast == 27)
                                control_key = 1;
                        // delete
                        if(r==127) control_key = 2;
                        // enter
                        if(r=='\n') control_key = 3;
                        // end
                        if(r == 70 && lastr == 79 && butlast == 27)
                                control_key = 4;
                        // home
                        if(r == 72 && lastr == 79 && butlast == 27)
                                control_key = 5;
                         // suppr
                        if(r==126 && lastr == 51 && butlast == 91 && butbutlast == 27)
                                control_key = 6;
                                
                        switch(control_key){
                                // printable
                                case 0: for(i=len-1; i>=pos; i--)
                                        command[i+1]=command[i];
                                        command[pos] = r;
                                        for(i=pos; i<=len; i++)
                                                putchar(command[i]);
                                        for(i=pos; i<len; i++) 
                                                putchar('\b');
                                        len++;
                                        pos++;
                                        break;
                                // arrow
                                case 1: switch(r){
                                                // down
                                                case 66: history_read_index = 
                                                                checkIndex(history_read_index+1);
                                                        control_action = 1;
                                                        break;
                                                // up
                                                case 65: history_read_index = 
                                                                checkIndex(history_read_index-1);
                                                        control_action = 1;
                                                        break;
                                                // right
                                                case 67: if(pos<len){
                                                                putchar(command[pos]);
                                                                pos++; 
                                                        }
                                                        break;
                                                // left
                                                case 68: 
                                                        if(pos>0){
                                                                putchar('\b');
                                                                pos--;
                                                        }
                                                        break;
                                               }
                                              break;
                                // delete
                                case 2: 
                                        if(pos>0){
                                                for(i=pos;i<len;i++)
                                                        command[i-1] = command[i];
                                                command[len-1] = ' ';
                                                putchar('\b');
                                                pos--;
                                                for(i=pos;i<len;i++)
                                                        putchar(command[i]);
                                                len--;
                                                for(i=pos;i<=len;i++)
                                                        putchar('\b');
                                        }
                                        break;
                                // enter
                                case 3: putchar('\n');
                                        command[len] = '\0';
                                        control_action = 0;
                                        break;
                                // end
                                case 4:while(pos<len){
                                        putchar(command[pos]);
                                        pos++; 
                                        }
                                        break;
                                // home
                                case 5:while(pos>0){
                                        putchar('\b');
                                        pos--; 
                                        }
                                        break;
                                // suppr
                                case 6: 
                                        for(i=pos;i<len-1;i++)
                                                command[i] = command[i+1];
                                        command[len-1] = ' ';
                                        for(i=pos;i<len;i++)
                                                putchar(command[i]);
                                        len--;
                                        for(i=pos;i<=len;i++)
                                                putchar('\b');
                                        break;
                                        
                                default: 
                                        break;
                        }
                        control_key = -1;
                                
     
                } while(control_action<0);
                
                switch(control_action){
                        // new command
                        case 0: if(strcmp(command,":q") && !onlySpace(command)) {
                                println(parse(command));
                                history_write_index = checkIndex(history_write_index+1);
                                history_read_index = checkIndex(history_write_index+1);
                                strcpy(history[history_write_index], command);
                                for(i=0; i<len; i++) command[i] = ' ';
                                pos = 0;
                                len = 0;
                                }
                                ioctl(STDOUT_FILENO, TIOCGWINSZ, &w); // printf("%d ", w.ws_col);
                                printf("Prelude> ");
                                break;
                        // history command
                        case 1: for(i=0; i<pos; i++) putchar('\b');
                                for(i=0; i<len; i++) putchar(' ');
                                for(i=0; i<len; i++) putchar('\b');
                                strcpy(command,history[history_read_index]);
                                for(i=0; i<strlen(command); i++) 
                                        if(command[i]=='\n') command[i] = '\0';
                                len = strlen(command);
                                for(i=0; i<len; i++) 
                                        putchar(command[i]);
                                pos = len;
                                break;
                }
                control_action = -1;
        } while(strcmp(command,":q"));
        
        close_keyboard();
        
  printf("\n%d atoms and %d links still allocated.\n", natoms,nlinks);
               
        fclose(history_hndl);
        history_hndl = fopen(history_path, "w");
        for(int i=0; i<HISTORY_ENTRIES; i++){
                if(!fputs(history[checkIndex(history_write_index-i)], history_hndl))
                        printf("\nCan't save all history.\n");
                if(!fputs("\n", history_hndl))
                        printf("\nCan't save all history.\n");
        }
        fclose(history_hndl);
        for(int i=0; i<HISTORY_ENTRIES; i++) free(history[i]);

}

void benchmark(){
        initialize();
        
        printf("\n%d atoms and %d links still allocated.\n", natoms,nlinks);

        int max_int = 13850;
        atom* bunchOfOnes = apply2(take,intc(max_int),apply(repeat,intc(1)));
        atom* nats = apply3(scanl,plus,intc(0),bunchOfOnes);
      
   println(apply2(take,intc(17),nats));
          
        atom* cube = parse("\\ x -> x * x * x");
        atom* cubes = apply2(map,cube,apply2(take,intc(25),nats)); // 1291^3 > 2^31
        
        atom* isCube = 
                apply(parse(
                        "\\ cubes x -> elem x (==) $ takeWhile (< (x+1)) cubes"), 
                      cubes);

        atom* cond = parse("\\ x cube -> (x - cube) > cube");
        atom* getCubes = 
                apply3(parse(
                        "\\ isCube cond cubes x -> "
                                "filter (isCube . ((-) x)) $ takeWhile (cond x) cubes")
                ,isCube, cond, cubes);
    println(apply(getCubes,intc(1729)));
   
    
    
        atom* ok = apply2(B, parse("\\ ys -> not $ (null ys) || (null $ tail ys)")
                , getCubes);
        atom* benchmark = apply2(take, intc(3), apply2(filter, ok, nats));
        println(benchmark);
        
         printf("\n%d atoms and %d links still allocated.\n", natoms,nlinks);
 
}
