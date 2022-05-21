#pragma once
#ifndef ENCODING_H
#define ENCODING_H
#include <openssl/evp.h>
#include <openssl/hmac.h>

#include <string>

namespace util::_encoding
{

    namespace
    {
        struct HmacCtx
        {
            HMAC_CTX *ctx = HMAC_CTX_new();
            HmacCtx()
            {
                //HMAC_CTX_init(&ctx);
            }
            ~HmacCtx()
            {
                //HMAC_CTX_cleanup(&ctx);
            }
        };
    }
    std::string hmac(const std::string& secret, std::string msg, std::size_t signed_len);

    namespace {
    constexpr char hexmap[] = {'0',
                           '1',
                           '2',
                           '3',
                           '4',
                           '5',
                           '6',
                           '7',
                           '8',
                           '9',
                           'a',
                           'b',
                           'c',
                           'd',
                           'e',
                           'f'};
    }

    std::string str_to_hex(unsigned char* data, std::size_t len);
}
#endif
