#pragma once

#include <string>
#include <vector>

namespace FrameWork
{
    namespace Base64
    {
        static const char* kChars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        inline std::string Encode(const std::string& input)
        {
            std::string out;
            int val = 0, valb = -6;
            for (unsigned char c : input)
            {
                val = (val << 8) + c;
                valb += 8;
                while (valb >= 0)
                {
                    out.push_back(kChars[(val >> valb) & 0x3F]);
                    valb -= 6;
                }
            }
            if (valb > -6)
                out.push_back(kChars[((val << 8) >> (valb + 8)) & 0x3F]);
            while (out.size() % 4)
                out.push_back('=');
            return out;
        }

        inline std::string Decode(const std::string& input)
        {
            std::vector<int> T(256, -1);
            for (int i = 0; i < 64; i++)
                T[(int)kChars[i]] = i;

            std::string out;
            int val = 0, valb = -8;
            for (unsigned char c : input)
            {
                if (T[c] == -1) break;
                val = (val << 6) + T[c];
                valb += 6;
                if (valb >= 0)
                {
                    out.push_back(char((val >> valb) & 0xFF));
                    valb -= 8;
                }
            }
            return out;
        }
    }
}
