// Author: Wyatt Ayers
// Date: October 30, 2024
// Implementation of trie data structure

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "trie.h"

/* Any user of this trie data structure is responsible for deallocation */

int ST_SIZE=512;

// Set new node root's children to null and set endword to false
void initTrie(TrieNode* root) {
    root->endWord=false;
    for (int i=0;i<26;i++) {
        root->children[i]=NULL;
    }
}

// Construct empty root node or return existing root node
TrieNode* emptyTrie() {
    TrieNode* root = malloc(sizeof(TrieNode));
    initTrie(root);
    return root;
}

// Check if word has been inserted, or if prefix is true, check 
// if a word beginning with the prefix given by word is present
bool contains(TrieNode* root, const char* word) {
    int wordLen=strlen(word);
    TrieNode* cur = root;
    for (int i=0;i<wordLen;i++) {
        if (cur->children[word[i]-'a']==NULL) return false;
        else cur=cur->children[word[i]-'a'];
    }
    return cur->endWord;
}

// Free each node in the trie in a pre order DFS 
// i.e free root then free subtrees of root from left to right
void clearTrie(TrieNode* root) {
    if (root==NULL) return;
    int top=0;
    TrieNode* dfs_stack[ST_SIZE]; dfs_stack[0]=root;
    while (top>=0) {
        TrieNode* cur=dfs_stack[top--];
        for (int i=0;i<26;i++) {
            if (cur->children[i]!=NULL) {
                dfs_stack[++top]=cur->children[i];
            } 
        }
        free(cur);
    }   
    return; 
}

// Insert word into the trie if not already present
// Return true iff new nodes allocated, i.e. word not found
bool insertWord(TrieNode* root, const char* word) {
    if (contains(root, word)) return false;
    TrieNode* cur=root;
    TrieNode* child=NULL;
    int wordLen=strlen(word);
    for (int i=0;i<wordLen;i++) {
        if (cur->children[word[i]-'a']==NULL) {
            child=malloc(sizeof(TrieNode));
            initTrie(child);
            cur->children[word[i]-'a']=child;
        }
        cur=cur->children[word[i]-'a'];
    }
    cur->endWord=true;
    return true;
}

// Return true iff tree state changes, i.e. word found and removed
bool removeWord(TrieNode* root, const char* word) {
    if (!contains(root, word)) return false;
    TrieNode* cur=root;
    int wordLen=strlen(word);
    for (int i=0;i<wordLen;i++) cur=cur->children[word[i]-'a'];
    cur->endWord=false;
    return true;
}