/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_ORCUS_SPREADSHEET_PIVOT_HPP
#define INCLUDED_ORCUS_SPREADSHEET_PIVOT_HPP

#include "orcus/env.hpp"
#include "orcus/pstring.hpp"
#include "orcus/types.hpp"
#include "orcus/spreadsheet/types.hpp"

#include <memory>
#include <vector>
#include <limits>

#include <boost/optional.hpp>

namespace ixion {

struct abs_range_t;

}

namespace orcus {

class string_pool;

namespace spreadsheet {

class document;

using pivot_cache_indices_t = std::vector<size_t>;

struct ORCUS_SPM_DLLPUBLIC pivot_cache_record_value_t
{
    enum class value_type
    {
        unknown = 0,
        boolean,
        date_time,
        character,
        numeric,
        blank,
        error,
        shared_item_index
    };

    value_type type;

    union
    {
        bool boolean;

        struct
        {
            // This must point to an interned string instance. May not be
            // null-terminated.
            const char* p;

            size_t n; // Length of the string value.

        } character;

        struct
        {
            int year;
            int month;
            int day;
            int hour;
            int minute;
            double second;

        } date_time;

        double numeric;

        size_t shared_item_index;

        // TODO : add error value.

    } value;

    pivot_cache_record_value_t();
};

using pivot_cache_record_t = std::vector<pivot_cache_record_value_t>;

struct ORCUS_SPM_DLLPUBLIC pivot_cache_item_t
{
    enum class item_type
    {
        unknown = 0, boolean, datetime, string, numeric, blank, error
    };

    item_type type;

    union
    {
        struct
        {
            // This must point to an interned string instance. May not be
            // null-terminated.
            const char* p;

            size_t n; // Length of the string value.

        } string;

        struct
        {
            int year;
            int month;
            int day;
            int hour;
            int minute;
            double second;

        } datetime;

        double numeric;
        bool boolean;

        // TODO : add error value.

    } value;

    pivot_cache_item_t();
    pivot_cache_item_t(const char* p_str, size_t n_str);
    pivot_cache_item_t(double _numeric);
    pivot_cache_item_t(bool _boolean);
    pivot_cache_item_t(const date_time_t& _datetime);

    pivot_cache_item_t(const pivot_cache_item_t& other);
    pivot_cache_item_t(pivot_cache_item_t&& other);

    bool operator< (const pivot_cache_item_t& other) const;
    bool operator== (const pivot_cache_item_t& other) const;

    pivot_cache_item_t& operator= (pivot_cache_item_t other);

    void swap(pivot_cache_item_t& other);
};

using pivot_cache_items_t = std::vector<pivot_cache_item_t>;

/**
 * Group data for a pivot cache field.
 */
struct ORCUS_SPM_DLLPUBLIC pivot_cache_group_data_t
{
    struct ORCUS_SPM_DLLPUBLIC range_grouping_type
    {
        pivot_cache_group_by_t group_by = pivot_cache_group_by_t::range;

        bool auto_start = true;
        bool auto_end   = true;

        double start    = 0.0;
        double end      = 0.0;
        double interval = 1.0;

        date_time_t start_date;
        date_time_t end_date;

        range_grouping_type() = default;
        range_grouping_type(const range_grouping_type& other) = default;
    };


    /**
     * Mapping of base field member indices to the group field item indices.
     */
    pivot_cache_indices_t base_to_group_indices;

    boost::optional<range_grouping_type> range_grouping;

    /**
     * Individual items comprising the group.
     */
    pivot_cache_items_t items;

    /** 0-based index of the base field. */
    size_t base_field;

    pivot_cache_group_data_t(size_t _base_field);
    pivot_cache_group_data_t(const pivot_cache_group_data_t& other);
    pivot_cache_group_data_t(pivot_cache_group_data_t&& other);

    pivot_cache_group_data_t() = delete;
};

struct ORCUS_SPM_DLLPUBLIC pivot_cache_field_t
{
    /**
     * Field name. It must be interned with the string pool belonging to the
     * document.
     */
    pstring name;

    pivot_cache_items_t items;

    boost::optional<double> min_value;
    boost::optional<double> max_value;

    boost::optional<date_time_t> min_date;
    boost::optional<date_time_t> max_date;

    std::unique_ptr<pivot_cache_group_data_t> group_data;

    pivot_cache_field_t();
    pivot_cache_field_t(const pstring& _name);
    pivot_cache_field_t(const pivot_cache_field_t& other);
    pivot_cache_field_t(pivot_cache_field_t&& other);
};

class ORCUS_SPM_DLLPUBLIC pivot_cache
{
    struct impl;
    std::unique_ptr<impl> mp_impl;

public:
    using fields_type = std::vector<pivot_cache_field_t>;

    pivot_cache(string_pool& sp);
    ~pivot_cache();

    /**
     * Bulk-insert all the fields in one step. Note that this will replace any
     * pre-existing fields if any.
     *
     * @param fields field instances to move into storage.
     */
    void insert_fields(fields_type fields);

    size_t get_field_count() const;

    /**
     * Retrieve a field data by its index.
     *
     * @param index index of the field to retrieve.
     *
     * @return pointer to the field instance, or nullptr if the index is
     *         out-of-range.
     */
    const pivot_cache_field_t* get_field(size_t index) const;
};

class ORCUS_SPM_DLLPUBLIC pivot_collection
{
    struct impl;
    std::unique_ptr<impl> mp_impl;

public:
    pivot_collection(document& doc);
    ~pivot_collection();

    /**
     * Insert a new pivot cache associated with a worksheet source.
     *
     * @param sheet_name name of the sheet where the source data is.
     * @param range range of the source data.  Note that the sheet indices are
     *              not used.
     */
    void insert_worksheet_cache(
        const pstring& sheet_name, const ixion::abs_range_t& range, std::unique_ptr<pivot_cache>&& cache);

    /**
     * Count the number of pivot caches currently stored.
     *
     * @return number of pivot caches currently stored in the document.
     */
    size_t get_cache_count() const;

    const pivot_cache* get_cache(
        const pstring& sheet_name, const ixion::abs_range_t& range) const;
};

}}

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
