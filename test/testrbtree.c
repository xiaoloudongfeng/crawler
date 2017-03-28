#include "core.h"

void rbtree_traverse(rbtree_node_t *root, rbtree_node_t *sentinel)
{
    if (root == sentinel) {
        return;
    }
    rbtree_traverse(root->left, sentinel);
    printf("%d ", root->key);
    rbtree_traverse(root->right, sentinel);
}

int main(void)
{
    rbtree_t        rbtree;
    rbtree_node_t   sentinel;

    rbtree_init(&rbtree, &sentinel, rbtree_insert_timer_value);

    rbtree_node_t node1 = { 100, NULL, NULL, NULL, 0, 0 };
    rbtree_insert(&rbtree, &node1);
    
    rbtree_node_t node2 = { 53, NULL, NULL, NULL, 0, 0 };
    rbtree_insert(&rbtree, &node2);

    rbtree_node_t node3 = { 66, NULL, NULL, NULL, 0, 0 };
    rbtree_insert(&rbtree, &node3);

    rbtree_node_t node4 = { 87, NULL, NULL, NULL, 0, 0 };
    rbtree_insert(&rbtree, &node4);

    rbtree_node_t node5 = { 91, NULL, NULL, NULL, 0, 0 };
    rbtree_insert(&rbtree, &node5);

    rbtree_node_t node6 = { 73, NULL, NULL, NULL, 0, 0 };
    rbtree_insert(&rbtree, &node6);

    rbtree_traverse(rbtree.root, &sentinel);
    printf("\n");

    rbtree_delete(&rbtree, &node1);
    rbtree_delete(&rbtree, &node2);
    
    rbtree_traverse(rbtree.root, &sentinel);
    printf("\n");
    
    return 0;
}
