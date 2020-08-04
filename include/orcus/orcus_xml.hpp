/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_ORCUS_ORCUS_XML_HPP
#define INCLUDED_ORCUS_ORCUS_XML_HPP

#include "env.hpp"
#include "spreadsheet/types.hpp"

#include <ostream>
#include <memory>

namespace orcus {

class pstring;
class xmlns_repository;

namespace spreadsheet { namespace iface {
    class import_factory;
    class export_factory;
}}

class ORCUS_DLLPUBLIC orcus_xml
{
    struct impl;
    std::unique_ptr<impl> mp_impl;

    void read_impl(const pstring& strm);

public:
    orcus_xml(const orcus_xml&) = delete;
    orcus_xml& operator= (const orcus_xml&) = delete;

    orcus_xml(xmlns_repository& ns_repo, spreadsheet::iface::import_factory* im_fact, spreadsheet::iface::export_factory* ex_fact);
    ~orcus_xml();

    /**
     * Define a namespace and its alias used in a map file.
     *
     * @param alias alias for the namespace.
     * @param uri namespace value.
     * @param default_ns whether or not to use this namespace as the default
     *                   namespace.  When this value is set to true, the
     *                   namespace being set will be applied for all elements
     *                   and attributes used in the paths without explicit
     *                   namespace values.
     */
    void set_namespace_alias(const pstring& alias, const pstring& uri, bool default_ns=false);

    /**
     * Define a mapping of a single element or attribute to a single cell
     * location.
     *
     * @param xpath path to the element or attribute to link.
     * @param sheet sheet index (0-based) of the linked cell location.
     * @param row row index (0-based) of the linked cell location.
     * @param col column index (0-based) of the linked cell location.
     */
    void set_cell_link(const pstring& xpath, const pstring& sheet, spreadsheet::row_t row, spreadsheet::col_t col);

    /**
     * Initiate the mapping definition of a linked range.  The definition will
     * get committed when the {@link commit_range} method is called.
     *
     * @param sheet sheet index (0-based) of the linked cell location.
     * @param row row index (0-based) of the linked cell location.
     * @param col column index (0-based) of the linked cell location.
     */
    void start_range(const pstring& sheet, spreadsheet::row_t row, spreadsheet::col_t col);

    /**
     * Append a field that is mapped to a specified path in the XML document
     * to the current linked range.
     *
     * @param xpath path to the element or attribute to link as a field.
     */
    void append_field_link(const pstring& xpath);

    void append_field_link(const pstring& xpath, const pstring& label);

    /**
     * Set the element located in the specified path as a row group in the
     * current linked range.
     *
     * If the element is defined as a row-group element, the row index will
     * increment whenever that element closes.
     *
     * @param xpath path to the element to use as a row group element.
     */
    void set_range_row_group(const pstring& xpath);

    /**
     * Commit the mapping definition of the current range.
     */
    void commit_range();

    /**
     * Append a new sheet to the spreadsheet document.
     *
     * @param name name of the sheet.
     */
    void append_sheet(const pstring& name);

    /**
     * Read the stream containing the source XML document.
     *
     * @param p pointer to the buffer containing the source XML document.
     * @param n size of the buffer.
     */
    void read_stream(const char* p, size_t n);

    /**
     * Read an XML stream that contains an entire set of mapping rules.
     *
     * This method also inserts all necessary sheets into the document model.
     *
     * @param p pointer to the buffer that contains the XML string.
     * @param n size of the buffer.
     */
    void read_map_definition(const char* p, size_t n);

    /**
     * Read a stream containing the source XML document, automatically detect
     * all linkable ranges and import them one range per sheet.
     *
     * @param p pointer to the buffer that contains the source XML document.
     * @param n size of the buffer.
     */
    void detect_map_definition(const char* p, size_t n);

    /**
     * Read a stream containing the source XML document, automatically detect
     * all linkable ranges, and write a map definition file depicting the
     * detected ranges.
     *
     * @param p pointer to the buffer that contains the source XML document.
     * @param n size of the buffer.
     * @param out output stream to write the map definition file to.
     */
    void write_map_definition(const char* p, size_t n, std::ostream& out) const;

    /**
     * Write the linked cells and ranges in the spreadsheet document as an XML
     * document using the same map definition rules used to load the content.
     *
     * Note that this requires the source XML document stream, as it re-uses
     * parts of the source stream.
     *
     * @param p_in pointer to the buffer that contains the source XML
     *             document.
     * @param n_in size of the buffer containing the source XML document.
     * @param out output stream to write the XML document to.
     */
    void write(const char* p_in, size_t n_in, std::ostream& out) const;
};

}

#endif
/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
