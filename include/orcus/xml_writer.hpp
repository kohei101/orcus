/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_ORCUS_XML_WRITER_HPP
#define INCLUDED_ORCUS_XML_WRITER_HPP

#include "orcus/types.hpp"

#include <memory>

namespace orcus {

class ORCUS_PSR_DLLPUBLIC xml_writer
{
    struct impl;
    std::unique_ptr<impl> mp_impl;

public:
    xml_writer(std::ostream& os);
    ~xml_writer();

    void push_element(const xml_name_t& name);

    void add_namespace(const pstring& alias, xmlns_id_t ns);

    void add_attribute(const xml_name_t& name, const pstring& value);

    void add_content(const pstring& content);

    xml_name_t pop_element();
};

}

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
