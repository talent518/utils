#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct _bin_tree_t {
	int dups;
	int data;
	struct _bin_tree_t *left;
	struct _bin_tree_t * right;
} bin_tree_t;

void addToBinTree(bin_tree_t *root, int data) {
	if (root->data > data) {
		if (root->left)
			addToBinTree(root->left, data);
		else {
			root->left = (bin_tree_t *) malloc(sizeof(bin_tree_t));
			root->left->dups = 0;
			root->left->data = data;
			root->left->left = NULL;
			root->left->right = NULL;
		}
	} else if (root->data == data) {
		root->dups++;
	} else {
		if (root->right)
			addToBinTree(root->right, data);
		else {
			root->right = (bin_tree_t *) malloc(sizeof(bin_tree_t));
			root->right->dups = 0;
			root->right->data = data;
			root->right->left = NULL;
			root->right->right = NULL;
		}
	}
}

void printBinTree(bin_tree_t *root) {
	if (root->left)
		printBinTree(root->left);

	int i;
	for (i = 0; i <= root->dups; i++)
		printf(" %d", root->data);

	if (root->right)
		printBinTree(root->right);
}

void freeBinTree(bin_tree_t *root) {
	if (root->left)
		freeBinTree(root->left);
	if (root->right)
		freeBinTree(root->right);
	free((void*) root);
}

int main(int argc, char *argv[]) {
	bin_tree_t root = { 0, 0, NULL, NULL };
	int i, n;

	if (argc < 3) {
		fprintf(stderr, "usage: %s 10 20 12 ...\n", argv[0]);
		return 1;
	}

	root.data = atoi(argv[1]);

	printf("before: %d", root.data);
	for (i = 2; i < argc; i++) {
		n = atoi(argv[i]);
		printf(" %d", n);
		addToBinTree(&root, n);
	}
	putchar('\n');

	printf("after:");
	printBinTree(&root);
	putchar('\n');

	if (root.left)
		freeBinTree(root.left);
	if (root.right)
		freeBinTree(root.right);

	return 0;
}
