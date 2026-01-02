#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <type_traits>

namespace MikuMikuWorld
{
	class Utilities
	{
	  public:
		static std::string getCurrentDateTime();
		static std::string getSystemLocale();
		static std::string getDivisionString(int div);
		static std::vector<std::string> splitString(const std::string& base, const char delimiter);
	};

	enum class ResultStatus
	{
		Success,
		Warning,
		Error
	};

	class Result
	{
	  private:
		ResultStatus status;
		std::string message;

	  public:
		Result(ResultStatus _status, std::string _msg = "") : status{ _status }, message{ _msg } {}

		ResultStatus getStatus() const { return status; }
		std::string getMessage() const { return message; }
		bool isOk() const { return status == ResultStatus::Success; }

		static Result Ok() { return Result(ResultStatus::Success); }
	};

	constexpr static const char* boolToString(bool value) { return value ? "true" : "false"; }

	template <typename, typename = void> struct size_type_trait
	{
		using type = std::size_t;
	};

	template <typename T>
	struct size_type_trait<T, std::void_t<typename std::remove_reference_t<T>::size_type>>
	{
		using type = typename std::remove_reference_t<T>::size_type;
	};

	template <typename ArrayType, typename IndexType, typename SizeType = typename size_type_trait<ArrayType>::type>
	static auto arrayGetItemSafe(ArrayType& arr, IndexType idx) -> decltype(arr[SizeType(0)])
	{
		SizeType index = static_cast<SizeType>(idx);
		if (index < 0 || index >= std::size(arr))
			throw std::runtime_error("Index out of range!");
		return arr[index];
	}

	template <typename ArrayType> static size_t arrayLength(const ArrayType& arr)
	{
		static_assert(std::is_array_v<ArrayType>);
		return (sizeof(arr) / sizeof(arr[0]));
	}

	template <typename Array>
	static inline bool isArrayIndexInBounds(size_t index, const Array& arr)
	{
		return index >= 0 && index < arrayLength(arr);
	}

	template <typename T>
	static inline bool isArrayIndexInBounds(size_t index, const std::vector<T>& arr)
	{
		return index >= 0 && index < arr.size();
	}

	template <typename Type>
	static size_t findArrayItem(const Type& item, const Type array[], size_t length)
	{
		for (int i = 0; i < length; i++)
		{
			if (array[i] == item)
				return i;
		}

		return -1;
	}

	static size_t findArrayItem(const char* item, const char* const array[], size_t length)
	{
		for (int i = 0; i < length; i++)
		{
			if (!strcmp(item, array[i]))
				return i;
		}

		return -1;
	}
}
