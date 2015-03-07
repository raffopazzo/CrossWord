#include <algorithm>
#include <exception>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

class TrieTreeNode {
  char c;
  unordered_map<char, shared_ptr<TrieTreeNode>> children;

public:
  TrieTreeNode(char c = 0) : c(c) { }
  TrieTreeNode(const TrieTreeNode&) = delete;

  bool empty() const { return children.empty(); }

  void add(const string &s, int i = 0) {
    if (i < s.length()) {
      auto it = children.emplace(s[i], make_shared<TrieTreeNode>(s[i]));
      shared_ptr<TrieTreeNode> child = it.first->second;
      child->add(s, i+1);
    }
  }

  bool containsWordStartingWith(const string &chars, int i) const {
    if (i < chars.size() && chars[i] != '\0') {
      auto it = children.find(chars[i]);
      if (it != children.end()) {
        return it->second->containsWordStartingWith(chars, i+1);
      } else {
        return false;
      }
    } else {
      return true;
    }
  }
};

class Bucket {
  vector<string>        words;
  mutable TrieTreeNode  trie;

public:

  Bucket()              = default;
  Bucket(const Bucket&) = delete;

  const vector<string>& getWords() const { return words; }

  void add(const string &s) { words.push_back(s); }

  int size() const { return words.size(); }

  bool containsWordStartingWith(const string &chars) const {
    if (trie.empty()) indexWords();
    return trie.containsWordStartingWith(chars, 0);
  }

  void indexWords() const {
    for (auto &s: words) trie.add(s);
  }
};


class CrossWord {
  friend ostream& operator<<(ostream& os, const CrossWord& crossword);
  int n_rows, n_cols;
  int lastCol;

  vector<string> v_rows;

public:
  CrossWord(int rows, int cols) :
      n_rows(rows),
      n_cols(cols),
      lastCol(0),
      v_rows(rows, string(cols, '\0'))
  { }

  CrossWord(const CrossWord&) = delete;

  int rows() { return n_rows; }
  int cols() { return n_cols; }

  void pushVertical(const string &s) {
    if (lastCol >= n_cols)  throw std::runtime_error{"No more cols"};
    if (s.length() != n_rows) throw std::runtime_error{"String too long"};
    int i = 0;
    for (auto &r : v_rows) {
      r[lastCol] = s[i++];
    }
    ++lastCol;
  }

  void popVertical() {
    if (lastCol == 0) throw std::runtime_error{"No cols to pop"};
    --lastCol;
    for (auto &r : v_rows) {
      r[lastCol] = 0;
    }
  }

  bool isFull() {
    return lastCol == n_cols;
  }

  bool isPartialOk(const vector<Bucket> &buckets) { 
    auto &horizontals = buckets[n_cols];
    for (auto &r: v_rows) {
      if (! horizontals.containsWordStartingWith(r)) {
        return false;
      }
    }
    return true;
  }
};

class LargestCrosswordProblem {
  // Ideally buckets would be declared as vector<Bucket>. However, since the
  // number of required buckets isn't known in advance, the vector would require
  // calls like resize() or push_back() or emplace() to add buckets, but Bucket
  // is non-copyable. So unique_ptr allows to defer the allocation until the
  // number of buckets has been discovered.
  unique_ptr<vector<Bucket>> buckets;

public:
  LargestCrosswordProblem(const string &filename) {
    organiseInBuckets(filename);
  }

  LargestCrosswordProblem(const LargestCrosswordProblem&) = delete;

  const vector<Bucket>& getBuckets() { return *buckets; }

  unique_ptr<CrossWord> findCrossword(int rows, int cols) {
    const Bucket& horizontals = (*buckets)[cols];
    const Bucket& verticals   = (*buckets)[rows];
    if (horizontals.size() > verticals.size()) {
      return tryBuildCrossword(rows, cols);
    } else {
      return tryBuildCrossword(cols, rows);
    }
  }

  unique_ptr<CrossWord> tryBuildCrossword(int rows, int cols) {
    const Bucket &horizontals = (*buckets)[cols];
    const Bucket &verticals   = (*buckets)[rows];
    cout << rows << "x" << cols << " / "
         << verticals.size() << "x" << horizontals.size() << endl;
    if (rows == cols && horizontals.size() < 2*rows) return nullptr;
    unique_ptr<CrossWord> crossword{new CrossWord(rows, cols)};

    for (auto &s : verticals.getWords()) {
      crossword->pushVertical(s);
      if (tryFill(crossword.get())) {
        return crossword;
      }
      crossword->popVertical();
    }
    return nullptr;
  }

private:
  void organiseInBuckets(const string &filename) {
    string word;
    auto maxSize = numeric_limits<decltype(word.length())>::min();
    fstream file{filename};
    vector<string> dictionary;
    while (file >> word) {
      maxSize = max(maxSize, word.length());
      dictionary.push_back(word);
    }
    buckets = unique_ptr<vector<Bucket>>{new vector<Bucket>{1+maxSize}};
    for (auto &s: dictionary) {
      (*buckets)[s.length()].add(s);
    }
  }

  bool tryFill(CrossWord *crossword) {
    const Bucket &horizontals = (*buckets)[crossword->cols()];
    const Bucket &verticals   = (*buckets)[crossword->rows()];
    if (! crossword->isPartialOk(getBuckets())) return false;
    if (crossword->isFull()) return true;
    for (auto &s: verticals.getWords()) {
      crossword->pushVertical(s);
      if (tryFill(crossword)) {
        return true;
      }
      crossword->popVertical();
    }
    return false;
  }

};

ostream& operator<<(ostream& os, const CrossWord& crossword) {
  for (auto &r: crossword.v_rows) {
    for (auto c: r) {
      os << (c != 0 ? c : '.');
    }
    os << endl;
  }
  return os;
}

int main(int argc, char* argv[]) {
  LargestCrosswordProblem p("parole.txt");
  int i=0;
  for (auto &b: p.getBuckets()) {
    cout << i++ << " " << b.size() << endl;
  }
  if (argc == 1) {
//  cout << p.findLargestCrossword() << endl;
  } else if (argc == 3) {
    auto crossword = p.findCrossword(stoi(argv[1]), stoi(argv[2]));
    if (crossword != nullptr) {
      cout << *crossword << endl;
    }
  } else {
    cerr << "Invalid arguments" << endl;
    return -1;
  }
  return 0;
}


