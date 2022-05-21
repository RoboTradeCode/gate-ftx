#include "Encoding.hpp"

namespace util::_encoding
{
    std::string hmac(const std::string& secret, std::string msg, std::size_t signed_len)
    {
        static HmacCtx hmac;
        char signed_msg[64];

        HMAC_Init_ex(hmac.ctx, secret.data(), (int)secret.size(), EVP_sha256(), nullptr);
        HMAC_Update(hmac.ctx, (unsigned char*)msg.data(), msg.size());
        HMAC_Final(hmac.ctx, (unsigned char*)signed_msg, nullptr);

        return {signed_msg, signed_len};
    }
    std::string str_to_hex(unsigned char* data, std::size_t len)
    {
        std::string s(len * 2, ' ');
        for (std::size_t i = 0; i < len; ++i) {
            s[2 * i] = hexmap[(data[i] & 0xF0) >> 4];
            s[2 * i + 1] = hexmap[data[i] & 0x0F];
        }
        return s;
    }
}

