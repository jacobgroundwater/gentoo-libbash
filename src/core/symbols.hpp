/*
   Please use git log for copyright holder and year information

   This file is part of libbash.

   libbash is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 2 of the License, or
   (at your option) any later version.

   libbash is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with libbash.  If not, see <http://www.gnu.org/licenses/>.
*/
///
/// \file symbols.hpp
/// \brief template implementation for symbols and symbol table
///

#ifndef LIBBASH_CORE_SYMBOLS_HPP_
#define LIBBASH_CORE_SYMBOLS_HPP_

#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <boost/variant.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/numeric/conversion/cast.hpp>

#include "exceptions.h"

///
/// \class converter
/// \brief template class of converter
///
template<typename T>
class converter: public boost::static_visitor<T>
{};

///
/// \class converter
/// \brief specialized converter for long
///
template<>
class converter<long>: public boost::static_visitor<long>
{
public:
  /// \brief converter for long value
  /// \param value the value to be converted
  /// \return the converted long
  long operator() (const long value) const
  {
    return value;
  }

  /// \brief converter for string value
  /// \param value the value to be converted
  /// \return the converted long
  long operator() (const std::string& value) const
  {
    long result = 0;
    try
    {
      result = boost::lexical_cast<long>(value);
    }
    catch(boost::bad_lexical_cast& e)
    {
      std::cerr << "can't cast " << value << " to long" << std::endl;
    }
    return result;
  }
};

///
/// \class converter
/// \brief specialized converter for string
///
template<>
class converter<std::string>:
  public boost::static_visitor<std::string>
{
public:
  /// \brief converter for long value
  /// \param value the value to be converted
  /// \return the converted string
  std::string operator() (const long value) const
  {
    return boost::lexical_cast<std::string>(value);
  }

  /// \brief converter for string value
  /// \param value the value to be converted
  /// \return the converted string
  std::string operator() (const std::string& value) const
  {
    return value;
  }
};

///
/// \class variable
/// \brief implementation for all variable types
///
class variable
{
  /// \brief variable name
  std::string name;

  /// \brief actual value of the variable. We put string in front of long
  ///        because we want "" as default string value; Otherwise we
  ///        will get "0".
  std::map<unsigned, boost::variant<std::string, long>> value;

  /// \brief whether the variable is readonly
  bool readonly;

public:
  /// size_type for array length
  typedef std::map<unsigned, boost::variant<std::string, long>>::size_type size_type;

  /// \brief retrieve variable name
  /// \return const string value of variable name
  const std::string& get_name() const
  {
    return name;
  }

  /// \brief constructor
  /// \param name the name of the variable
  /// \param v the value of the variable
  /// \param ro whether the variable is readonly
  /// \param index the index of the variable, use 0 if it's not an array
  template <typename T>
  variable(const std::string& name,
           const T& v,
           bool ro=false,
           const unsigned index=0)
    : name(name), readonly(ro)
  {
    value[index] = v;
  }

  /// \brief retrieve actual value of the variable, if index is out of bound,
  ///        will return the default value of type T
  /// \param index the index of the variable, use 0 if it's not an array
  /// \return the value of the variable
  template<typename T>
  T get_value(const unsigned index=0) const
  {
    static converter<T> visitor;

    auto iter = value.find(index);
    if(iter == value.end())
        return T{};

    return boost::apply_visitor(visitor, iter->second);
  }

  /// \brief retrieve all values of the array
  /// \param[out] all_values vector that stores all array values
  template<typename T>
  void get_all_values(std::vector<T>& all_values) const
  {
    static converter<T> visitor;

    for(auto iter = value.begin(); iter != value.end(); ++iter)
        all_values.push_back(
                boost::apply_visitor(visitor, iter->second));
  }


  /// \brief set the value of the variable, raise exception if it's readonly
  /// \param new_value the new value to be set
  /// \param index array index, use index=0 if it's not an array
  template <typename T>
  void set_value(const T& new_value,
                 const unsigned index=0)
  {
    if(readonly)
      throw libbash::readonly_exception(get_name() + " is readonly variable");

    value[index] = new_value;
  }

  /// \brief unset the variable, only used for array variable
  /// \param index the index to be unset
  void unset_value(const unsigned index)
  {
    if(readonly)
      throw libbash::readonly_exception(get_name() + " is readonly variable");

    value.erase(index);
  }

  /// \brief get the length of a variable
  /// \param index the index of the variable, use 0 if it's not an array
  /// \return the length of the variable
  std::string::size_type get_length(const unsigned index=0) const
  {
    return get_value<std::string>(index).size();
  }

  /// \brief get the length of an array variable
  /// \return the length of the array
  size_type get_array_length() const
  {
    return value.size();
  }

  size_type get_max_index() const
  {
    return value.size() == 0 ? 0 : value.rbegin()->first;
  }

  /// \brief check whether the value of the variable is null
  /// \return whether the value of the variable is null
  bool is_unset(const unsigned index=0) const
  {
    return value.find(index) == value.end();
  }

  /// \brief check whether the value of the variable is unset
  /// \return whether the value of the variable is unset
  bool is_null(const unsigned index=0) const
  {
    return get_value<std::string>(index) == "";
  }

  /// \brief check whether the value of the variable is readonly
  /// \return whether the value of the variable is readonly
  bool is_readonly() const
  {
    return readonly;
  }

  int shift(unsigned shift_number)
  {
    assert(!readonly&&"readonly variables shouldn't be shifted");
    // Remove this cast after making arithmetic expansion follow POSIX
    unsigned size = boost::numeric_cast<unsigned>(value.size());

    if(shift_number > size) 
    {
      return 1;
    }
    else if(shift_number == size)
    {
      value.clear();
    }
    else
    {
      // copy elements
      for(unsigned i = shift_number + 1; i <= size; ++i)
        value[i - shift_number] = value[i]; 

      // remove tail elements
      for(unsigned i = size - shift_number + 1; i <= size; ++i)
        value.erase(i); 
    }

    return 0;
  }
};

/// \brief the specialized constructor for arrays
/// \param name the variable name
/// \param v the variable value
/// \param ro whether the variable readonly
template <>
inline variable::variable<>(const std::string& name,
                            const std::map<unsigned, std::string>& v,
                            bool ro,
                            unsigned)
    : name(name), value(v.begin(), v.end()), readonly(ro)
{
}

#endif
