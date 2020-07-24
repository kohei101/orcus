/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_ORCUS_XML_STRUCTURE_TREE_HPP
#define INCLUDED_ORCUS_XML_STRUCTURE_TREE_HPP

#include "env.hpp"
#include "types.hpp"

#include <ostream>
#include <memory>
#include <functional>

namespace orcus {

class xmlns_context;

struct ORCUS_DLLPUBLIC xml_table_range_t
{
    std::vector<std::string> paths;
    std::vector<std::string> row_groups;

    xml_table_range_t();
    ~xml_table_range_t();
};

/**
 * Tree representing the structure of elements in XML content.  Recurring
 * elements under the same parent are represented by a single element
 * instance.  This tree only includes elements; no attributes and content
 * nodes appear in this tree.
 */
class ORCUS_DLLPUBLIC xml_structure_tree
{
    struct impl;
    std::unique_ptr<impl> mp_impl;

public:
    xml_structure_tree() = delete;
    xml_structure_tree(const xml_structure_tree&) = delete;
    xml_structure_tree& operator= (const xml_structure_tree&) = delete;

    struct ORCUS_DLLPUBLIC entity_name
    {
        xmlns_id_t ns;
        pstring name;

        entity_name();
        entity_name(xmlns_id_t _ns, const pstring& _name);

        bool operator< (const entity_name& r) const;
        bool operator== (const entity_name& r) const;

        struct ORCUS_DLLPUBLIC hash
        {
            size_t operator ()(const entity_name& val) const;
        };
    };

    typedef std::vector<entity_name> entity_names_type;

    struct ORCUS_DLLPUBLIC element
    {
        entity_name name;
        bool repeat;
        bool has_content;

        element();
        element(const entity_name& _name, bool _repeat, bool _has_content);
    };

    struct walker_impl;

    /**
     * This class allows client to traverse the tree.
     */
    class ORCUS_DLLPUBLIC walker
    {
        friend class xml_structure_tree;

        std::unique_ptr<walker_impl> mp_impl;

        walker(const xml_structure_tree::impl& parent_impl);
    public:
        walker() = delete;
        walker(const walker& r);
        ~walker();
        walker& operator= (const walker& r);

        /**
         * Set current position to the root element, and return the root
         * element.
         *
         * @return root element.
         */
        element root();

        /**
         * Descend into specified child element.
         *
         * @param ns namespace of child element
         * @param name name of child element
         *
         * @return child element
         */
        element descend(const entity_name& name);

        /**
         * Move up to the parent element.
         */
        element ascend();

        /**
         * Move to the element specified by a path expression. The path
         * expression may be generated by
         * <code>xml_structure_tree::walker::get_path</code>.
         *
         * @param path a simple XPath like expression
         *
         * @return element pointed to by the path.
         */
        element move_to(const std::string& path);

        /**
         * Get a list of names of all child elements at the current element
         * position.  The list of names is in order of appearance.
         *
         * @return list of child element names in order of appearance.
         */
        entity_names_type get_children();

        /**
         * Get a list of names of all attributes that belong to current
         * element.  The list of names is in order of appearance.
         *
         * @return list of attribute names in order of appearance.
         */
        entity_names_type get_attributes();

        /**
         * Get a numerical, 0-based index of given XML namespace.
         *
         * @param ns XML namespace ID.
         *
         * @return numeric, 0-based index of XML namespace if found, or
         *         <code>xml_structure_tree::walker::index_not_found</code> if
         *         the namespace is not found in this structure.
         */
        size_t get_xmlns_index(xmlns_id_t ns) const;

        std::string get_xmlns_short_name(xmlns_id_t ns) const;

        /**
         * Convert an entity name to its proper string representation.
         *
         * @param name entity name to convert to string.
         *
         * @return string representation of the entity name, including the
         *         namespace.
         */
        std::string to_string(const entity_name& name) const;

        /**
         * Get a XPath like ID for the element inside of the XML tree.
         *
         */
        std::string get_path() const;
    };

    xml_structure_tree(xmlns_context& xmlns_cxt);
    xml_structure_tree(xml_structure_tree&& other);
    ~xml_structure_tree();

    void parse(const char* p, size_t n);

    void dump_compact(std::ostream& os) const;

    walker get_walker() const;

    using range_handler_type = std::function<void(xml_table_range_t&&)>;

    void process_ranges(range_handler_type rh) const;

    void swap(xml_structure_tree& other);
};

}



#endif
/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
