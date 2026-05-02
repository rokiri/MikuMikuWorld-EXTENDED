#include "IO.h"
#include <algorithm>
#include <zlib.h>
#include <sstream>
#include <cassert>

namespace IO
{
	char* reverse(char* str)
	{
		char* end = str;
		char* start = str;

		if (!str || !*str)
			return str;
		while (*(end + 1))
			end++;
		while (end > start)
		{
			int ch = *end;
			*end-- = *start;
			*start++ = ch;
		}
		return str;
	}

	char* tostringBaseN(char* buff, long long num, int base)
	{
		int sign = num < 0;
		char* savedbuff = buff;

		if (base < 2 || base >= sizeof(digits))
			return NULL;
		if (buff)
		{
			do
			{
				*buff++ = digits[abs(num % base)];
				num /= base;
			} while (num);
			if (sign)
			{
				*buff++ = '-';
			}
			*buff = 0;
			reverse(savedbuff);
		}
		return savedbuff;
	}

	bool isComment(const std::string_view& line, const std::string_view& delim)
	{
		if (line.empty())
			return true;

		return line.find_first_of(delim) == 0 || line.at(0) == '\n';
	}

	bool startsWith(const std::string_view& line, const std::string_view& key)
	{
		if (line.size() < key.size())
			return false;
		return std::equal(key.begin(), key.end(), line.begin());
	}

	bool endsWith(const std::string_view& line, const std::string_view& key)
	{
		if (line.size() < key.size())
			return false;
		return std::equal(key.rbegin(), key.rend(), line.rbegin());
	}

	bool isDigit(const std::string_view& str)
	{
		if (str.empty())
			return false;

		return std::all_of(str.begin() + (str.at(0) == '-' ? 1 : 0), str.end(), std::isdigit);
	}

	std::string trim(const std::string& line)
	{
		if (line.empty())
			return line;

		size_t start = line.find_first_not_of(" ");
		size_t end = line.find_last_not_of(" ");

		return line.substr(start, end - start + 1);
	}

	std::vector<std::string> split(const std::string& line, std::string_view delim)
	{
		std::vector<std::string> values;
		size_t start = 0;
		size_t end;

		do
		{
			end = line.find_first_of(delim, start);
			values.push_back(line.substr(start, end - start));

			start = end + 1;
		} while (end != std::string::npos);

		return values;
	}

	std::pair<std::string, std::string> split_first(const std::string& line,
	                                                const std::string& delim)
	{
		std::pair<std::string, std::string> values;
		size_t firstDelim = line.find_first_of(delim);
		values.first = line.substr(0, firstDelim);
		if (firstDelim != std::string::npos)
			values.second = line.substr(firstDelim + 1);
		return values;
	}

	std::string concat(const char* s1, const char* s2, const char* join)
	{
		return std::string(s1).append(join).append(s2);
	}

	static std::vector<uint8_t> processCompressionStream(const std::vector<uint8_t>& data,
	                                                     z_stream* stream, int flush,
	                                                     int (*process)(z_streamp, int))
	{
		stream->avail_in = data.size();
		stream->next_in = (Byte*)data.data();

		std::vector<uint8_t> dest;
		std::unique_ptr<Byte[]> buf(new Byte[Z_CHUNK_SIZE]);
		int result = Z_OK;

		do
		{
			stream->next_out = buf.get();
			stream->avail_out = Z_CHUNK_SIZE;

			result = process(stream, flush);
			if (dest.size() < stream->total_out)
			{
				size_t count = Z_CHUNK_SIZE - stream->avail_out;
				for (size_t i = 0; i < count; i++)
					dest.push_back(buf[i]);
			}

		} while (result == Z_OK);

		return dest;
	}

	std::vector<uint8_t> inflateGzip(const std::vector<uint8_t>& data)
	{
		z_stream stream{};
		memset(&stream, 0, sizeof(z_stream));

		int result = inflateInit2(&stream, 15 | 16);
		if (result != Z_OK)
			return {};

		std::vector<uint8_t> dest = processCompressionStream(data, &stream, Z_FULL_FLUSH, inflate);

		inflateEnd(&stream);
		return dest;
	}

	std::vector<uint8_t> deflateGzip(const std::vector<uint8_t>& data)
	{
		z_stream stream{};
		memset(&stream, 0, sizeof(z_stream));

		int result = deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 | 16, 8,
		                          Z_DEFAULT_STRATEGY);
		if (result != Z_OK)
			return {};

		std::vector<uint8_t> dest = processCompressionStream(data, &stream, Z_FINISH, deflate);

		deflateEnd(&stream);
		return dest;
	}

	bool isGzipCompressed(const std::vector<uint8_t>& data)
	{
		return data.size() > 2 && data[0] == 0x1F && data[1] == 0x8B;
	}
}
