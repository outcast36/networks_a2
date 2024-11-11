// Author: Wyatt Ayers
// Date: October 30, 2024
// Header file for trie data structure

#include <stdbool.h>

typedef struct TrieNode TrieNode;
struct TrieNode {
    struct TrieNode* children[26];
    bool endWord;
};

TrieNode* emptyTrie();
bool contains(TrieNode* root, const char* word);
void clearTrie(TrieNode* root);
bool insertWord(TrieNode* root, const char* word);
bool removeWord(TrieNode* root, const char* word);