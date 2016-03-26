/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "sheet_rows.hpp"
#include "orcus/spreadsheet/sheet.hpp"

namespace orcus { namespace python {

sheet_rows_data::sheet_rows_data() :
    m_range(ixion::abs_range_t::invalid),
    m_current_row(-1) {}

sheet_rows_data::~sheet_rows_data() {}

namespace {

/**
 * Python object for orcus.SheetRows.
 */
struct sheet_rows
{
    PyObject_HEAD

    sheet_rows_data* m_data;
};

void sheet_rows_dealloc(sheet_rows* self)
{
    delete self->m_data;

    Py_TYPE(self)->tp_free(reinterpret_cast<PyObject*>(self));
}

PyObject* sheet_rows_new(PyTypeObject* type, PyObject* /*args*/, PyObject* /*kwargs*/)
{
    sheet_rows* self = (sheet_rows*)type->tp_alloc(type, 0);
    self->m_data = new sheet_rows_data;
    return reinterpret_cast<PyObject*>(self);
}

int sheet_rows_init(sheet_rows* self, PyObject* /*args*/, PyObject* /*kwargs*/)
{
    return 0;
}

PyObject* sheet_rows_iter(PyObject* self)
{
    sheet_rows_data* data = reinterpret_cast<sheet_rows*>(self)->m_data;
    data->m_current_row = 0;

    Py_INCREF(self);
    return self;
}

PyObject* sheet_rows_iternext(PyObject* self)
{
    sheet_rows_data* data = reinterpret_cast<sheet_rows*>(self)->m_data;

    if (data->m_current_row >= data->m_range.last.row)
    {
        // No more elements.  Stop the iteration.
        PyErr_SetNone(PyExc_StopIteration);
        return nullptr;
    }

    ++data->m_current_row;

    PyObject* row = PyTuple_New(data->m_range.last.column);
    for (ixion::col_t i = 0; i < data->m_range.last.column; ++i)
    {
        // TODO : Store the real cell value.
        PyTuple_SetItem(row, i, PyLong_FromLong(i));
    }

    return row;
}

PyTypeObject sheet_rows_type =
{
    PyVarObject_HEAD_INIT(nullptr, 0)
    "orcus.SheetRows",                        // tp_name
    sizeof(sheet_rows),                       // tp_basicsize
    0,                                        // tp_itemsize
    (destructor)sheet_rows_dealloc,           // tp_dealloc
    0,                                        // tp_print
    0,                                        // tp_getattr
    0,                                        // tp_setattr
    0,                                        // tp_compare
    0,                                        // tp_repr
    0,                                        // tp_as_number
    0,                                        // tp_as_sequence
    0,                                        // tp_as_mapping
    0,                                        // tp_hash
    0,                                        // tp_call
    0,                                        // tp_str
    0,                                        // tp_getattro
    0,                                        // tp_setattro
    0,                                        // tp_as_buffer
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, // tp_flags
    "orcus sheet rows iterator",              // tp_doc
    0,		                                  // tp_traverse
    0,		                                  // tp_clear
    0,		                                  // tp_richcompare
    0,		                                  // tp_weaklistoffset
    (getiterfunc)sheet_rows_iter,		      // tp_iter
    (iternextfunc)sheet_rows_iternext,        // tp_iternext
    0,                                        // tp_methods
    0,                                        // tp_members
    0,                                        // tp_getset
    0,                                        // tp_base
    0,                                        // tp_dict
    0,                                        // tp_descr_get
    0,                                        // tp_descr_set
    0,                                        // tp_dictoffset
    (initproc)sheet_rows_init,                // tp_init
    0,                                        // tp_alloc
    sheet_rows_new,                           // tp_new
};

}

PyTypeObject* get_sheet_rows_type()
{
    return &sheet_rows_type;
}

void store_sheet_rows_data(PyObject* self, const spreadsheet::sheet* orcus_sheet)
{
    sheet_rows_data* data = reinterpret_cast<sheet_rows*>(self)->m_data;
    data->m_sheet = orcus_sheet;
    data->m_range = orcus_sheet->get_data_range();
}

}}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
