/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_ORCUS_XML_NAMESPACE_MANAGER_HPP
#define INCLUDED_ORCUS_XML_NAMESPACE_MANAGER_HPP

#include "types.hpp"

#include <ostream>
#include <memory>

namespace orcus {

class xmlns_context;
class pstring;
struct xmlns_repository_impl;
struct xmlns_context_impl;

/**
 * Central XML namespace repository that stores all namespaces that are used
 * in the current session.
 */
class ORCUS_PSR_DLLPUBLIC xmlns_repository
{
    friend class xmlns_context;

    struct impl;
    std::unique_ptr<impl> mp_impl;

    xmlns_id_t intern(const pstring& uri);

    xmlns_repository(const xmlns_repository&); // disabled
    xmlns_repository& operator= (const xmlns_repository&); // disabled

    size_t get_index(xmlns_id_t ns_id) const;

public:
    xmlns_repository();
    ~xmlns_repository();

    /**
     * Add a set of predefined namespace values to the repository.
     *
     * @param predefined_ns predefined set of namespace values. This is a
     *                      null-terminated array of xmlns_id_t.  This
     *                      xmlns_repository instance will assume that the
     *                      instances of these xmlns_id_t values will be
     *                      available throughout its life cycle; caller needs
     *                      to ensure that they won't get deleted before the
     *                      corresponding xmlns_repository instance is
     *                      deleted.
     */
    void add_predefined_values(const xmlns_id_t* predefined_ns);

    xmlns_context create_context();

    /**
     * Get XML namespace identifier from its numerical index.
     *
     * @param index numeric index of namespace.
     *
     * @return valid namespace identifier, or XMLNS_UNKNOWN_ID if not found.
     */
    xmlns_id_t get_identifier(size_t index) const;

    std::string get_short_name(xmlns_id_t ns_id) const;
    std::string get_short_name(size_t index) const;
};

/**
 * XML namespace context.  A new context should be used for each xml stream
 * since the namespace keys themselves are not interned.  Don't hold an
 * instance of this class any longer than the life cycle of the xml stream
 * it is used in.
 *
 * An empty key value is associated with a default namespace.
 */
class ORCUS_PSR_DLLPUBLIC xmlns_context
{
    friend class xmlns_repository;

    struct impl;
    std::unique_ptr<impl> mp_impl;

    xmlns_context(); // disabled
    xmlns_context(xmlns_repository& repo);
public:
    xmlns_context(const xmlns_context& r);
    ~xmlns_context();

    xmlns_id_t push(const pstring& key, const pstring& uri);
    void pop(const pstring& key);

    /**
     * Get the currnet namespace identifier for a specified namespace alias.
     *
     * @param key namespace alias to get the current namespace identifier for.
     *
     * @return current namespace identifier associated with the alias.
     */
    xmlns_id_t get(const pstring& key) const;

    /**
     * Get a unique index value associated with a specified identifier.  An
     * index value is guaranteed to be unique regardless of contexts.
     *
     * @param ns_id a namespace identifier to obtain index for.
     *
     * @return index value associated with the identifier.
     */
    size_t get_index(xmlns_id_t ns_id) const;

    /**
     * Get a 'short' name associated with a specified identifier.  A short
     * name is a string value conveniently short enough for display purposes,
     * but still guaranteed to be unique to the identifier it is associated
     * with.
     *
     * <p>Note that the xmlns_repository class has method of the same
     * name, and that method works identically to this method.</p>
     *
     * @param ns_id a namespace identifier to obtain short name for.
     *
     * @return short name for the specified identifier.
     */
    std::string get_short_name(xmlns_id_t ns_id) const;

    /**
     * Get an alias currently associated with a given namespace identifier.
     *
     * @param ns_id namespace identifier.
     *
     * @return alias name currently associted with the given namespace
     *         identifier, or an empty string if the given namespace is
     *         currently not associated with any aliases.
     */
    pstring get_alias(xmlns_id_t ns_id) const;

    std::vector<xmlns_id_t> get_all_namespaces() const;

    void dump(std::ostream& os) const;
};

}

#endif
/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
