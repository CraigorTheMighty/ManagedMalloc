typedef struct avl_tree_node_s
{
	void					*data;
	int						height;
	int						num_children;
	struct avl_tree_node_s	*child[2];
	struct avl_tree_node_s	*parent;
}avl_tree_node_t;

void 			AVLTree_AdjustBalance(avl_tree_node_t **rootp);
void			**AVLTree_Query(avl_tree_node_t *root, void *value, void *context, int (*compare_fp)(void *arg0, void *arg1, void *context));
void 			AVLTree_Insert(avl_tree_node_t **rootp, void *value, void *context, int (*compare_fp)(void *arg0, void *arg1, void *context));
void			AVLTree_DeleteValue(avl_tree_node_t **rootp, void *value, void *context, int (*compare_fp)(void *arg0, void *arg1, void *context), void (*delete_fp)(void *value, void *context));
avl_tree_node_t *AVLTree_New();
void 			AVLTree_Destroy(avl_tree_node_t **root, void *context, void (*delete_fp)(void *value, void *context));
void			AVLTree_Walk(avl_tree_node_t *root, int dir, void *context, int (*callback_fp)(void *value, void *context, int depth));
void			AVLTree_WalkPre(avl_tree_node_t *root, int dir, void *context, int (*callback_fp)(void *value, void *context, int depth)); // returns quicker when callback returns nonzero
void			*AVLTree_Payload(avl_tree_node_t *root);
void			**AVLTree_PayloadP(avl_tree_node_t *root);