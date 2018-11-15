/*
 * Copyright 2017 MapD Technologies, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// http://stackoverflow.com/questions/5279051/how-can-i-create-cartesian-product-of-vector-of-vectors

#include <cassert>

#include <limits>
#include <stdexcept>
#include <vector>
#include <iostream>

#include <boost/iterator/iterator_facade.hpp>

//! Class iterating over the Cartesian product of a forward iterable container of forward iterable containers
template <typename T>
class CartesianProductIterator : public boost::iterator_facade<CartesianProductIterator<T>,
                                                               std::vector<typename T::value_type::value_type> const,
                                                               boost::forward_traversal_tag> {
 public:
  //! Delete default constructor
  CartesianProductIterator() = delete;

  //! Constructor setting the underlying iterator and position
  /*!
   * \param[in] structure The underlying structure
   * \param[in] pos The position the iterator should be initialized to.  std::numeric_limits<std::size_t>::max()stands
   * for the end, the position after the last element.
   */
  explicit CartesianProductIterator(T const& structure, std::size_t pos);
  std::vector<typename T::value_type::value_type> const& getValue(std::size_t i) const;
  std::vector<typename T::value_type::value_type>& getValue_nc(std::size_t i) const;
  std::size_t const getSize() const{
    return result_.size();
  }
 private:
  //! Give types more descriptive names
  // \{
  typedef T OuterContainer;
  typedef typename T::value_type Container;
  typedef typename T::value_type::value_type Content;
  // \}

  //! Grant access to boost::iterator_facade
  friend class boost::iterator_core_access;

  //! Increment iterator
  void increment();

  //! Check for equality
  bool equal(CartesianProductIterator<T> const& other) const;

  //! Dereference iterator
  std::vector<Content> const& dereference() const;

  //! The part we are iterating over
  OuterContainer const& structure_;

  //! The position in the Cartesian product
  /*!
   * For each element of structure_, give the position in it.
   * The empty vector represents the end position.
   * Note that this vector has a size equal to structure->size(), or is empty.
   */
  std::vector<typename Container::const_iterator> position_;

  //! The position just indexed by an integer
  std::size_t absolutePosition_ = 0;

  //! The begin iterators, saved for convenience and performance
  std::vector<typename Container::const_iterator> cbegins_;

  //! The end iterators, saved for convenience and performance
  std::vector<typename Container::const_iterator> cends_;

  //! Used for returning references
  /*!
   * We initialize with one empty element, so that we only need to add more elements in increment().
   */
  mutable std::vector<std::vector<Content>> result_;

  //! The size of the instance of OuterContainer
  std::size_t size_ = 0;

  size_t order_size_;
};

template <typename T>
CartesianProductIterator<T>::CartesianProductIterator(OuterContainer const& structure, std::size_t pos)
    : structure_(structure) {

  std::vector<size_t> index_;
  std::vector<size_t> input_size;
  order_size_ = 1;
  for (auto &entry:structure_) {
      if(entry.size()!=0) {
          ++size_;
          index_.push_back(0);
          input_size.push_back(entry.size());
          order_size_ *= entry.size();
      }
  }
//  std::cout<<"amount: "<<amount_<<std::endl;
  //last_stop_ = 0;
  if (pos == std::numeric_limits<std::size_t>::max() || size_ == 0) {
    absolutePosition_ = std::numeric_limits<std::size_t>::max();
    return;
  }
  std::vector<size_t> steps(input_size);
  for (size_t i = 0; i != order_size_; ++i) {
    std::vector<Content> row;
    for(size_t j=0;j<size_;++j) {
      row.push_back(structure[j][index_[j]]);
    }
    result_.push_back(row);
    for(size_t j=size_;j>0;--j){
      if(--steps[j-1]==0){
        steps[j-1]=input_size[j-1];
        continue;
      }
      else{
        index_[j-1]=(index_[j-1]+1)%input_size[j-1];
        break;
      }
    }
  }
  // Increment to wanted position
  for (std::size_t i = 0; i < pos; ++i) {
    increment();
  }
//  for (auto& entry : structure_) {
//    cbegins_.push_back(entry.cbegin());
//    cends_.push_back(entry.cend());
//    ++size_;
//  }
//
//  if (pos == std::numeric_limits<std::size_t>::max() || size_ == 0) {
//    absolutePosition_ = std::numeric_limits<std::size_t>::max();
//    return;
//  }
//
//  // Initialize with all cbegin() position
//  position_.reserve(size_);
//  for (std::size_t i = 0; i != size_; ++i) {
//    position_.push_back(cbegins_[i]);
//    if (cbegins_[i] == cends_[i]) {
//      // Empty member, so Cartesian product is empty
//      absolutePosition_ = std::numeric_limits<std::size_t>::max();
//      return;
//    }
//  }
//
//  // Increment to wanted position
//  for (std::size_t i = 0; i < pos; ++i) {
//    increment();
//  }
}

template <typename T>
void CartesianProductIterator<T>::increment() {
  if (absolutePosition_ == std::numeric_limits<std::size_t>::max()) {
    return;
  }
  if(absolutePosition_>=order_size_-1){
    absolutePosition_=std::numeric_limits<std::size_t>::max();
    return;
  }
  ++absolutePosition_;
//  if (absolutePosition_ == std::numeric_limits<std::size_t>::max()) {
//    return;
//  }
//
//  std::size_t pos = size_ - 1;
//
//  // Descend as far as necessary
//  while (++(position_[pos]) == cends_[pos] && pos != 0) {
//    --pos;
//  }
//  if (position_[pos] == cends_[pos]) {
//    assert(pos == 0);
//    absolutePosition_ = std::numeric_limits<std::size_t>::max();
//    return;
//  }
//  // Set all to begin behind pos
//  for (++pos; pos != size_; ++pos) {
//    position_[pos] = cbegins_[pos];
//  }
//  ++absolutePosition_;
//  result_.emplace_back();

}

template <typename T>
std::vector<typename T::value_type::value_type> const& CartesianProductIterator<T>::dereference() const {
  if (absolutePosition_ == std::numeric_limits<std::size_t>::max()) {
    throw new std::out_of_range("Out of bound dereference in CartesianProductIterator\n");
  }
  auto &result = result_[absolutePosition_];
  return result;
}
template <typename T>
std::vector<typename T::value_type::value_type> const& CartesianProductIterator<T>::getValue(std::size_t i) const {
  if (i >= result_.size()) {
    throw new std::out_of_range("Out of bound dereference in CartesianProductIterator\n");
  }
  auto &result = result_[i];
  return result;
}
template <typename T>
std::vector<typename T::value_type::value_type> &CartesianProductIterator<T>::getValue_nc(std::size_t i) const {
    if (i >= result_.size()) {
        throw new std::out_of_range("Out of bound dereference in CartesianProductIterator\n");
    }
    auto &result = result_[i];
    return result;
}

template <typename T>
bool CartesianProductIterator<T>::equal(CartesianProductIterator<T> const& other) const {
  return absolutePosition_ == other.absolutePosition_ && structure_ == other.structure_;
}

//! Class that turns a forward iterable container of forward iterable containers into a forward iterable container which
// iterates over the Cartesian product of the forward iterable containers
template <typename T>
class CartesianProduct {
 public:
  //! Constructor from type T
  explicit CartesianProduct(T const& t) : t_(t), iterator_(t,0) { }

  //! Iterator to beginning of Cartesian product
  CartesianProductIterator<T> begin() const { return CartesianProductIterator<T>(t_, 0); }

  //! Iterator behind the last element of the Cartesian product
  CartesianProductIterator<T> end() const {
    return CartesianProductIterator<T>(t_, std::numeric_limits<std::size_t>::max());
  }
  std::vector<typename T::value_type::value_type> const& operator[] (const std::size_t pos) const {
    return iterator_.getValue(pos);
  };
  std::vector<typename T::value_type::value_type> &operator[] (const std::size_t pos){
    return iterator_.getValue_nc(pos);
  };
  std::size_t const size() const{
    return iterator_.getSize();
  }
 private:
  T const& t_;
  CartesianProductIterator<T> iterator_;
};
