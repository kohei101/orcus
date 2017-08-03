/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "orcus/orcus_xls_xml.hpp"
#include "orcus/stream.hpp"
#include "orcus/xml_namespace.hpp"
#include "orcus/config.hpp"
#include "orcus/spreadsheet/import_interface.hpp"
#include "orcus/parser_base.hpp"

#include "xml_stream_parser.hpp"
#include "xls_xml_handler.hpp"
#include "xls_xml_detection_handler.hpp"
#include "session_context.hpp"
#include "xls_xml_tokens.hpp"
#include "xls_xml_namespace_types.hpp"
#include "detection_result.hpp"

#include <iostream>

using namespace std;

namespace orcus {

struct orcus_xls_xml_impl
{
    xmlns_repository m_ns_repo;
    session_context m_cxt;
    spreadsheet::iface::import_factory* mp_factory;

    orcus_xls_xml_impl(spreadsheet::iface::import_factory* factory) : mp_factory(factory) {}
};

orcus_xls_xml::orcus_xls_xml(spreadsheet::iface::import_factory* factory) :
    iface::import_filter(format_t::xls_xml),
    mp_impl(new orcus_xls_xml_impl(factory))
{
    mp_impl->m_ns_repo.add_predefined_values(NS_xls_xml_all);
}

orcus_xls_xml::~orcus_xls_xml()
{
    delete mp_impl;
}

bool orcus_xls_xml::detect(const unsigned char* buffer, size_t size)
{
    config opt(format_t::xls_xml);
    xmlns_repository ns_repo;
    ns_repo.add_predefined_values(NS_xls_xml_all);
    xml_stream_parser parser(opt, ns_repo, xls_xml_tokens, reinterpret_cast<const char*>(buffer), size);

    session_context cxt;
    xls_xml_detection_handler handler(cxt, xls_xml_tokens);
    parser.set_handler(&handler);
    try
    {
        parser.parse();
    }
    catch (const detection_result& res)
    {
        return res.get_result();
    }
    catch (...) {}

    return false;
}

void orcus_xls_xml::read_file(const string& filepath)
{
    string strm = load_file_content(filepath.c_str());
    if (strm.empty())
        return;

    read_stream(&strm[0], strm.size());
}

void orcus_xls_xml::read_stream(const char* content, size_t len)
{
    if (!content || !len)
        return;

    spreadsheet::iface::import_global_settings* gs =
        mp_impl->mp_factory->get_global_settings();

    if (!gs)
        return;

    gs->set_origin_date(1899, 12, 30);
    gs->set_default_formula_grammar(spreadsheet::formula_grammar_t::xls_xml);

    xml_stream_parser parser(
        get_config(), mp_impl->m_ns_repo, xls_xml_tokens, content, len);

    auto handler = orcus::make_unique<xls_xml_handler>(
        mp_impl->m_cxt, xls_xml_tokens, mp_impl->mp_factory);

    parser.set_handler(handler.get());
    try
    {
        parser.parse();
    }
    catch (const parse_error& e)
    {
        std::cerr << create_parse_error_output(pstring(content, len), e.offset()) << std::endl;
        std::cerr << e.what() << std::endl;
        return;
    }

    mp_impl->mp_factory->finalize();
}

const char* orcus_xls_xml::get_name() const
{
    static const char* name = "xls-xml";
    return name;
}

}
/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
