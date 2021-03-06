Orcus - library for processing spreadsheet documents.
=====================================================

Orcus is a library that provides a collection of standalone file processing
filters.  It is currently focused on providing filters for spreadsheet
documents, but filters for other productivity application types (such as
wordprocessor and presentation) are in consideration.

The library currently includes the following import filters:

* Microsoft Excel 2007 XML
* Microsoft Excel 2003 XML
* Open Document Spreadsheet
* Plain Text
* Gnumeric XML
* Generic XML

The library also includes low-level parsers for the following:

* CSV
* CSS
* XML
* JSON
* YAML

These parsers are all implemented as C++ templates and require a handler class
passed as a template argument so that the handler class receives various
callbacks from the parser as the file is being parsed.

## API Documentation

* [Official API documentation](https://orcus.readthedocs.io/en/latest/) for
  general users of the library.

## Pages

* [Old packages](OLD-DOWNLOADS.md)
* [For contributors](CONTRIBUTING.md)
