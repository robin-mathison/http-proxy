/*
Robin Mathison
rmathiso
pa7
List.c
** Pulled some code from P. Tantalo's course website
*/

#include<stdio.h>
#include<stdlib.h>
#include <stdint.h>
#include "List.h"

// structs --------------------------------------------------------------------

// private NodeObj type
typedef struct NodeObj{
   LIST_ELEMENT data;
   struct NodeObj* next;
   struct NodeObj* prev;
} NodeObj;

// private Node type
typedef NodeObj* Node;

// private ListObj type
typedef struct ListObj{
   Node front;
   Node back;
   Node cursor;
   int length;
   int c_index; 
} ListObj;


// Constructors-Destructors ---------------------------------------------------

// newNode()
// Returns reference to new Node object. Initializes next and data fields.
Node newNode(LIST_ELEMENT data){
   Node N = malloc(sizeof(NodeObj));
   N->data = data;
   N->next = NULL;
   N->prev = NULL;
   return(N);
}

// freeNode()
// Frees heap memory pointed to by *pN, sets *pN to NULL.
void freeNode(Node* pN){
   if( pN!=NULL && *pN!=NULL ){
      free(*pN);
      *pN = NULL;
   }
}

List newList(void){
   List L;
   L = malloc(sizeof(ListObj));
   L->front = L->back = L->cursor = NULL;
   L->length = 0;
   L->c_index = -1;
   return(L);
} // Creates and returns a new empty List.

void freeList(List* pL){
   if(pL!=NULL && *pL!=NULL) { 
      Node n = (*pL)->front;
      while( n != NULL ) { 
         Node f = n;
         n = n->next;
         free(f);
         f = NULL;
      }
      free(*pL);
      *pL = NULL;
   }
} // Frees all heap memory associated with *pL, and sets
 // *pL to NULL.

// Access functions -----------------------------------------------------------
int length(List L){
   if( L==NULL ){
      return 0;
   }
   return L->length;
} // Returns the number of elements in L.

int lindex(List L){
   if(L!=NULL){
      return L->c_index;
   } else {
      return -1;
   }
} // Returns index of cursor element if defined, -1 otherwise.

LIST_ELEMENT front(List L){
   if(length(L) > 0){
      return L->front->data;
   } else{
      exit(EXIT_FAILURE);
   }
} // Returns front element of L. Pre: length()>0

LIST_ELEMENT back(List L){
   if(L == NULL){
      exit(EXIT_FAILURE);
   }
   if(length(L) > 0){
      return L->back->data;
   } else{
      exit(EXIT_FAILURE);
   }
} // Returns back element of L. Pre: length()>0

LIST_ELEMENT get(List L){
   if(L == NULL){
      exit(EXIT_FAILURE);
   }
   if(length(L) > 0 && lindex(L) >= 0){
      return L->cursor->data;
   } else{
      exit(EXIT_FAILURE);
   }
} // Returns cursor element of L. Pre: length()>0, index()>=0

int ListEquals(List A, List B){
   if( A==NULL || B==NULL ){
      printf("Error\n");
      exit(EXIT_FAILURE);
   }

   if(length(A) != length(B)){
      return 0;
   }

   List X = A;
   List Y = B;
   moveFront(X);
   moveFront(Y);
   for(int i = 0; i < length(X); i++){
      if(get(X) != get(Y)){
         return 0;
      }
      moveNext(X);
      moveNext(Y);
   }

   return 1;
} // Returns true (1) iff Lists A and B are in same
 // state, and returns false (0) otherwise.*/

// Manipulation procedures ----------------------------------------------------
void clear(List L){
   if(L != NULL){
      Node front = L->front;
      while( front != NULL ) { 
         Node f = front;
         front = front->next;
         freeNode(&f);
      }

      L->front = NULL;
      L->back = NULL;
      L->cursor = NULL;
      L->length = 0;
      L->c_index = -1;
   } else{
      exit(EXIT_FAILURE);
   }

} // Resets L to its original empty state.

void set(List L, LIST_ELEMENT x){
   if(length(L) > 0 && lindex(L) >= 0){
      L->cursor->data = x;
      return;
   } else{
      exit(EXIT_FAILURE);
   }
} // Overwrites the cursor element’s data with x.
 // Pre: length()>0, index()>=0

void moveFront(List L){
   if(length(L) > 0){
      L->c_index = 0;
      L->cursor = L->front;
   }
   return;
} // If L is non-empty, sets cursor under the front element,
 // otherwise does nothing.

void moveBack(List L){
   if(length(L) > 0){
      L->c_index = length(L) - 1;
      L->cursor = L->back;
   }
   return;
} // If L is non-empty, sets cursor under the back element,
 // otherwise does nothing.

void movePrev(List L){
   if(L->cursor != NULL){
      if(L->c_index == 0){
         L->c_index = -1;
         L->cursor = NULL;
      } else{
         L->c_index--;
         L->cursor = L->cursor->prev;
      }
   } else{
      exit(EXIT_FAILURE);
   }
   return;
} // If cursor is defined and not at front, move cursor one
 // step toward the front of L; if cursor is defined and at
 // front, cursor becomes undefined; if cursor is undefined
 // do nothing

void moveNext(List L){
   if(L->cursor != NULL){
      if(L->c_index == length(L) - 1){
         L->c_index = -1;
         L->cursor = NULL;
      } else{
         L->c_index++;
         L->cursor = L->cursor->next;
      }
   } else{
      exit(EXIT_FAILURE);
   }
   return;
} // If cursor is defined and not at back, move cursor one
 // step toward the back of L; if cursor is defined and at
 // back, cursor becomes undefined; if cursor is undefined
 // do nothing

void prepend(List L, LIST_ELEMENT x){
   if(L == NULL){
      exit(EXIT_FAILURE);
   }
   Node new = newNode(x);
   if(length(L) == 0){
      L->back = new;
      L->front = new;
      L->cursor = L->front;
   } else{
      Node temp = L->front;
      L->front->prev = new;
      L->front = new;
      L->front->next = temp;
      L->c_index++;
   }
   L->length++;
   return;
} // Insert new element into L. If L is non-empty,
 // insertion takes place before front element.

void append(List L, LIST_ELEMENT x){
   if(L == NULL){
      exit(EXIT_FAILURE);
   }
   Node new = newNode(x);
   if(length(L) == 0){
      L->front = new;
      L->back = new;
      L->cursor = L->back;
   } else{
      Node temp = L->back;
      L->back->next = new;
      L->back = new;
      L->back->prev = temp;
   }
   L->length++;
   return;
} // Insert new element into L. If L is non-empty,
 // insertion takes place after back element.

void insertBefore(List L, LIST_ELEMENT x){
   if(L == NULL){
      exit(EXIT_FAILURE);
   }
   if(length(L) > 0 && lindex(L) >= 0){
      Node new = newNode(x);
      if(L->cursor->prev == NULL){
         L->front = new;
         L->front->next = L->cursor;
         L->cursor->prev = new;
      } else{
         Node temp1 = L->cursor->prev;
         L->cursor->prev->next = new;
         L->cursor->prev = new;
         L->cursor->prev->prev = temp1;
         L->cursor->prev->next = L->cursor;
      }
      L->c_index++;
      L->length++;
      return;
   } else{
      exit(EXIT_FAILURE);
   }
}// Insert new element before cursor.
 // Pre: length()>0, index()>=0
void insertAfter(List L, LIST_ELEMENT x){
   if(L == NULL){
      exit(EXIT_FAILURE);
   }
   if(length(L) > 0 && lindex(L) >= 0){
      Node new = newNode(x);
      if(L->cursor->next == NULL){
         L->cursor->next = new;
         L->back = new;
         L->back->prev = L->cursor;
      } else{
         Node temp = L->cursor->next;
         L->cursor->next->prev = new;
         L->cursor->next = new;
         L->cursor->next->prev = L->cursor;
         L->cursor->next->next = temp;
      }
      L->length++;
      return;
   } else{
      exit(EXIT_FAILURE);
   }
} // Insert new element after cursor.
 // Pre: length()>0, index()>=0

void deleteFront(List L){
   if(L == NULL){
      exit(EXIT_FAILURE);
   }
   if(length(L) > 0){
      if(L->front == L->cursor){
         L->c_index = -1;
         L->cursor = NULL;
      } else{
         if(L->c_index != -1){
            L->c_index--;
         }
      }
      if(length(L) == 1){
         Node new = L->front;
         freeNode(&new);
         L->c_index = -1;
         L->cursor = NULL;
         L->front = L->back = NULL;
      } else {
         Node new = L->front;
         L->front = L->front->next;
         L->front->prev = NULL;
         freeNode(&new);
      }
      
      L->length--;

   } else {
      exit(EXIT_FAILURE);
   }
   return;
} // Delete the front element. Pre: length()>0

void deleteBack(List L){
   if(L == NULL){
      exit(EXIT_FAILURE);
   }
   if(length(L) > 0){
      if(length(L) == 1){
         Node back = L->back;
         freeNode(&back);
         L->c_index = -1;
         L->cursor = NULL;
         L->front = L->back = NULL;
      } else{

         Node new = L->back;
         Node temp = L->back->prev;

         if(L->cursor == L->back){
            L->c_index = -1;
            L->cursor = NULL;
         }

         L->back = L->back->prev;
         L->back->next = NULL;
         L->back->prev = temp->prev;

         freeNode(&new);

      }

      L->length--;
   } else {
      exit(EXIT_FAILURE);
   }
   return;
} // Delete the back element. Pre: length()>0

void delete_list(List L){
   if(L == NULL){
      exit(EXIT_FAILURE);
   }
   if(length(L) > 0 && lindex(L) >= 0){
      if(L->front == L->cursor){
         deleteFront(L);
         return;
      }
      if(L->back == L->cursor){
         deleteBack(L);
         return;
      }

      L->cursor->prev->next = L->cursor->next;
      L->cursor->next->prev = L->cursor->prev;
      free(L->cursor);
      L->c_index = -1;
      L->cursor = NULL;
      L->length--;
   } else {
      exit(EXIT_FAILURE);
   }
   return;
} // Delete cursor element, making cursor undefined.
 // Pre: length()>0, index()>=0

// Other operations -----------------------------------------------------------
void printList(List L){
   Node printer = L->front;
   while(printer != NULL){
      printf("%d ", printer->data);
      printer = printer->next;
   }

   return;
} // Prints to the file pointed to by out, a
 // string representation of L consisting
 // of a space separated sequence of integers,
// with front on left.
List copyList(List L){
   List new = newList();
   Node n = L->front;
   for(int i = 0; i < length(L); i++){
      append(new, n->data);
      n = n->next;
   }
   new->cursor = NULL;
   return new;
} // Returns a new List representing the same integer
 // sequence as L. The cursor in the new list is undefined,
// regardless of the state of the cursor in L. The state
// of L is unchanged.

