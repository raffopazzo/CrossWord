#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <exception>
#include <fstream>
#include <future>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

class DenseTrie {

  struct Node {
    char c;
    shared_ptr<DenseTrie> child;

    Node(char ch) : c(ch), child(make_shared<DenseTrie>()) {}
  };

  vector<Node> nodes;

public:
  DenseTrie()                 = default;
  DenseTrie(const DenseTrie&) = delete;

  bool empty() const { return nodes.empty(); }

  void add(const string &s, int i = 0) {
    if (i < s.length()) {
      auto child = insertOrGet(s[i]);
      child->add(s, i + 1);
    }
  }

  bool containsWordStartingWith(const string &chars, int i, int maxLen) const {
    if (i < maxLen) {
      auto child = findChild(chars[i]);
      if (child != nullptr) {
        return child->containsWordStartingWith(chars, i+1, maxLen);
      } else {
        return false;
      }
    } else {
      return true;
    }
  }

private:
  DenseTrie* findChild(char c) const {
    for (auto &n: nodes) {
      if (n.c == c) {
        return n.child.get();
      }
    }
    return nullptr;
  }

  DenseTrie* insertOrGet(char c) {
    auto n = findChild(c);
    if (n != nullptr) {
      return n;
    } else {
      nodes.emplace_back(c);
      return nodes.back().child.get();
    }
  }
};

class Bucket {
  vector<string>     words;
  mutable DenseTrie  trie;

public:

  Bucket()              = default;
  Bucket(const Bucket&) = delete;

  const vector<string>& getWords() const { return words; }

  void add(const string &s) { words.push_back(s); }

  int size() const { return words.size(); }

  bool containsWordStartingWith(const string &chars, int maxLen) const {
    return trie.containsWordStartingWith(chars, 0, maxLen);
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
  }

  bool isFull() {
    return lastCol == n_cols;
  }

  bool isPartialOk(const vector<Bucket> &buckets) { 
    auto &horizontals = buckets[n_cols];
    for (auto &r: v_rows) {
      if (! horizontals.containsWordStartingWith(r, lastCol)) {
        return false;
      }
    }
    return true;
  }
};

static atomic_int   index;
static atomic_bool  aborted {false};
static int          threads = 2;
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

  shared_ptr<CrossWord> findLargestCrossword() {
    auto maxSize = getBuckets().size() - 1;
    for (int area = maxSize * maxSize; area >= 1; --area) {
      for (int rows = maxSize; rows >= sqrt(area); --rows ) {
        if (area % rows == 0) {
          int cols = area / rows;
          if (cols <= maxSize) {
            shared_ptr<CrossWord> crossword = findCrossword(rows, cols);
            if (crossword != nullptr) return crossword;
          }
        }
      }
    }
    return nullptr;
  }

  shared_ptr<CrossWord> findCrossword(int rows, int cols) {
    const Bucket& horizontals = (*buckets)[cols];
    const Bucket& verticals   = (*buckets)[rows];
    cout << rows << "x" << cols << " / "
         << verticals.size() << "x" << horizontals.size() << endl;
    horizontals.indexWords();
    if (cols != rows) {
      verticals.indexWords();
    }
    index = 0;
    vector<future<shared_ptr<CrossWord>>> futures;
    if (horizontals.size() <= verticals.size()) {
      swap(rows, cols);
    }
    if (threads == 1) {
      return tryBuildCrossword(rows,cols);
    } else {
      for (int i = 0; i < threads; ++i) {
        futures.push_back(async(launch::async,
                                &LargestCrosswordProblem::tryBuildCrossword,
                                this,
                                rows,
                                cols));
      }
      while (true) {
        for (auto &f: futures) {
          f.wait();
        }
        for (auto &f: futures) {
          auto res = f.get();
          if (res != nullptr) {
            return res;
          }
        }
        return nullptr;
      }
    }
  }

  shared_ptr<CrossWord> tryBuildCrossword(int rows, int cols) {
    const Bucket &horizontals = (*buckets)[cols];
    const Bucket &verticals   = (*buckets)[rows];
    if (rows == cols && horizontals.size() < 2*rows) return nullptr;
    shared_ptr<CrossWord> crossword{make_shared<CrossWord>(rows, cols)};

    auto &words = verticals.getWords();
    for (int i=index.fetch_add(1, memory_order_relaxed);
           i < verticals.size() && !aborted;
           i=index.fetch_add(1, memory_order_relaxed)) {
      ostringstream str;
      str << rows << 'x' << cols << ": " << i << " of " << verticals.size() << ' ' << this_thread::get_id() << endl;
      cout << str.str();
      crossword->pushVertical(words[i]);
      if (tryFill(crossword.get())) {
        // set flag to abort all other async calls
        aborted = true;
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
    const Bucket& horizontals = (*buckets)[crossword->cols()];
    const Bucket &verticals   = (*buckets)[crossword->rows()];
    if (! crossword->isPartialOk(getBuckets())) return false;
    if (crossword->isFull()) return true;
    for (auto &s: verticals.getWords()) {
      if (aborted) return false;
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

struct Config {
  int threads {1};
  int rows    {0};
  int cols    {0};
  bool print_buckets{false};

  Config(int argc, char* argv[]) {
    int t1, t2;
    for (int i=1; i<argc; ++i) {
      if (1 == sscanf(argv[i], "--threads=%d", &t1)) {
        threads = t1;
      } else if (2 == sscanf(argv[i], "--size=%dx%d", &t1, &t2)) {
        rows = t1;
        cols = t2;
      } else if (1 == sscanf(argv[i], "--print-buckets=%d", &t1)) {
        print_buckets = t1 != 0;
      } else {
        cout << "Unrecognised parameter " << argv[i] << endl;
      }
    }
  }
};

int main(int argc, char* argv[]) {
  Config config {argc, argv};
  threads = config.threads;

  LargestCrosswordProblem p("parole.txt");
  if (config.print_buckets) {
    int i = 0;
    for (auto &b: p.getBuckets()) {
      cout << i++ << " " << b.size() << endl;
    }
  }
  if (config.rows == 0 && config.cols == 0) {
    cout << p.findLargestCrossword() << endl;
  } else {
    auto crossword = p.findCrossword(config.rows, config.cols);
    if (crossword != nullptr) {
      cout << *crossword << endl;
    }
  }
  return 0;
}


