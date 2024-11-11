#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "trie.h"

TrieNode* readLexicon(const char* filePath) {
	TrieNode* root = emptyTrie();
	FILE* f = fopen(filePath, "r");
	if (f==NULL) {
		fprintf(stderr,"Error: Opening file failed\n");
		exit(EXIT_FAILURE);
	}
	char line[128];
	while (fgets(line, sizeof(line), f) != NULL) {
		line[strcspn(line, "\n")] = '\0';
		insertWord(root, line);
	}
	fclose(f);
	return root;
}

bool test1(TrieNode* root) {
    const char* apple="apple";
    const char* app="app";
    if (!insertWord(root, apple)) return false;
    if (!contains(root, apple)) return false;
    if (contains(root, app)) return false;
    return true;
}

bool test2(TrieNode* root) {
    return !contains(root, "a");
}

bool test3(TrieNode* root) {
    if (!insertWord(root, "app")) return false;
    if (!insertWord(root, "apple")) return false;
    if (!insertWord(root, "beer")) return false;
    if (!insertWord(root, "add")) return false;
    if (!insertWord(root, "jam")) return false;
    if (!insertWord(root, "rental")) return false;
    if (contains(root, "apps")) return false;
    if (!contains(root, "app")) return false;
    if (contains(root, "ad")) return false;
    if (contains(root, "applepie")) return false;
    if (contains(root, "rest")) return false;
    if (contains(root, "jan")) return false;
    if (contains(root, "rent")) return false;
    if (!contains(root, "beer")) return false;
    if (!contains(root, "jam")) return false;
    return true;
}

bool test4(TrieNode* root) {
    if (!insertWord(root, "app")) return false;
    if (!insertWord(root, "apple")) return false;
    if (!contains(root, "app")) return false;
    if (!contains(root, "apple")) return false;
    if (!removeWord(root, "app")) return false;
    if (contains(root, "app")) return false;
    if (!contains(root, "apple")) return false;
    if (!insertWord(root, "app")) return false;
    return true;
}

bool test5(TrieNode* root) {
    if (!contains(root, "aardwolves")) return false;
    if (!contains(root, "aardvarks")) return false;
    if (!contains(root, "abapical")) return false;
    if (!contains(root, "regionalization")) return false;
    if (!contains(root, "pantisocratists")) return false;
    return true;
}

int main(int argc, char **argv) {
    TrieNode* r1 = emptyTrie();
    printf("Test 1: %d\n", test1(r1));
    clearTrie(r1);
    r1 = emptyTrie();
    printf("Test 2: %d\n", test2(r1));
    printf("Test 3: %d\n", test3(r1));
    clearTrie(r1);
    r1=emptyTrie();
    printf("Test 4: %d\n", test4(r1));
    clearTrie(r1);
    if (argc==2) {
        TrieNode* r2 = readLexicon(argv[1]);
        printf("Test 5: %d\n", test5(r2));
        clearTrie(r2);
    }
    exit(EXIT_SUCCESS);
}