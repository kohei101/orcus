/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "orcus/json_document_tree.hpp"
#include "orcus/json_parser.hpp"
#include "orcus/pstring.hpp"
#include "orcus/global.hpp"
#include "orcus/config.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>

#include <boost/current_function.hpp>

namespace orcus {

namespace {

const char* tab = "    ";
constexpr char quote = '"';
constexpr char backslash = '\\';

enum class json_value_type
{
    unset,
    string,
    number,
    object,
    array,
    boolean_true,
    boolean_false,
    null
};

struct json_value
{
    json_value_type type;

    json_value() : type(json_value_type::unset) {}
    json_value(json_value_type _type) : type(_type) {}
    virtual ~json_value() {}
};

struct json_value_string : public json_value
{
    std::string value_string;

    json_value_string() : json_value(json_value_type::string) {}
    json_value_string(const std::string& s) : json_value(json_value_type::string), value_string(s) {}
    json_value_string(const char* p, size_t n) : json_value(json_value_type::string), value_string(p, n) {}
    virtual ~json_value_string() {}
};

struct json_value_number : public json_value
{
    double value_number;

    json_value_number() : json_value(json_value_type::number) {}
    json_value_number(double num) : json_value(json_value_type::number), value_number(num) {}
    virtual ~json_value_number() {}
};

struct json_value_array : public json_value
{
    std::vector<std::unique_ptr<json_value>> value_array;

    json_value_array() : json_value(json_value_type::array) {}
    virtual ~json_value_array() {}
};

struct json_value_object : public json_value
{
    std::vector<std::string> key_order;
    std::unordered_map<std::string, std::unique_ptr<json_value>> value_object;

    json_value_object() : json_value(json_value_type::object) {}
    virtual ~json_value_object() {}
};

void dump_repeat(std::ostringstream& os, const char* s, int repeat)
{
    for (int i = 0; i < repeat; ++i)
        os << s;
}

void dump_string(std::ostringstream& os, const std::string& s)
{
    os << quote;
    for (auto it = s.begin(), ite = s.end(); it != ite; ++it)
    {
        char c = *it;
        if (is_in(c, "\"/"))
            // Escape double quote and forward slash.
            os << backslash;
        else if (c == backslash)
        {
            // Escape a '\' if and only if the next character is not one of control characters.
            auto itnext = it + 1;
            if (itnext == ite || json::get_escape_char_type(*itnext) != json::escape_char_t::control_char)
                os << backslash;
        }
        os << c;
    }
    os << quote;
}

void dump_value(std::ostringstream& os, const json_value* v, int level, const std::string* key = nullptr)
{
    dump_repeat(os, tab, level);

    if (key)
        os << quote << *key << quote << ": ";

    switch (v->type)
    {
        case json_value_type::array:
        {
            auto& vals = static_cast<const json_value_array*>(v)->value_array;
            os << "[" << std::endl;
            size_t n = vals.size();
            for (auto it = vals.begin(), ite = vals.end(); it != ite; ++it)
            {
                dump_value(os, it->get(), level+1);
                size_t pos = std::distance(vals.begin(), it);
                if (pos < (n-1))
                    os << ",";
                os << std::endl;
            }

            dump_repeat(os, tab, level);
            os << "]" << std::endl;
        }
        break;
        case json_value_type::boolean_false:
            os << "false";
        break;
        case json_value_type::boolean_true:
            os << "true";
        break;
        case json_value_type::null:
            os << "null";
        break;
        case json_value_type::number:
            os << static_cast<const json_value_number*>(v)->value_number;
        break;
        case json_value_type::object:
        {
            auto& key_order = static_cast<const json_value_object*>(v)->key_order;
            auto& vals = static_cast<const json_value_object*>(v)->value_object;
            os << "{" << std::endl;
            size_t n = vals.size();

            if (key_order.empty())
            {
                // Dump object's children unordered.
                for (auto it = vals.begin(), ite = vals.end(); it != ite; ++it)
                {
                    auto& key = it->first;
                    auto& val = it->second;
                    dump_value(os, val.get(), level+1, &key);
                    size_t pos = std::distance(vals.begin(), it);
                    if (pos < (n-1))
                        os << ",";
                    os << std::endl;
                }
            }
            else
            {
                // Dump them based on key's original ordering.
                for (auto it = key_order.begin(), ite = key_order.end(); it != ite; ++it)
                {
                    auto& key = *it;
                    auto val_pos = vals.find(key);
                    assert(val_pos != vals.end());

                    dump_value(os, val_pos->second.get(), level+1, &key);
                    size_t pos = std::distance(key_order.begin(), it);
                    if (pos < (n-1))
                        os << ",";
                    os << std::endl;
                }
            }

            dump_repeat(os, tab, level);
            os << "}" << std::endl;
        }
        break;
        case json_value_type::string:
            dump_string(os, static_cast<const json_value_string*>(v)->value_string);
        break;
        case json_value_type::unset:
        default:
            ;
    }
}

std::string dump_json_tree(const json_value* root)
{
    if (root->type == json_value_type::unset)
        return std::string();

    std::ostringstream os;
    dump_value(os, root, 0);
    return os.str();
}

void dump_string_xml(std::ostringstream& os, const std::string& s)
{
    for (auto it = s.begin(), ite = s.end(); it != ite; ++it)
    {
        char c = *it;
        switch (c)
        {
            case '"':
                os << "&quot;";
            break;
            case '<':
                os << "&lt;";
            break;
            case '>':
                os << "&gt;";
            break;
            case '&':
                os << "&amp;";
            break;
            case '\'':
                os << "&apos;";
            break;
            default:
                os << c;
        }
    }
}

void dump_value_xml(std::ostringstream& os, const json_value* v, int level)
{
    switch (v->type)
    {
        case json_value_type::array:
        {
            auto& vals = static_cast<const json_value_array*>(v)->value_array;
            os << "<array>";
            for (auto it = vals.begin(), ite = vals.end(); it != ite; ++it)
            {
                os << "<item>";
                dump_value_xml(os, it->get(), level+1);
                os << "</item>";
            }

            os << "</array>";
        }
        break;
        case json_value_type::boolean_false:
            os << "<false/>";
        break;
        case json_value_type::boolean_true:
            os << "<true/>";
        break;
        case json_value_type::null:
            os << "<null/>";
        break;
        case json_value_type::number:
            os << "<number value=\"";
            os << static_cast<const json_value_number*>(v)->value_number;
            os << "\"/>";
        break;
        case json_value_type::object:
        {
            auto& key_order = static_cast<const json_value_object*>(v)->key_order;
            auto& vals = static_cast<const json_value_object*>(v)->value_object;
            os << "<object>";

            if (key_order.empty())
            {
                // Dump object's children unordered.
                for (auto it = vals.begin(), ite = vals.end(); it != ite; ++it)
                {
                    auto& key = it->first;
                    auto& val = it->second;
                    os << "<item name=\"";
                    dump_string_xml(os, key);
                    os << "\">";
                    dump_value_xml(os, val.get(), level+1);
                    os << "</item>";
                }
            }
            else
            {
                // Dump them based on key's original ordering.
                for (auto it = key_order.begin(), ite = key_order.end(); it != ite; ++it)
                {
                    auto& key = *it;
                    auto val_pos = vals.find(key);
                    assert(val_pos != vals.end());

                    os << "<item name=\"";
                    dump_string_xml(os, key);
                    os << "\">";
                    dump_value_xml(os, val_pos->second.get(), level+1);
                    os << "</item>";
                }
            }

            os << "</object>";
        }
        break;
        case json_value_type::string:
            os << "<string value=\"";
            dump_string_xml(os, static_cast<const json_value_string*>(v)->value_string);
            os << "\"/>";
        break;
        case json_value_type::unset:
        default:
            ;
    }
}

std::string dump_xml_tree(const json_value* root)
{
    if (root->type == json_value_type::unset)
        return std::string();

    std::ostringstream os;
    os << "<?xml version=\"1.0\"?>" << std::endl;
    dump_value_xml(os, root, 0);
    os << std::endl;
    return os.str();
}

struct parser_stack
{
    std::string key;
    json_value* node;

    parser_stack(json_value* _node) : node(_node) {}
};

class parser_handler
{
    const json_config& m_config;

    std::unique_ptr<json_value> m_root;
    std::vector<parser_stack> m_stack;
    std::string m_cur_object_key;

    json_value* push_value(std::unique_ptr<json_value>&& value)
    {
        assert(!m_stack.empty());
        parser_stack& cur = m_stack.back();

        switch (cur.node->type)
        {
            case json_value_type::array:
            {
                json_value_array* jva = static_cast<json_value_array*>(cur.node);
                jva->value_array.push_back(std::move(value));
                return jva->value_array.back().get();
            }
            break;
            case json_value_type::object:
            {
                const std::string& key = cur.key;
                json_value_object* jvo = static_cast<json_value_object*>(cur.node);
                if (m_config.preserve_object_order)
                    jvo->key_order.push_back(key);
                auto r = jvo->value_object.insert(
                    std::make_pair(key, std::move(value)));

                return r.first->second.get();
            }
            break;
            default:
            {
                std::ostringstream os;
                os << BOOST_CURRENT_FUNCTION << ": unstackable JSON value type.";
                throw json::parse_error(os.str());
            }
        }

        return nullptr;
    }

public:
    parser_handler(const json_config& config) : m_config(config) {}

    void begin_parse()
    {
        m_root.reset();
    }

    void end_parse()
    {
    }

    void begin_array()
    {
        if (m_root)
        {
            json_value* jv = push_value(make_unique<json_value_array>());
            assert(jv && jv->type == json_value_type::array);
            m_stack.push_back(parser_stack(jv));
        }
        else
        {
            m_root = make_unique<json_value_array>();
            m_stack.push_back(parser_stack(m_root.get()));
        }
    }

    void end_array()
    {
        assert(!m_stack.empty());
        m_stack.pop_back();
    }

    void begin_object()
    {
        if (m_root)
        {
            json_value* jv = push_value(make_unique<json_value_object>());
            assert(jv && jv->type == json_value_type::object);
            m_stack.push_back(parser_stack(jv));
        }
        else
        {
            m_root = make_unique<json_value_object>();
            m_stack.push_back(parser_stack(m_root.get()));
        }
    }

    void object_key(const char* p, size_t len)
    {
        parser_stack& cur = m_stack.back();
        cur.key = std::string(p, len);
    }

    void end_object()
    {
        assert(!m_stack.empty());
        m_stack.pop_back();
    }

    void boolean_true()
    {
        push_value(make_unique<json_value>(json_value_type::boolean_true));
    }

    void boolean_false()
    {
        push_value(make_unique<json_value>(json_value_type::boolean_false));
    }

    void null()
    {
        push_value(make_unique<json_value>(json_value_type::null));
    }

    void string(const char* p, size_t len)
    {
        push_value(make_unique<json_value_string>(p, len));
    }

    void number(double val)
    {
        push_value(make_unique<json_value_number>(val));
    }

    void swap(std::unique_ptr<json_value>& other)
    {
        other.swap(m_root);
    }
};

}

struct json_document_tree::impl
{
    std::unique_ptr<json_value> m_root;
};

json_document_tree::json_document_tree() : mp_impl(make_unique<impl>()) {}
json_document_tree::~json_document_tree() {}

void json_document_tree::load(const std::string& strm, const json_config& config)
{
    parser_handler hdl(config);
    json_parser<parser_handler> parser(strm.data(), strm.size(), hdl);
    parser.parse();
    hdl.swap(mp_impl->m_root);
}

std::string json_document_tree::dump() const
{
    if (!mp_impl->m_root)
        return std::string();

    return dump_json_tree(mp_impl->m_root.get());
}

std::string json_document_tree::dump_xml() const
{
    return dump_xml_tree(mp_impl->m_root.get());
}

}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
