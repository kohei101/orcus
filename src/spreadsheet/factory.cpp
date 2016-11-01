/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "orcus/spreadsheet/factory.hpp"

#include "orcus/spreadsheet/pivot.hpp"
#include "orcus/spreadsheet/shared_strings.hpp"
#include "orcus/spreadsheet/styles.hpp"
#include "orcus/spreadsheet/sheet.hpp"
#include "orcus/spreadsheet/document.hpp"
#include "orcus/spreadsheet/global_settings.hpp"
#include "orcus/spreadsheet/import_interface_pivot.hpp"
#include "orcus/global.hpp"
#include "orcus/string_pool.hpp"
#include "orcus/exception.hpp"

#include <ixion/formula_name_resolver.hpp>

#include <sstream>
#include <cassert>

namespace orcus { namespace spreadsheet {

class import_pc_field_group : public iface::import_pivot_cache_field_group
{
    using numeric_range_type = pivot_cache_group_data_t::numeric_range_type;

    document& m_doc;
    pivot_cache_field_t& m_parent_field;
    std::unique_ptr<pivot_cache_group_data_t> m_data;
    pivot_cache_item_t m_current_field_item;

private:
    pstring intern(const char* p, size_t n)
    {
        return m_doc.get_string_pool().intern(p, n).first;
    }

    numeric_range_type& get_numeric_range()
    {
        if (!m_data->numeric_range)
            m_data->numeric_range = numeric_range_type();

        return *m_data->numeric_range;
    }

public:
    import_pc_field_group(document& doc, pivot_cache_field_t& parent, size_t base_index) :
        m_doc(doc),
        m_parent_field(parent),
        m_data(orcus::make_unique<pivot_cache_group_data_t>(base_index)) {}

    virtual ~import_pc_field_group() override {}

    virtual void link_base_to_group_items(size_t group_item_index) override
    {
        pivot_cache_indices_t& b2g = m_data->base_to_group_indices;
        b2g.push_back(group_item_index);
    }

    virtual void set_field_item_string(const char* p, size_t n) override
    {
        m_current_field_item.type = pivot_cache_item_t::item_type::string;
        pstring s = intern(p, n);
        m_current_field_item.value.string.p = s.get();
        m_current_field_item.value.string.n = s.size();
    }

    virtual void set_field_item_numeric(double v) override
    {
        m_current_field_item.type = pivot_cache_item_t::item_type::numeric;
        m_current_field_item.value.numeric = v;
    }

    virtual void commit_field_item() override
    {
        m_data->items.push_back(std::move(m_current_field_item));
    }

    virtual void set_auto_start(bool b) override
    {
        get_numeric_range().auto_start = b;
    }

    virtual void set_auto_end(bool b) override
    {
        get_numeric_range().auto_end = b;
    }

    virtual void set_start_number(double v) override
    {
        get_numeric_range().start = v;
    }

    virtual void set_end_number(double v) override
    {
        get_numeric_range().end = v;
    }

    virtual void set_group_interval(double v) override
    {
        get_numeric_range().interval = v;
    }

    virtual void commit() override
    {
        m_parent_field.group_data = std::move(m_data);
    }
};

class import_pivot_cache_def : public iface::import_pivot_cache_definition
{
    enum source_type { unknown = 0, worksheet, external, consolidation, scenario };

    document& m_doc;

    pivot_cache_id_t m_cache_id = 0;

    source_type m_src_type = unknown;
    pstring m_src_sheet_name;
    ixion::abs_range_t m_src_range;

    std::unique_ptr<pivot_cache> m_cache;
    pivot_cache::fields_type m_current_fields;
    pivot_cache_field_t m_current_field;
    pivot_cache_item_t m_current_field_item;

    std::unique_ptr<import_pc_field_group> m_current_field_group;

private:
    pstring intern(const char* p, size_t n)
    {
        return m_doc.get_string_pool().intern(p, n).first;
    }

public:
    import_pivot_cache_def(document& doc) : m_doc(doc) {}

    void create_cache(pivot_cache_id_t cache_id)
    {
        m_src_type = unknown;

        m_cache_id = cache_id;
        m_cache = orcus::make_unique<pivot_cache>(m_doc.get_string_pool());
    }

    virtual void set_worksheet_source(
        const char* ref, size_t n_ref, const char* sheet_name, size_t n_sheet_name) override
    {
        assert(m_cache);
        assert(m_cache_id > 0);

        const ixion::formula_name_resolver* resolver = m_doc.get_formula_name_resolver();
        assert(resolver);

        m_src_type = worksheet;
        m_src_sheet_name = intern(sheet_name, n_sheet_name);

        ixion::formula_name_t fn = resolver->resolve(ref, n_ref, ixion::abs_address_t(0,0,0));

        if (fn.type != ixion::formula_name_t::range_reference)
        {
            std::ostringstream os;
            os << pstring(ref, n_ref) << " is not a valid range.";
            throw xml_structure_error(os.str());
        }

        m_src_range = ixion::to_range(fn.range).to_abs(ixion::abs_address_t(0,0,0));
    }

    virtual void set_field_count(size_t n) override
    {
        m_current_fields.reserve(n);
    }

    virtual void set_field_name(const char* p, size_t n) override
    {
        m_current_field.name = intern(p, n);
    }

    virtual iface::import_pivot_cache_field_group* create_field_group(size_t base_index) override
    {
        m_current_field_group =
            orcus::make_unique<import_pc_field_group>(m_doc, m_current_field, base_index);

        return m_current_field_group.get();
    }

    virtual void set_field_min_value(double v) override
    {
        m_current_field.min_value = v;
    }

    virtual void set_field_max_value(double v) override
    {
        m_current_field.max_value = v;
    }

    virtual void set_field_min_datetime(const date_time_t& dt) override
    {
        m_current_field.min_datetime = dt;
    }

    virtual void set_field_max_datetime(const date_time_t& dt) override
    {
        m_current_field.max_datetime = dt;
    }

    virtual void commit_field() override
    {
        m_current_fields.push_back(std::move(m_current_field));
    }

    virtual void set_field_item_string(const char* p, size_t n) override
    {
        m_current_field_item.type = pivot_cache_item_t::item_type::string;
        pstring s = intern(p, n);
        m_current_field_item.value.string.p = s.get();
        m_current_field_item.value.string.n = s.size();
    }

    virtual void set_field_item_numeric(double v) override
    {
        m_current_field_item.type = pivot_cache_item_t::item_type::numeric;
        m_current_field_item.value.numeric = v;
    }

    virtual void set_field_item_datetime(const date_time_t& dt) override
    {
        m_current_field_item.type = pivot_cache_item_t::item_type::datetime;
        m_current_field_item.value.datetime.year = dt.year;
        m_current_field_item.value.datetime.month = dt.month;
        m_current_field_item.value.datetime.day = dt.day;
        m_current_field_item.value.datetime.hour = dt.hour;
        m_current_field_item.value.datetime.minute = dt.minute;
        m_current_field_item.value.datetime.second = dt.second;
    }

    virtual void commit_field_item() override
    {
        m_current_field.items.push_back(std::move(m_current_field_item));
    }

    virtual void commit() override
    {
        m_cache->insert_fields(std::move(m_current_fields));
        assert(m_current_fields.empty());

        m_doc.get_pivot_collection().insert_worksheet_cache(
            m_src_sheet_name, m_src_range, std::move(m_cache));
    }
};

struct import_factory_impl
{
    document& m_doc;
    row_t m_default_row_size;
    col_t m_default_col_size;

    import_global_settings m_global_settings;
    import_pivot_cache_def m_pc_def;

    import_factory_impl(document& doc, row_t row_size, col_t col_size) :
        m_doc(doc),
        m_default_row_size(row_size),
        m_default_col_size(col_size),
        m_global_settings(doc),
        m_pc_def(doc) {}
};

import_factory::import_factory(document& doc, row_t row_size, col_t col_size) :
    mp_impl(new import_factory_impl(doc, row_size, col_size)) {}

import_factory::~import_factory()
{
    delete mp_impl;
}

iface::import_global_settings* import_factory::get_global_settings()
{
    return &mp_impl->m_global_settings;
}

iface::import_shared_strings* import_factory::get_shared_strings()
{
    return mp_impl->m_doc.get_shared_strings();
}

iface::import_styles* import_factory::get_styles()
{
    return mp_impl->m_doc.get_styles();
}

iface::import_pivot_cache_definition* import_factory::create_pivot_cache_definition(
    pivot_cache_id_t cache_id)
{
    mp_impl->m_pc_def.create_cache(cache_id);
    return &mp_impl->m_pc_def;
}

iface::import_sheet* import_factory::append_sheet(const char* sheet_name, size_t sheet_name_length)
{
    return mp_impl->m_doc.append_sheet(
        pstring(sheet_name, sheet_name_length), mp_impl->m_default_row_size, mp_impl->m_default_col_size);
}

iface::import_sheet* import_factory::get_sheet(const char* sheet_name, size_t sheet_name_length)
{
    return mp_impl->m_doc.get_sheet(pstring(sheet_name, sheet_name_length));
}

iface::import_sheet* import_factory::get_sheet(sheet_t sheet_index)
{
    return mp_impl->m_doc.get_sheet(sheet_index);
}

void import_factory::finalize()
{
    mp_impl->m_doc.finalize();
}

struct export_factory_impl
{
    document& m_doc;

    export_factory_impl(document& doc) : m_doc(doc) {}
};

export_factory::export_factory(document& doc) :
    mp_impl(new export_factory_impl(doc)) {}

export_factory::~export_factory()
{
    delete mp_impl;
}

const iface::export_sheet* export_factory::get_sheet(const char* sheet_name, size_t sheet_name_length) const
{
    return mp_impl->m_doc.get_sheet(pstring(sheet_name, sheet_name_length));
}

}}
/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
