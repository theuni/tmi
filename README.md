tmi: Tiny Multi-Index
=====================================

A slimmed down but very functional version of boost::multi\_index

Enough is implemented here to work as a near drop-in replacement for Bitcoin
Core's usage of multi-index. It passes all tests and matches or beats Boost in
our benchmarks.

Still tons of work to do:
- Replace the red-black-tree which was copied from LLVM as a starting point
- Lots of docs
- Lots of tests
- Lots of benchmarks

WIP
