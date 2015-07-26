/*
Copyright (C) 2014 Declan Ireland <http://github.com/torndeco/extDB2>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.

getGUID --
Code to Convert SteamID -> BEGUID
From Frank https://gist.github.com/Fank/11127158

*/

#include "misc.h"

#include <cstdlib>
#include <thread>

#include <boost/crc.hpp>
#include <boost/random/random_device.hpp>
#include <boost/random/uniform_int_distribution.hpp>

#include <Poco/DigestEngine.h>
#include <Poco/NumberFormatter.h>
#include <Poco/NumberParser.h>
#include <Poco/MD4Engine.h>
#include <Poco/MD5Engine.h>
#include <Poco/StringTokenizer.h>


bool MISC::init(AbstractExt *extension, const std::string &database_id, const std::string &init_str)
{
	extension_ptr = extension;
	return true;
}


void MISC::getCrc32(std::string &input_str, std::string &result)
{
	std::lock_guard<std::mutex> lock(mutex_crc32);
	crc32.reset();
	crc32.process_bytes(input_str.data(), input_str.length());
	result = "[1,\"" + Poco::NumberFormatter::format(crc32.checksum()) + "\"]";
}


void MISC::getMD4(std::string &input_str, std::string &result)
{
	std::lock_guard<std::mutex> lock(mutex_md4);
	md4.update(input_str);
	result = "[1,\"" + Poco::DigestEngine::digestToHex(md4.digest()) + "\"]";
}


void MISC::getMD5(std::string &input_str, std::string &result)
{
	std::lock_guard<std::mutex> lock(mutex_md5);
	md5.update(input_str);
	result = "[1,\"" + Poco::DigestEngine::digestToHex(md5.digest()) + "\"]";
}


void MISC::getBEGUID(std::string &input_str, std::string &result)
// From Frank https://gist.github.com/Fank/11127158
// Modified to use libpoco
{
	bool status = true;

	if (input_str.empty())
	{
		status = false;
		result = "[0,\"Invalid SteamID\"";
	}
	else
	{
		for (unsigned int index=0; index < input_str.length(); index++)
		{
			if (!std::isdigit(input_str[index]))
			{
				status = false;
				result = "[0,\"Invalid SteamID\"";
				break;
			}
		}
	}

	if (status)
	{
		Poco::Int64 steamID = Poco::NumberParser::parse64(input_str);
		Poco::Int8 i = 0, parts[8] = { 0 };

		do
		{
			parts[i++] = steamID & 0xFFu;
		} while (steamID >>= 8);

		std::stringstream bestring;
		bestring << "BE";
		for (auto &part: parts)
		{
			bestring << char(part);
		}

		std::lock_guard<std::mutex> lock(mutex_md5);
		md5.update(bestring.str());
		result = "[1,\"" + Poco::DigestEngine::digestToHex(md5.digest()) + "\"]";
	}
}


void MISC::getRandomString(std::string &input_str, std::string &result)
{
	Poco::StringTokenizer tokens(input_str, ":");
	if (tokens.count() != 2)
	{
		result = "[0,\"Error Syntax\"]";
	}
	else
	{
		int num_of_strings;
		int len_of_string;

		if (!((Poco::NumberParser::tryParse(tokens[0], num_of_strings)) && (Poco::NumberParser::tryParse(tokens[1], len_of_string))))
		{
			result = "[0,\"Error Invalid Number\"]";
		}
		else
		{
			if (num_of_strings <= 0)
			{
				result = "[0,\"Error Number of Variable <= 0\"]";
			}
			else
			{
				extension_ptr->getUniqueString(len_of_string, num_of_strings, result);
			}
		}
	}
}


bool MISC::callProtocol(std::string input_str, std::string &result, const bool async_method, const unsigned int unique_id)
{
	// Protocol
	std::string command;
	std::string data;

	const std::string::size_type found = input_str.find(":");

	if (found==std::string::npos)  // Check Invalid Format
	{
		command = input_str;
	}
	else
	{
		command = input_str.substr(0,found);
		data = input_str.substr(found+1);
	}
	if (command == "TIME")
	{
		extension_ptr->getDateTime(data, result);
	}
	else if (command == "BEGUID")
	{
		getBEGUID(data, result);
	}
	else if (command == "CRC32")
	{
		getCrc32(data, result);
	}
	else if (command == "MD4")
	{
		getMD4(data, result);
	}
	else if (command == "MD5")
	{
		getMD5(data, result);
	}
	else if (command == "RANDOM_UNIQUE_STRING")
	{
		getRandomString(data, result);
	}
	else if (command == "TEST")
	{
		result = data;
	}
	else
	{
		result = "[0,\"Error Invalid Format\"]";
		extension_ptr->logger->warn("extDB2: Misc Invalid Command: {0}", command);
	}
	return true;
}