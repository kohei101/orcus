/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "orcus/css_parser_base.hpp"
#include "orcus/parser_global.hpp"

#include <cstring>
#include <cassert>
#include <sstream>
#include <limits>

using namespace std;

namespace orcus { namespace css {

parse_error::parse_error(const std::string& msg) : general_error(msg) {}

void parse_error::throw_with(const char* msg_before, char c, const char* msg_after)
{
    std::ostringstream os;

    if (msg_before)
        os << msg_before;

    os << c;

    if (msg_after)
        os << msg_after;

    throw css::parse_error(os.str());
}

void parse_error::throw_with(
    const char* msg_before, const char* p, size_t n, const char* msg_after)
{
    std::ostringstream os;

    if (msg_before)
        os << msg_before;

    write_to(os, p, n);

    if (msg_after)
        os << msg_after;

    throw css::parse_error(os.str());
}

parser_base::parser_base(const char* p, size_t n) :
    mp_char(p),
    m_pos(0),
    m_length(n),
    m_simple_selector_count(0),
    m_combinator(combinator_descendant) {}

void parser_base::next()
{
    ++m_pos;
    ++mp_char;
}

char parser_base::cur_char() const
{
    return *mp_char;
}

char parser_base::next_char() const
{
    const char* p = mp_char;
    ++p;
    return *p;
}

size_t parser_base::remaining_size() const { return m_length - m_pos - 1; }
bool parser_base::has_char() const { return m_pos < m_length; }

void parser_base::identifier(const char*& p, size_t& len, const char* extra)
{
    p = mp_char;
    len = 1;
    for (next(); has_char(); next(), ++len)
    {
        char c = cur_char();
        if (is_alpha(c) || is_name_char(c) || is_numeric(c))
            continue;

        if (extra)
        {
            // See if the character is one of the extra allowed characters.
            if (is_in(c, extra))
                continue;
        }
        return;
    }
}

uint8_t parser_base::parse_uint8()
{
    // 0 - 255
    int val = 0;
    size_t len = 0;
    for (; has_char() && len <= 3; next())
    {
        char c = cur_char();
        if (!is_numeric(c))
            break;

        ++len;
        val *= 10;
        val += c - '0';
    }

    if (!len)
        throw parse_error("parse_uint8: no digit encountered.");

    int maxval = std::numeric_limits<uint8_t>::max();
    if (val > maxval)
        val = maxval;

    return static_cast<uint8_t>(val);
}

void parser_base::skip_to(const char*&p, size_t& len, char c)
{
    p = mp_char;
    len = 1;
    for (next(); has_char(); next(), ++len)
    {
        if (cur_char() == c)
            return;
    }
}

void parser_base::skip_blanks()
{
    for (; has_char(); next())
    {
        if (!is_blank(*mp_char))
            break;
    }
}

void parser_base::skip_blanks_reverse()
{
    const char* p = mp_char + remaining_size();
    for (; p != mp_char; --p, --m_length)
    {
        if (!is_blank(*p))
            break;
    }
}

void parser_base::shrink_stream()
{
    // Skip any leading blanks.
    skip_blanks();

    if (!remaining_size())
        return;

    // Skip any trailing blanks.
    skip_blanks_reverse();

    // Skip leading <!-- if present.

    const char* com_open = "<!--";
    size_t com_open_len = std::strlen(com_open);
    if (remaining_size() < com_open_len)
        // Not enough stream left.  Bail out.
        return;

    const char* p = mp_char;
    for (size_t i = 0; i < com_open_len; ++i, ++p)
    {
        if (*p != com_open[i])
            return;
        next();
    }
    mp_char = p;

    // Skip leading blanks once again.
    skip_blanks();

    // Skip trailing --> if present.
    const char* com_close = "-->";
    size_t com_close_len = std::strlen(com_close);
    size_t n = remaining_size();
    if (n < com_close_len)
        // Not enough stream left.  Bail out.
        return;

    p = mp_char + n; // move to the last char.
    for (size_t i = com_close_len; i > 0; --i, --p)
    {
        if (*p != com_close[i-1])
            return;
    }
    m_length -= com_close_len;

    skip_blanks_reverse();
}

bool parser_base::skip_comment()
{
    char c = cur_char();
    if (c != '/')
        return false;

    if (remaining_size() > 2 && next_char() == '*')
    {
        next();
        comment();
        skip_blanks();
        return true;
    }

    return false;
}

void parser_base::comment()
{
    assert(cur_char() == '*');

    // Parse until we reach either EOF or '*/'.
    bool has_star = false;
    for (next(); has_char(); next())
    {
        char c = cur_char();
        if (has_star && c == '/')
        {
            next();
            return;
        }
        has_star = (c == '*');
    }

    // EOF reached.
}

void parser_base::skip_comments_and_blanks()
{
    skip_blanks();
    while (skip_comment())
        ;
}

void parser_base::set_combinator(char c, css::combinator_t combinator)
{
    if (!m_simple_selector_count)
        css::parse_error::throw_with(
            "set_combinator: combinator '", c, "' encountered without parent element.");

    m_combinator = combinator;
    next();
    skip_comments_and_blanks();
}

void parser_base::reset_before_block()
{
    m_simple_selector_count = 0;
    m_combinator = css::combinator_descendant;
}

}}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
