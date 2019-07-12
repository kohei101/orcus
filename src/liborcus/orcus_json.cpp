/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "orcus/orcus_json.hpp"
#include "orcus/json_document_tree.hpp"
#include "orcus/config.hpp"
#include "orcus/spreadsheet/import_interface.hpp"
#include "orcus/global.hpp"
#include "orcus/json_parser.hpp"
#include "json_map_tree.hpp"

#include <iostream>

using namespace std;

namespace orcus {

namespace {

struct json_value
{
    enum class value_type { string, numeric, boolean, null };

    value_type type;

    union
    {
        struct { const char* p; size_t n; } str;
        double numeric;
        bool boolean;

    } value;

    json_value(double v) : type(value_type::numeric)
    {
        value.numeric = v;
    }

    json_value(const char* p, size_t n) : type(value_type::string)
    {
        value.str.p = p;
        value.str.n = n;
    }

    json_value(bool v) : type(value_type::boolean)
    {
        value.boolean = v;
    }

    json_value(value_type vt) : type(vt) {}

    void commit(spreadsheet::iface::import_factory& im_factory, const cell_position_t& pos) const
    {
        spreadsheet::iface::import_sheet* sheet =
            im_factory.get_sheet(pos.sheet.data(), pos.sheet.size());

        if (!sheet)
            return;

        switch (type)
        {
            case value_type::string:
            {
                spreadsheet::iface::import_shared_strings* ss = im_factory.get_shared_strings();
                if (!ss)
                    break;

                size_t sid = ss->add(value.str.p, value.str.n);
                sheet->set_string(pos.row, pos.col, sid);
                break;
            }
            case value_type::numeric:
                sheet->set_value(pos.row, pos.col, value.numeric);
                break;
            case value_type::boolean:
                sheet->set_bool(pos.row, pos.col, value.boolean);
                break;
            case value_type::null:
                break;
        }
    }
};

class json_content_handler
{
    json_map_tree::walker m_walker;
    json_map_tree::node* mp_current_node;
    json_map_tree::range_reference_type* mp_increment_row;

    spreadsheet::iface::import_factory& m_im_factory;

public:
    json_content_handler(const json_map_tree& map_tree, spreadsheet::iface::import_factory& im_factory) :
        m_walker(map_tree.get_tree_walker()),
        mp_current_node(nullptr),
        mp_increment_row(nullptr),
        m_im_factory(im_factory) {}

    void begin_parse() {}
    void end_parse() {}

    void begin_array()
    {
        push_node(json_map_tree::input_node_type::array);
    }

    void end_array()
    {
        pop_node(json_map_tree::input_node_type::array);
    }

    void begin_object()
    {
        push_node(json_map_tree::input_node_type::object);
    }

    void object_key(const char* p, size_t len, bool transient)
    {
        m_walker.set_object_key(p, len);
    }

    void end_object()
    {
        pop_node(json_map_tree::input_node_type::object);
    }

    void boolean_true()
    {
        push_node(json_map_tree::input_node_type::value);
        commit_value(true);
        pop_node(json_map_tree::input_node_type::value);
    }

    void boolean_false()
    {
        push_node(json_map_tree::input_node_type::value);
        commit_value(false);
        pop_node(json_map_tree::input_node_type::value);
    }

    void null()
    {
        push_node(json_map_tree::input_node_type::value);
        commit_value(json_value::value_type::null);
        pop_node(json_map_tree::input_node_type::value);
    }

    void string(const char* p, size_t len, bool transient)
    {
        push_node(json_map_tree::input_node_type::value);
        commit_value(json_value(p, len));
        pop_node(json_map_tree::input_node_type::value);
    }

    void number(double val)
    {
        push_node(json_map_tree::input_node_type::value);
        commit_value(val);
        pop_node(json_map_tree::input_node_type::value);
    }

private:

    void push_node(json_map_tree::input_node_type nt)
    {
        mp_current_node = m_walker.push_node(nt);

        if (mp_current_node)
        {
            if (mp_current_node->row_group && mp_increment_row == mp_current_node->row_group)
            {
                // The last closing node was a row group boundary.  Increment the row position.
                ++mp_current_node->row_group->row_position;
                mp_increment_row = nullptr;
            }
        }
    }

    void pop_node(json_map_tree::input_node_type nt)
    {
        mp_current_node = m_walker.pop_node(nt);

        if (mp_current_node)
        {
            if (mp_current_node->row_group)
            {
                mp_increment_row = mp_current_node->row_group;
            }
        }
    }

    void commit_value(const json_value& v)
    {
        if (!mp_current_node)
            return;

        switch (mp_current_node->type)
        {
            case json_map_tree::map_node_type::cell_ref:
            {
                // Single cell reference
                v.commit(m_im_factory, mp_current_node->value.cell_ref->pos);
                break;
            }
            case json_map_tree::map_node_type::range_field_ref:
            {
                // Range field reference.  Offset from the origin before
                // pushing the value.
                spreadsheet::col_t col_offset = mp_current_node->value.range_field_ref->column_pos;
                json_map_tree::range_reference_type* ref = mp_current_node->value.range_field_ref->ref;

                cell_position_t pos = ref->pos; // copy
                pos.col += col_offset;
                pos.row += ref->row_position;
                v.commit(m_im_factory, pos);
                break;
            }
            default:
                ;
        }
    }
};

} // anonymous namespace

struct orcus_json::impl
{
    spreadsheet::iface::import_factory* im_factory;
    spreadsheet::sheet_t sheet_count;
    json_map_tree map_tree;

    impl(spreadsheet::iface::import_factory* _im_factory) :
        im_factory(_im_factory), sheet_count(0) {}
};

orcus_json::orcus_json(spreadsheet::iface::import_factory* im_fact) :
    mp_impl(orcus::make_unique<impl>(im_fact)) {}

orcus_json::~orcus_json() {}

void orcus_json::set_cell_link(const pstring& path, const pstring& sheet, spreadsheet::row_t row, spreadsheet::col_t col)
{
    mp_impl->map_tree.set_cell_link(path, cell_position_t(sheet, row, col));
}

void orcus_json::start_range(const pstring& sheet, spreadsheet::row_t row, spreadsheet::col_t col)
{
    mp_impl->map_tree.start_range(cell_position_t(sheet, row, col));
}

void orcus_json::append_field_link(const pstring& path)
{
    mp_impl->map_tree.append_field_link(path);
}

void orcus_json::set_range_row_group(const pstring& path)
{
    mp_impl->map_tree.set_range_row_group(path);
}

void orcus_json::commit_range()
{
    mp_impl->map_tree.commit_range();
}

void orcus_json::append_sheet(const pstring& name)
{
    if (name.empty())
        return;

    mp_impl->im_factory->append_sheet(mp_impl->sheet_count++, name.data(), name.size());
}

void orcus_json::read_stream(const char* p, size_t n)
{
    json_content_handler hdl(mp_impl->map_tree, *mp_impl->im_factory);
    json_parser<json_content_handler> parser(p, n, hdl);
    parser.parse();
}

void orcus_json::read_map_definition(const char* p, size_t n)
{
    // Since a typical map file will likely be very small, let's be lazy and
    // load the whole thing into a in-memory tree.
    json::document_tree map_doc;
    json_config jc;
    jc.preserve_object_order = false;
    jc.persistent_string_values = false;
    jc.resolve_references = false;

    map_doc.load(p, n, jc);
    json::const_node root = map_doc.get_document_root();

    // Create sheets first.

    if (!root.has_key("sheets"))
        throw json_structure_error("The map definition must contains 'sheets' section.");

    for (const json::const_node& node_name : root.child("sheets"))
        append_sheet(node_name.string_value());

    if (root.has_key("cells"))
    {
        // Set cell links.
        for (const json::const_node& link_node : root.child("cells"))
        {
            pstring path = link_node.child("path").string_value();
            pstring sheet = link_node.child("sheet").string_value();
            spreadsheet::row_t row = link_node.child("row").numeric_value();
            spreadsheet::col_t col = link_node.child("column").numeric_value();

            set_cell_link(path, sheet, row, col);
        }
    }

    if (root.has_key("ranges"))
    {
        // Set range links.
        for (const json::const_node& link_node : root.child("ranges"))
        {
            pstring sheet = link_node.child("sheet").string_value();
            spreadsheet::row_t row = link_node.child("row").numeric_value();
            spreadsheet::col_t col = link_node.child("column").numeric_value();

            start_range(sheet, row, col);

            for (const json::const_node& field_node : link_node.child("fields"))
            {
                pstring path = field_node.child("path").string_value();
                append_field_link(path);
            }

            for (const json::const_node& rg_node : link_node.child("row-groups"))
            {
                pstring path = rg_node.child("path").string_value();
                set_range_row_group(path);
            }

            commit_range();
        }
    }
}

}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
