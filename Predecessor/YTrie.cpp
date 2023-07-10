#include "YTrie.h"
#include <cmath>
#include <bitset>

/*
* Calculates the depth needed for the trie.
* Since the numbers are max 64 bits, we could just assume depth = 64.
* But we can speed up the process when only smaller numbers are present.
* Eg. with only 32 bit numbers or smaller (even in 64 bit format), we can half the depth of the Trie.
* The depth is given as the number of bits used to represent the largest number in the input values.
*/
int64_t calcDepth(std::vector <uint64_t> values) {
	return (int64_t)std::floor(std::log2(values.back()));
}

/**
* 
*/
BST* constructBST(uint64_t position, uint64_t groupSize, std::vector <uint64_t> values) {
	// Isolate the group with the given size for the representative on the given position 
	std::vector<uint64_t> group(values.begin() + position - (groupSize - 1), values.begin() + position + 1);
	BST* tree = new BST(group);
	return tree;
}

/*
* Adds a leaf to the trie representatives with a maximum sized binary search tree.
* 
* @param values A full vector of all values for which the trie is built.
* @param index The index of the representative in the given values.
* @param representatives A vector of all representatives so far in TrieNode format.
* @param depth The trie depth.
*/
void addRegularTrieLeaf(std::vector<uint64_t> values, uint64_t index, std::vector<TrieNode*>* representatives, uint64_t depth) {
	int64_t maxIndex = values.size() - 1;
	if (index == depth - 1) {
		// First to add has nullptr as previous
		representatives->push_back(new TrieNode(values[index], nullptr, constructBST(index, depth, values)));
	}
	else {
		// All other representatives have the predecessor as previous. Also sets the next pointers for the last added leaf.
		representatives->push_back(new TrieNode(values[index], representatives->back(), constructBST(index, depth, values)));
		(*representatives)[representatives->size() - 2]->setNext((*representatives)[representatives->size() - 1]);
	}
}

/*
* Adds a leaf to the trie representatives WITHOUT a maximum sized binary search tree. This can only happen, when the split is imperfect.
* Does not need an index, since it knows it has to be the last representant.
* 
* @param values A full vector of all values for which the trie is built.
* @param representatives A vector of all representatives so far in TrieNode format.
* @param depth The trie depth.
*/
void addIrregularTrieLeaf(std::vector<uint64_t> values, std::vector<TrieNode*>* representatives, uint64_t depth) {
	representatives->push_back(new TrieNode(values.back(), representatives->back(), constructBST(values.size() - 1, values.size() % depth, values)));
	(*representatives)[representatives->size() - 2]->setNext((*representatives)[representatives->size() - 1]);
}


void YTrie::split(std::vector <uint64_t> values) {
	for (int64_t i = depth_ - 1; i < values.size(); i = i + depth_) {
		// Store the representative and construct the BST for it.
		addRegularTrieLeaf(values, i, &representatives_, depth_);
	}
	if (values.size() % depth_ != 0) {
		// One or more(<depth) values at the back don't have a representant yet. Take the last value as one and construct the BST.
		addIrregularTrieLeaf(values, &representatives_, depth_);
	}
}

// For the whole trie: 0 = left, 1 = right
void YTrie::constructTrie(std::vector<TrieNode*>* representatives, std::vector<uint64_t>* representativeValues,
	int64_t exponent, std::string bitHistory, int64_t leftRange, int64_t rightRange) {
	int64_t split = 1LL << exponent; // 2^exponent is the border to split
	if (exponent != -1) { // Construct inner node
		int64_t splitIndex = rightRange + 1;
		TrieNode* leftMax = nullptr;
		TrieNode* rightMin = nullptr;
		bool foundSplit = false; 
		// Attempt to find a split point
		for (int64_t i = leftRange; i <= rightRange; i++) {
			if ((*representativeValues)[i] >= split) { // Found split point
				if (!foundSplit) { // Found split point for the first time
					splitIndex = i;
					foundSplit = true;
					if (i != leftRange) {
						leftMax = (*representatives)[i - 1];
					}
					rightMin = (*representatives)[i];
				}
				(*representativeValues)[i] = (*representativeValues)[i] - split; // Because of the way the numbers are iterated, these can never create a negative value
			}
		}
		if (leftMax == nullptr && rightMin == nullptr) { // No split was found, all representant belong to the left side of this inner node
			leftMax = (*representatives)[rightRange];
		}
		TrieNode* node = new TrieNode(leftMax, rightMin);
		lookup_.insert({ bitHistory, node });
		if (splitIndex > leftRange) { // Construct left subtree
			std::string leftMaskHistory = bitHistory;
			constructTrie(representatives, representativeValues, exponent - 1, leftMaskHistory.append("0"), leftRange, splitIndex - 1);
		}
		if (splitIndex <= rightRange) { // Construct right subtree
			std::string rightMaskHistory = bitHistory;
			constructTrie(representatives, representativeValues, exponent - 1, rightMaskHistory.append("1"), splitIndex, rightRange);
		}
	}
	else { // Add the leafs
		if ((bitHistory.compare(bitHistory.size(), 1, "0")) == 0) { // Choose the correct leftRange and rightRange (according to last bit) to not go out of boundary
			lookup_.insert({ bitHistory, (*representatives)[leftRange] });
		}
		else {
			lookup_.insert({ bitHistory, (*representatives)[rightRange] });
		}
	}
}


YTrie::YTrie(std::vector<uint64_t> values) :
	depth_(calcDepth(values)) {
	split(values);
	std::vector<uint64_t> representativeValues;
	for (int i = 0; i < representatives_.size(); i++) {
		representativeValues.push_back(representatives_[i]->getValue());
	}
	constructTrie(&representatives_, &representativeValues, depth_, "", 0, (representatives_.size() - 1));
}


int64_t YTrie::getPredecessor(uint64_t limit) {
	int64_t lowRange = 0;
	int64_t highRange = depth_ + 1;
	std::string fullBitString = std::bitset<64>(limit).to_string().substr(64-(depth_+1)); // Input bit-string with same length as representants
	TrieNode* bestMatchingNode = lookup_[""];
	while (lowRange <= highRange) {
		int64_t middle = round((lowRange + highRange) / 2);
		std::string partBitString = fullBitString.substr(0, middle);
		if (lookup_.count(partBitString) != 0) { // Matched part bit string. Remember node and search lower in trie 
			bestMatchingNode = lookup_[partBitString];
			if (lowRange == middle) {
				lowRange++; // When stuck, move the lower range one higher
			}
			else {
				lowRange = middle;
			}
		}
		else { // Search higher in trie
			if (highRange == middle) {
				highRange--; // When stuck, move the higher range one lower
			}
			else {
				highRange = middle;
			}
		}
	}
	if (bestMatchingNode->isLeaf()) {
		return bestMatchingNode->getValue();
	}
	else {
		// Our binary search should have gotten to the best possible node for us. This means the bestMatchingNode only has one right, or one left child.
		if (bestMatchingNode->getLeftMax() != nullptr) {
			if (bestMatchingNode->getLeftMax()->next() != nullptr) {
				return bestMatchingNode->getLeftMax()->next()->getBinarySearchTree()->getPredecessor(bestMatchingNode->getLeftMax()->getValue(), limit);
			}
			else {
				return bestMatchingNode->getLeftMax()->getValue();
			}
		}
		else if (bestMatchingNode->getRightMin() != nullptr) {
			if (bestMatchingNode->getRightMin()->previous() != nullptr) {
				return bestMatchingNode->getRightMin()->getBinarySearchTree()->getPredecessor(bestMatchingNode->getRightMin()->previous()->getValue(), limit);
			}
			else {
				return bestMatchingNode->getRightMin()->getBinarySearchTree()->getPredecessor(0, limit); // 0 is ok if we checked for input bound before
			}
		}
		else {
			// Something went wrong
			return LLONG_MAX;
		}
	}
}