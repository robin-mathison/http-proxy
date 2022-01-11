//-----------------------------------------------------------------------------
// Robin Mathison
// Dictionary.c
//-----------------------------------------------------------------------------
#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include "Dictionary.h"

#define KEY_TYPE char*
#define VAL_TYPE char*
#define VAL_TYPE2 int
#define KEY_UNDEF NULL
#define VAL_UNDEF NULL
#define VAL_UNDEF2 -1
#define KEY_FORMAT "%s" //"%d"
#define VAL_FORMAT "%s"
#define VAL_FORMAT2 "%d"
#define KEY_CMP(x,y) strcmp(x,y)
#define RED 0
#define BLACK 1


// Exported type --------------------------------------------------------------
typedef struct NodeObj{
   KEY_TYPE key;
   VAL_TYPE val;
   VAL_TYPE2 val2;
   int color;
   struct NodeObj* parent;
   struct NodeObj* left;
   struct NodeObj* right;
} NodeObj;

// private Node type
typedef NodeObj* Node;

// Exported type --------------------------------------------------------------
typedef struct DictionaryObj {
	int unique;
	int num_elements;
	Node index;
	Node root;
	Node temp;
} DictionaryObj;


// Constructors-Destructors ---------------------------------------------------
// newNode()
// Returns reference to new Node object. Initializes next and data fields.
Node new_Node(KEY_TYPE key, VAL_TYPE val, VAL_TYPE2 val2){
   Node N = malloc(sizeof(NodeObj));
   N->color = -1;
   if(key != KEY_UNDEF){
   	N->key = malloc(sizeof(char)*(strlen(key)+1));
   	memcpy(N->key, key, strlen(key));
   	N->key[strlen(key)] = '\0';
   } else {
   	N->key = key;
   }

   if(val != VAL_UNDEF){
   	N->val = malloc(sizeof(char)*(val2+1));
   	memcpy(N->val, val, val2);
   	N->val[val2] = '\0';
   } else {
   	N->val = val;
   }
   
   N->val2 = val2;
   N->parent = NULL;
   N->left = NULL;
   N->right = NULL;
   return(N);
}

// freeNode()
// Frees heap memory pointed to by *pN, sets *pN to NULL.
void free_Node(Node* pN){
   if( pN!=NULL && *pN!=NULL ){
   		if((*pN)->key != NULL){
   			free((*pN)->key);
   			(*pN)->key = NULL;
   		}

   		if((*pN)->val != NULL){
   			free((*pN)->val);
   			(*pN)->val = NULL;
   		}
      free(*pN);
      *pN = NULL;
   }
}
// newDictionary()
// Creates a new empty Dictionary. If unique==false (0), then the Dictionary 
// will accept duplicate keys, i.e. distinct pairs with identical keys. If 
// unique==true (1 or any non-zero value), then duplicate keys will not be 
// accepted. In this case, the operation insert(D, k) will enforce the 
// precondition: lookup(D, k)==VAL_UNDEF
Dictionary newDictionary(int unique){
	Dictionary D;
	D = malloc(sizeof(DictionaryObj));
	D->unique = unique;
	D->num_elements = 0;

	D->temp = new_Node(KEY_UNDEF, VAL_UNDEF, VAL_UNDEF2);
	D->root = D->temp;

	D->index = D->root;

	return(D);
}

// freeDictionary()
// Frees heap memory associated with *pD, sets *pD to NULL.
void freeDictionary(Dictionary* pD){
	if(pD != NULL && *pD != NULL){
		makeEmpty(*pD);
		free_Node(&(*pD)->temp);
		free(*pD);
		*pD = NULL;
	}
	return;
}


// Access functions -----------------------------------------------------------

// size()
// Returns the number of (key, value) pairs in Dictionary D.
int size(Dictionary D){
	return D->num_elements;
}

int color(Dictionary D){
	return D->index->color;
}

// getUnique()
// Returns true (1) if D requires that all pairs have unique keys. Returns
// false (0) if D accepts distinct pairs with identical keys.
int getUnique(Dictionary D){
	return D->unique;
}

// lookup()
// If Dictionary D contains a (key, value) pair whose key matches k (i.e. if
// KEY_CMP(key, k)==0), then returns value. If D contains no such pair, then
// returns VAL_UNDEF.
Node look_help(Node N, Dictionary D, KEY_TYPE k){
	if(N == D->temp){
		return N;
	}

	if(KEY_CMP(k, N->key) == 0){
		return N;
	} 

	if(KEY_CMP(k, N->key) < 0) {
		return look_help(N->left, D, k);

	} else{
		return look_help(N->right, D, k);
	}
}

VAL_TYPE lookup(Dictionary D, KEY_TYPE k, int* v2){
	if(D->num_elements == 0){
	  return VAL_UNDEF;
	}

	Node N = D->root;

	Node looker = look_help(N, D, k);

	if(looker != NULL){
		if(looker == D->temp){
		return VAL_UNDEF;

		} else{
			if(v2 != NULL){
				*v2 = looker->val2;
			}
			return looker->val;
		}
	} else {
		return VAL_UNDEF;
	}

}


// Manipulation procedures ----------------------------------------------------
void LeftRotate(Dictionary D, Node x){
	Node y = x->right;

	x->right = y->left;
	if(y->left != D->temp){
		y->left->parent = x;
	}

	y->parent = x->parent;
	if(x->parent == D->temp){
		D->root = y;
	} else if(x == x->parent->left){
		x->parent->left = y;
	} else{
		x->parent->right = y;
	}

	y->left = x;
	x->parent = y;
}

void RightRotate(Dictionary D, Node x){
	Node y = x->left;

	x->left = y->right;
	if(y->right != D->temp){
		y->right->parent = x;
	}

	y->parent = x->parent;
	if(x->parent == D->temp){
		D->root = y;
	} else if(x == x->parent->right){
		x->parent->right = y;
	} else{
		x->parent->left = y;
	}

	y->right = x;
	x->parent = y;
}

void InsertFixUp(Dictionary D, Node z){
	while(z->parent->color == RED){
		if(z->parent == z->parent->parent->left){
			Node y = z->parent->parent->right;
			if(y->color == RED){
				z->parent->color = BLACK;
				y->color = BLACK;
				z->parent->parent->color = RED;
				z = z->parent->parent;
			} else{
				if (z == z->parent->right){
					z = z->parent;
					LeftRotate(D, z);
				}
				z->parent->color = BLACK;
				z->parent->parent->color = RED;
				RightRotate(D, z->parent->parent);
			}
		} else{
			Node y = z->parent->parent->left;
			if(y->color == RED){
				z->parent->color = BLACK;
				y->color = BLACK;
				z->parent->parent->color = RED;
				z = z->parent->parent;
			} else{
				if(z == z->parent->left){
					z = z->parent;
					RightRotate(D, z);
				}

				z->parent->color = BLACK;
				z->parent->parent->color = RED;
				LeftRotate(D, z->parent->parent);
			}
		}
	}

	D->root->color = BLACK;
}

// insert()
// Insert the pair (k,v) into Dictionary D. 
// If getUnique(D) is false (0), then there are no preconditions.
// If getUnique(D) is true (1), then the precondition lookup(D, k)==VAL_UNDEF
// is enforced. 
void insert(Dictionary D, KEY_TYPE k, VAL_TYPE v, VAL_TYPE2 v2){
	if(getUnique(D) == 1 && lookup(D, k, NULL) == VAL_UNDEF){
		Node z = new_Node(k, v, v2);
		Node y = D->temp;
		Node x = D->root;
		while(x != D->temp){
			y = x;
			if(KEY_CMP(z->key, x->key) < 0){
				x = x->left;
			} else{
				x = x->right;
			}
		}

		z->parent = y;
		if(y == D->temp){
			D->root = z;
		} else if(KEY_CMP(z->key, y->key) < 0){
			if(D->index == y->left){
				D->index = z;
			}
			y->left = z;
		} else{
			if(D->index == y->right){
				D->index = z;
			}
			y->right = z;
		}

		z->left = D->temp;
		z->right = D->temp;
		z->color = RED;
		D->num_elements++;
		InsertFixUp(D, z);
	}
}

// delete()
// Remove the pair whose key is k from Dictionary D.
// Pre: lookup(D,k)!=VAL_UNDEF (i.e. D contains a pair whose key is k.)
Node TreeMin(Dictionary D, Node x){
	if(x != D->temp){
		while(x->left != D->temp){
			x = x->left;
		}

		return x;
	} else{
		printf("tree error\n");
		exit(EXIT_FAILURE);
	}
}

void switch_help(Dictionary D, Node u, Node v) {
	if(u->parent == D->temp){
		D->root = v;
	} else if(u == u->parent->left){
		u->parent->left = v;
	} else{
		u->parent->right = v;
	}

	v->parent = u->parent;
}

void DeleteFixUp(Dictionary D, Node x){
	Node w;
	while(x != D->root && x->color == BLACK){
		if(x == x->parent->left){
			w = x->parent->right;
			if(w->color == RED){
				w->color = BLACK;
				x->parent->color = RED;
				LeftRotate(D, x->parent);
				w = x->parent->right;
			}
			if(w->left->color == BLACK && w->right->color == BLACK){
				w->color = RED;
				x = x->parent;
			} else{
				if(w->right->color == BLACK){
					w->left->color = BLACK;
					w->color = RED;
					RightRotate(D, w);
					w = x->parent->right;
				}

				w->color = x->parent->color;
				x->parent->color = BLACK;
				w->right->color = BLACK;
				LeftRotate(D, x->parent);
				x = D->root;
			}
		} else{
			w = x->parent->left;
			if(w->color == RED){
				w->color = BLACK;
				x->parent->color = RED;
				RightRotate(D, x->parent);
				w = x->parent->left;
			}
			if(w->right->color == BLACK && w->left->color == BLACK){
				w->color = RED;
				x = x->parent;
			} else{
				if(w->left->color == BLACK){
					w->right->color = BLACK;
					w->color = RED;
					LeftRotate(D, w);
					w = x->parent->left;
				}
				w->color = x->parent->color;
				x->parent->color = BLACK;
				w->left->color = BLACK;
				RightRotate(D, x->parent);
				x = D->root;
			}
		}
	}

	x->color = BLACK;
}

void delete_item(Dictionary D, KEY_TYPE k){
	if(lookup(D, k, NULL) != VAL_UNDEF && size(D) > 0){
		Node z = look_help(D->root, D, k);
        Node y = z;
        Node x;
        int y_og_color = y->color;
        if(z->left == D->temp){
        	x = z->right;
        	switch_help(D, z, z->right);
        } else if(z->right == D->temp){
        	x = z->left;
        	switch_help(D, z, z->left);
        } else{
        	y = TreeMin(D, z->right);
        	y_og_color = y->color;
        	x = y->right;
        	if(y->parent == z){
        		x->parent = y;
        	} else{
        		switch_help(D, y, y->right);
        		y->right = z->right;
        		y->right->parent = y;
        	}

        	switch_help(D, z, y);
        	y->left = z->left;
        	y->left->parent = y;
        	y->color = z->color;
        }

        if(y_og_color == BLACK){
        	DeleteFixUp(D, x);
        }

        if(z == D->index){
        	D->index = D->temp;
        }

        free_Node(&z);

        D->num_elements--;
	} /*else {
		printf("dictionary error\n");
		//exit(EXIT_FAILURE);
	}*/
}

// makeEmpty()
// Reset Dictionary D to the empty state, containing no pairs.
void Empty_help(Dictionary D, Node N){
	if (N != D->temp){
		Empty_help(D, N->right);
		Empty_help(D, N->left);
		free_Node(&N);
	}
}

void makeEmpty(Dictionary D) {
	Node del = D->root;
	Empty_help(D, del);
	D->index = D->root = D->temp;
	D->num_elements = 0;
}

// beginForward()
// If D is non-empty, starts a forward iteration over D at the first key 
// (as defined by the order operator KEY_CMP()), then returns the first
// value. If D is empty, returns VAL_UNDEF. 

VAL_TYPE beginForward(Dictionary D){
	if(size(D) > 0){
		Node N = D->root;

		while (N->left != D->temp){
			N = N->left;
		}

		D->index = N;
		return N->val;

	} else {
		return VAL_UNDEF;
	}
}

// beginReverse()
// If D is non-empty, starts a reverse iteration over D at the last key 
// (as defined by the order operator KEY_CMP()), then returns the last
// value. If D is empty, returns VAL_UNDEF.
VAL_TYPE beginReverse(Dictionary D){
	if(size(D) > 0){
		Node N = D->root;

		while (N->right != D->temp){
			N = N->right;
		}

		D->index = N;
		return N->val;

	} else {
		return VAL_UNDEF;
	}
}

// currentKey()
// If an iteration (forward or reverse) over D has started, returns the 
// the current key. If no iteration is underway, returns KEY_UNDEF.
KEY_TYPE currentKey(Dictionary D){
	if (D->index != D->temp){
		return D->index->key;
	} else{
		return KEY_UNDEF;
	}
}

// currentVal()
// If an iteration (forward or reverse) over D has started, returns the 
// value corresponding to the current key. If no iteration is underway, 
// returns VAL_UNDEF.
VAL_TYPE currentVal(Dictionary D){
	if (D->index != D->temp){
		return D->index->val;
	} else{
		return VAL_UNDEF;
	}
}

// next()
// If an iteration (forward or reverse) over D has started, and has not
// reached the last pair, moves to the next key in D (as defined by the 
// order operator KEY_CMP()), and returns the value corresponding to the 
// new key. If an iteration has started, and has reached the last pair,
// ends the iteration and returns VAL_UNDEF. If no iteration is underway, 
// returns VAL_UNDEF.

VAL_TYPE next(Dictionary D){
	Node N = D->index->right;
	if (D->index->right != D->temp){
		while (N->left != D->temp){
			N = N->left;
		}

		D->index = N;
		return D->index->val;
	} else {
		N = D->index->parent;
		while (D->index == N->right && N != D->temp){
			D->index = N;
			N = N->parent;
			//D->index = N;
		}
		D->index = N;
		return D->index->val;
	}
}


// prev()
// If an iteration (forward or reverse) over D has started, and has not
// reached the first pair, moves to the previous key in D (as defined by the 
// order operator KEY_CMP()), and returns the value corresponding to the 
// new key. If an iteration has started, and has reached the first pair,
// ends the iteration and returns VAL_UNDEF. If no iteration is underway, 
// returns VAL_UNDEF. 
VAL_TYPE prev(Dictionary D){
	Node N = D->index->left;
	if (D->index->left != D->temp){
		while (N->right != D->temp){
			N = N->right;
		}

		D->index = N;
		return D->index->val;
	} else {
		N = D->index->parent;
		while (D->index == N->left && N != D->temp){
			D->index = N;
			N = N->parent;
		}
		D->index = N;
		return N->val;
	}
}


// Other operations -----------------------------------------------------------

// printDictionary()
// Prints a text representation of D to the file pointed to by out. Each key-
// value pair is printed on a single line, with the two items separated by a
// single space.  The pairs are printed in the order defined by the operator
// KEY_CMP().
void PreOrder(FILE* out, Dictionary D, Node N){
	if(N != D->temp){
		fprintf(out, "%s ", N->key);
		PreOrder(out, D, N->left);
		PreOrder(out, D, N->right);
	}
}

void InOrder(Dictionary D, Node N){
	if(N != D->temp){
		InOrder(D, N->left);
		printf("%s ", N->key);
		InOrder(D, N->right);
	}
}

void PostOrder(FILE* out, Dictionary D, Node N){
	if(N != D->temp){
		PostOrder(out, D, N->left);
		PostOrder(out, D, N->right);
		fprintf(out, "%s ", N->key);
	}
}

void printDictionary(Dictionary D, const char* ord){
	if (D != NULL) {
		Node N = D->root;
		if(strcmp(ord, "pre") == 0){
			exit(EXIT_FAILURE);
			//PreOrder(D, N);

		} else if(strcmp(ord, "in") == 0){
			InOrder(D, N);

		} else if (strcmp(ord, "post") == 0){
			exit(EXIT_FAILURE);
			//PostOrder(D, N);
		} 
	} else {
		printf("print error\n");
		exit(EXIT_FAILURE);
	}

}

