#include "basic.h"
#include <iostream>
#include <cmath>

namespace bf {

size_t basic_bloom_filter::m(double fp, size_t capacity)
{
  auto ln2 = std::log(2);
  return std::ceil(-(capacity * std::log(fp) / ln2 / ln2));
}

size_t basic_bloom_filter::k(size_t cells, size_t capacity)
{
  auto frac = static_cast<double>(cells) / static_cast<double>(capacity);
  return std::ceil(frac * std::log(2));
}

basic_bloom_filter::basic_bloom_filter(hasher h, size_t cells)
  : hasher_(std::move(h)),
    bits_(cells)
{
}

basic_bloom_filter::basic_bloom_filter(double fp, size_t capacity, size_t seed,
                                       bool double_hashing)
{
  auto required_cells = m(fp, capacity);
  // auto optimal_k = k(required_cells, capacity);
  auto optimal_k = 1;
  std::cout << "Using " << optimal_k << " hash functions\n" << std::endl;
  bits_.resize(required_cells);
  hasher_ = make_hasher(optimal_k, seed, double_hashing);
}

basic_bloom_filter::basic_bloom_filter(basic_bloom_filter&& other)
  : hasher_(std::move(other.hasher_)),
    bits_(std::move(other.bits_))
{
}

void basic_bloom_filter::add(object const& o)
{
  //std::cout << "enter basic_bloom_filter::add" << std::endl;
  //std::cout << "cc" << std::endl;
  for (auto d : hasher_(o))
    bits_.set(d % bits_.size());
  //std::cout << "eee" << std::endl;
}

size_t basic_bloom_filter::lookup(object const& o) const
{
  //std::cout << "Enter basic_bloom_filter::lookup" << std::endl;
  for (auto d : hasher_(o))
    if (! bits_[d % bits_.size()])
      return 0;
  return 1;
}

void basic_bloom_filter::clear()
{
  bits_.reset();
}

void basic_bloom_filter::remove(object const& o)
{
  for (auto d : hasher_(o))
    bits_.reset(d % bits_.size());
}

void basic_bloom_filter::swap(basic_bloom_filter& other)
{
  using std::swap;
  swap(hasher_, other.hasher_);
  swap(bits_, other.bits_);
}

} // namespace bf

