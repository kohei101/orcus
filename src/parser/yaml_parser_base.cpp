/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "orcus/yaml_parser_base.hpp"
#include "orcus/global.hpp"
#include "orcus/cell_buffer.hpp"
#include "orcus/parser_global.hpp"

#include <mdds/sorted_string_map.hpp>

#include <limits>
#include <vector>
#include <deque>
#include <sstream>
#include <algorithm>

namespace orcus { namespace yaml {

parse_error::parse_error(const std::string& msg, std::ptrdiff_t offset) :
    ::orcus::parse_error(msg, offset) {}

void parse_error::throw_with(const char* msg_before, char c, const char* msg_after, std::ptrdiff_t offset)
{
    throw parse_error(build_message(msg_before, c, msg_after), offset);
}

void parse_error::throw_with(
    const char* msg_before, const char* p, size_t n, const char* msg_after, std::ptrdiff_t offset)
{
    throw parse_error(build_message(msg_before, p, n, msg_after), offset);
}

struct scope
{
    size_t width;
    detail::scope_t type;

    scope(size_t _width) : width(_width), type(detail::scope_t::unset) {}
};

struct parser_base::impl
{
    cell_buffer m_buffer;
    std::vector<scope> m_scopes;
    std::deque<pstring> m_line_buffer;
    const char* m_document;

    size_t m_comment_length;

    bool m_in_literal_block;
    bool m_parsed_to_end_of_line;

    detail::parse_token_t m_last_token;

    impl() :
        m_document(nullptr),
        m_comment_length(0),
        m_in_literal_block(false),
        m_parsed_to_end_of_line(false),
        m_last_token(detail::parse_token_t::unknown) {}
};

const size_t parser_base::parse_indent_blank_line    = std::numeric_limits<size_t>::max();
const size_t parser_base::parse_indent_end_of_stream = std::numeric_limits<size_t>::max() - 1;
const size_t parser_base::scope_empty = std::numeric_limits<size_t>::max() - 2;

parser_base::parser_base(const char* p, size_t n) :
    ::orcus::parser_base(p, n), mp_impl(orcus::make_unique<impl>()) {}

parser_base::~parser_base() {}

void parser_base::push_parse_token(detail::parse_token_t t)
{
    mp_impl->m_last_token = t;
}

detail::parse_token_t parser_base::get_last_parse_token() const
{
    return mp_impl->m_last_token;
}

size_t parser_base::offset_last_char_of_line() const
{
    // The current parser position should be on the linefeed char after
    // calling parse_to_end_of_line().
    assert(mp_impl->m_parsed_to_end_of_line);

    size_t pos = offset();

    if (mp_impl->m_comment_length)
    {
        assert(mp_impl->m_comment_length < pos);
        pos -= mp_impl->m_comment_length; // should be on the '#' character.
    }

    pos -= 1;

    // Ignore any trailing whitespaces.
    const char* p = mp_begin + pos;
    for (; mp_begin < p && *p == ' '; --p, --pos)
        ;

    return pos;
}

size_t parser_base::parse_indent()
{
    for (size_t indent = 0; has_char(); next(), ++indent)
    {
        char c = cur_char();
        switch (c)
        {
            case '#':
                skip_comment();
                return parse_indent_blank_line;
            case '\n':
                next();
                return parse_indent_blank_line;
            case ' ':
                continue;
            default:
                return indent;
        }
    }

    return parse_indent_end_of_stream;
}

pstring parser_base::parse_to_end_of_line()
{
    const char* p = mp_char;
    size_t len = 0;
    for (; has_char(); next(), ++len)
    {
        switch (cur_char())
        {
            case '#':
                skip_comment();
            break;
            case '\'':
            {
                const char* p_open_quote = mp_char;

                // character immediately after the closing quote.
                const char* p_end =
                    parse_to_closing_single_quote(mp_char, remaining_size());

                if (!p_end)
                    throw parse_error("parse_to_end_of_line: closing single quote was expected but not found.", offset());

                size_t diff = p_end - p_open_quote - 1;

                // Move the cursor to the closing quote.
                next(diff);
                len += diff;
                assert(cur_char() == '\'');
                continue;
            }
            break;
            case '"':
            {
                const char* p_open_quote = mp_char;

                // character immediately after the closing quote.
                const char* p_end =
                    parse_to_closing_double_quote(mp_char, remaining_size());

                if (!p_end)
                    throw parse_error("parse_to_end_of_line: closing double quote was expected but not found.", offset());

                size_t diff = p_end - p_open_quote - 1;

                // Move the cursor to the closing quote.
                next(diff);
                len += diff;
                assert(cur_char() == '"');
                continue;
            }
            break;
            case '\n':
                next();
            break;
            default:
                continue;
        }
        break;
    }

    pstring ret(p, len);
    mp_impl->m_parsed_to_end_of_line = true;
    return ret;
}

void parser_base::skip_comment()
{
    assert(cur_char() == '#');

    size_t n = 1;

    for (; has_char(); next(), ++n)
    {
        if (cur_char() == '\n')
        {
            next();
            break;
        }
    }

    mp_impl->m_comment_length = n;
}

void parser_base::reset_on_new_line()
{
    mp_impl->m_comment_length = 0;
    mp_impl->m_parsed_to_end_of_line = false;
}

size_t parser_base::get_scope() const
{
    return (mp_impl->m_scopes.empty()) ? scope_empty : mp_impl->m_scopes.back().width;
}

void parser_base::push_scope(size_t scope_width)
{
    mp_impl->m_scopes.emplace_back(scope_width);
}

void parser_base::clear_scopes()
{
    mp_impl->m_scopes.clear();
}

detail::scope_t parser_base::get_scope_type() const
{
    assert(!mp_impl->m_scopes.empty());
    return mp_impl->m_scopes.back().type;
}

void parser_base::set_scope_type(detail::scope_t type)
{
    assert(!mp_impl->m_scopes.empty());
    mp_impl->m_scopes.back().type = type;
}

size_t parser_base::pop_scope()
{
    assert(!mp_impl->m_scopes.empty());
    mp_impl->m_scopes.pop_back();
    return get_scope();
}

void parser_base::push_line_back(const char* p, size_t n)
{
    mp_impl->m_line_buffer.emplace_back(p, n);
}

pstring parser_base::pop_line_front()
{
    assert(!mp_impl->m_line_buffer.empty());

    pstring ret = mp_impl->m_line_buffer.front();
    mp_impl->m_line_buffer.pop_front();
    return ret;
}

bool parser_base::has_line_buffer() const
{
    return !mp_impl->m_line_buffer.empty();
}

size_t parser_base::get_line_buffer_count() const
{
    return mp_impl->m_line_buffer.size();
}

pstring parser_base::merge_line_buffer()
{
    assert(!mp_impl->m_line_buffer.empty());

    char sep = mp_impl->m_in_literal_block ? '\n' : ' ';

    cell_buffer& buf = mp_impl->m_buffer;
    buf.reset();

    auto it = mp_impl->m_line_buffer.begin();
    buf.append(it->get(), it->size());
    ++it;

    std::for_each(it, mp_impl->m_line_buffer.end(),
        [&](const pstring& line)
        {
            buf.append(&sep, 1);
            buf.append(line.get(), line.size());
        }
    );

    mp_impl->m_line_buffer.clear();
    mp_impl->m_in_literal_block = false;

    return pstring(buf.get(), buf.size());
}

const char* parser_base::get_doc_hash() const
{
    return mp_impl->m_document;
}

void parser_base::set_doc_hash(const char* hash)
{
    mp_impl->m_document = hash;
}

namespace {

mdds::sorted_string_map<detail::keyword_t>::entry keyword_entries[] = {
    { ORCUS_ASCII("FALSE"), detail::keyword_t::boolean_false },
    { ORCUS_ASCII("False"), detail::keyword_t::boolean_false },
    { ORCUS_ASCII("N"),     detail::keyword_t::boolean_false },
    { ORCUS_ASCII("NO"),    detail::keyword_t::boolean_false },
    { ORCUS_ASCII("NULL"),  detail::keyword_t::null          },
    { ORCUS_ASCII("No"),    detail::keyword_t::boolean_false },
    { ORCUS_ASCII("Null"),  detail::keyword_t::null          },
    { ORCUS_ASCII("OFF"),   detail::keyword_t::boolean_false },
    { ORCUS_ASCII("ON"),    detail::keyword_t::boolean_true  },
    { ORCUS_ASCII("Off"),   detail::keyword_t::boolean_false },
    { ORCUS_ASCII("On"),    detail::keyword_t::boolean_true  },
    { ORCUS_ASCII("TRUE"),  detail::keyword_t::boolean_true  },
    { ORCUS_ASCII("True"),  detail::keyword_t::boolean_true  },
    { ORCUS_ASCII("Y"),     detail::keyword_t::boolean_true  },
    { ORCUS_ASCII("YES"),   detail::keyword_t::boolean_true  },
    { ORCUS_ASCII("Yes"),   detail::keyword_t::boolean_true  },
    { ORCUS_ASCII("false"), detail::keyword_t::boolean_false },
    { ORCUS_ASCII("n"),     detail::keyword_t::boolean_false },
    { ORCUS_ASCII("no"),    detail::keyword_t::boolean_false },
    { ORCUS_ASCII("null"),  detail::keyword_t::null          },
    { ORCUS_ASCII("off"),   detail::keyword_t::boolean_false },
    { ORCUS_ASCII("on"),    detail::keyword_t::boolean_true  },
    { ORCUS_ASCII("true"),  detail::keyword_t::boolean_true  },
    { ORCUS_ASCII("y"),     detail::keyword_t::boolean_true  },
    { ORCUS_ASCII("yes"),   detail::keyword_t::boolean_true  },
    { ORCUS_ASCII("~"),     detail::keyword_t::null          },
};

void throw_quoted_string_parse_error(
    const char* func_name, const parse_quoted_string_state& ret, std::ptrdiff_t offset)
{
    std::ostringstream os;
    os << func_name << ": failed to parse ";
    if (ret.length == parse_quoted_string_state::error_illegal_escape_char)
        os << "due to the presence of illegal escape character.";
    else if (ret.length == parse_quoted_string_state::error_no_closing_quote)
        os << "because the closing quote was not found.";
    else
        os << "due to unknown reason.";

    throw parse_error(os.str(), offset);
}

}

detail::keyword_t parser_base::parse_keyword(const char* p, size_t len)
{
    static mdds::sorted_string_map<detail::keyword_t> map(
        keyword_entries,
        ORCUS_N_ELEMENTS(keyword_entries),
        detail::keyword_t::unknown);

    detail::keyword_t value = map.find(p, len);
    return value;
}

parser_base::key_value parser_base::parse_key_value(const char* p, size_t len)
{
    size_t scope = get_scope();
    assert(scope != scope_empty);

    assert(*p != ' ');
    assert(len);

    const char* p_end = p + len;

    key_value kv;

    char last = 0;
    bool key_found = false;

    const char* p_head = p;

    for (; p != p_end; ++p)
    {
        if (*p == ' ')
        {
            if (!key_found)
            {
                if (last == ':')
                {
                    // Key found.
                    kv.key = pstring(p_head, p-p_head-1).trim();
                    key_found = true;
                    p_head = nullptr;
                }
            }
        }
        else
        {
            if (!p_head)
                p_head = p;
        }

        last = *p;
    }

    assert(p_head);

    if (key_found)
    {
        // Key has already been found and the value comes after the ':'.
        kv.value = pstring(p_head, p-p_head);
    }
    else if (last == ':')
    {
        // Line only contains a key and ends with ':'.
        kv.key = pstring(p_head, p-p_head-1).trim();
    }
    else
    {
        // Key has not been found.
        detail::scope_t st = get_scope_type();
        if (st == detail::scope_t::map)
            throw yaml::parse_error("key was expected, but not found.", offset_last_char_of_line());
    }

    return kv;
}

pstring parser_base::parse_single_quoted_string_value(const char*& p, size_t max_length)
{
    parse_quoted_string_state ret =
        parse_single_quoted_string(p, max_length, mp_impl->m_buffer);

    if (!ret.str)
        throw_quoted_string_parse_error("parse_single_quoted_string_value", ret, offset());

    return pstring(ret.str, ret.length);
}

pstring parser_base::parse_double_quoted_string_value(const char*& p, size_t max_length)
{
    parse_quoted_string_state ret =
        parse_double_quoted_string(p, max_length, mp_impl->m_buffer);

    if (!ret.str)
        throw_quoted_string_parse_error("parse_double_quoted_string_value", ret, offset());

    return pstring(ret.str, ret.length);
}

void parser_base::skip_blanks(const char*& p, size_t len)
{
    const char* p_end = p + len;
    for (; p != p_end && *p == ' '; ++p)
        ;
}

void parser_base::start_literal_block()
{
    mp_impl->m_in_literal_block = true;
}

bool parser_base::in_literal_block() const
{
    return mp_impl->m_in_literal_block;
}

void parser_base::handle_line_in_literal(size_t indent)
{
    size_t cur_scope = get_scope();

    if (!has_line_buffer())
    {
        // Start a new multi-line string scope.

        if (indent == cur_scope)
            throw yaml::parse_error("parse: first line of a literal block must be indented.", offset());

        push_scope(indent);
        set_scope_type(yaml::detail::scope_t::multi_line_string);
    }
    else
    {
        // The current scope is already a multi-line scope.
        assert(get_scope_type() == yaml::detail::scope_t::multi_line_string);
        size_t leading_indent = indent - cur_scope;
        prev(leading_indent);
    }

    pstring line = parse_to_end_of_line();
    push_line_back(line.get(), line.size());
}

void parser_base::handle_line_in_multi_line_string()
{
    if (get_scope_type() != yaml::detail::scope_t::multi_line_string)
        set_scope_type(yaml::detail::scope_t::multi_line_string);

    pstring line = parse_to_end_of_line();
    line = line.trim();
    assert(!line.empty());
    push_line_back(line.get(), line.size());
}

}}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
