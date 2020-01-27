/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_NAMED_EXPRESSION_HPP
#define INCLUDED_NAMED_EXPRESSION_HPP

#include <Python.h>
#include <ixion/formula_tokens_fwd.hpp>

namespace orcus {

namespace spreadsheet {

class document;

}

namespace python {

/** non-python part. */
struct named_exp_data
{
    named_exp_data();
    ~named_exp_data();
};

PyObject* create_named_exp_object(
    const spreadsheet::document& doc, const ixion::formula_tokens_t* tokens);

PyTypeObject* get_named_exp_type();

}}

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */