/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_ORCUS_XPATH_PARSER_HPP
#define INCLUDED_ORCUS_XPATH_PARSER_HPP

#include "orcus/types.hpp"

namespace orcus {

class pstring;
class xmlns_context;

class xpath_parser
{
    const xmlns_context& m_cxt;
    const char* mp_char;
    const char* mp_end;

    enum class token_type { element, attribute };

public:

    struct token
    {
        xmlns_id_t ns;
        pstring name;
        bool attribute;

        token(xmlns_id_t _ns, const pstring& _name, bool _attribute);
        token();
        token(const token& r);
    };

    xpath_parser(const xmlns_context& cxt, const char* p, size_t n);

    token next();
};

}

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
