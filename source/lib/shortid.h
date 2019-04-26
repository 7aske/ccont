//
//  ShortID.h
//  SocketServer
//
//  Created by Kay Makowsky on 16.06.16.
//  Copyright © 2016 Kay Makowsky. All rights reserved.
//

#ifndef ShortID_h
#define ShortID_h

#include <string>
#include <cmath>

const static std::string Alphabet = "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

class ShortID
{
public:
	static double BcPow(double a, double b)
	{
		return std::floor(std::pow(a, b));
	}

	static long Decode(std::string value, int pad = 0)
	{
		long len = value.length() - 1;
		unsigned long result = 0;

		for (long t = len; t >= 0; t--)
		{
			unsigned long bcp = (unsigned long)BcPow(Alphabet.length(), len - t);
			result += (unsigned long)Alphabet.find(value[t]) * bcp;
		}

		if (pad > 0)
		{
			result -= (unsigned long)BcPow(Alphabet.length(), pad);
		}

		return result;
	}

	static std::string Encode(unsigned long value, int pad = 0)
	{
		std::string result = "";

		if (pad > 0)
		{
			value += (unsigned long)BcPow(Alphabet.length(), pad);
		}
		int lg = std::log(value) / std::log(Alphabet.length());
		for (int t = (value != 0 ? lg : 0); t >= 0; t--)
		{
			unsigned long bcp = (unsigned long)BcPow(Alphabet.length(), t);
			unsigned long a = ((unsigned long)std::floor((double)value / (double)bcp)) % (unsigned long)Alphabet.length();
			result += Alphabet[(int)a];
			value  = value - (a * bcp);
		}

		return result;
	}

private:
	static std::string reverseString(std::string value)
	{
		return std::string (value.rbegin(), value.rend());
	}
};

#endif