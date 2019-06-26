/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "orcus/json_document_tree.hpp"
#include "orcus/json_parser_base.hpp"
#include "orcus/json_structure_tree.hpp"
#include "orcus/config.hpp"
#include "orcus/stream.hpp"
#include "orcus/xml_namespace.hpp"
#include "orcus/dom_tree.hpp"
#include "orcus/global.hpp"

#include <iostream>
#include <fstream>
#include <string>
#include <memory>

#include <mdds/sorted_string_map.hpp>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

using namespace std;
using namespace orcus;
namespace po = boost::program_options;
namespace fs = boost::filesystem;

namespace {

namespace output_format {

typedef mdds::sorted_string_map<json_config::output_format_type> map_type;

// Keys must be sorted.
const std::vector<map_type::entry> entries =
{
    { ORCUS_ASCII("check"), json_config::output_format_type::check },
    { ORCUS_ASCII("json"),  json_config::output_format_type::json  },
    { ORCUS_ASCII("none"),  json_config::output_format_type::none  },
    { ORCUS_ASCII("xml"),   json_config::output_format_type::xml   },
};

const map_type& get()
{
    static map_type mt(entries.data(), entries.size(), json_config::output_format_type::unknown);
    return mt;
}

} // namespace output_format

namespace mode {

enum class type {
    unknown,
    convert,
    map,
    structure
};

typedef mdds::sorted_string_map<type> map_type;

// Keys must be sorted.
const std::vector<map_type::entry> entries =
{
    { ORCUS_ASCII("convert"),   type::convert   },
    { ORCUS_ASCII("map"),       type::map       },
    { ORCUS_ASCII("structure"), type::structure },
};

const map_type& get()
{
    static map_type mt(entries.data(), entries.size(), type::unknown);
    return mt;
}

} // namespace mode

const char* help_program =
"The FILE must specify the path to an existing file.";

const char* help_json_output =
"Output file path.";

const char* help_json_output_format =
"Specify the format of output file.  Supported format types are:\n"
"  * XML (xml)\n"
"  * JSON (json)\n"
"  * flat tree dump (check)\n"
"  * no output (none)";

const char* err_no_input_file = "No input file.";

void print_json_usage(std::ostream& os, const po::options_description& desc)
{
    os << "Usage: orcus-json [options] FILE" << endl << endl;
    os << help_program << endl << endl << desc;
}

std::string build_mode_help_text()
{
    std::ostringstream os;
    os << "Mode of operation. Select one of the following options: ";
    auto it = mode::entries.cbegin(), ite = mode::entries.cend();
    --ite;

    for (; it != ite; ++it)
        os << std::string(it->key, it->keylen) << ", ";

    os << "or " << std::string(it->key, it->keylen) << ".";
    return os.str();
}

struct cmd_params
{
    std::unique_ptr<json_config> config;
    mode::type mode = mode::type::convert;
};

void parse_args_for_convert(
    cmd_params& params, const po::options_description& desc, const po::variables_map& vm)
{
    if (vm.count("resolve-refs"))
        params.config->resolve_references = true;

    if (vm.count("output-format"))
    {
        std::string s = vm["output-format"].as<string>();
        params.config->output_format = output_format::get().find(s.data(), s.size());

        if (params.config->output_format == json_config::output_format_type::unknown)
        {
            cerr << "Unknown output format type '" << s << "'." << endl;
            params.config.reset();
            return;
        }
    }
    else
    {
        cerr << "Output format is not specified." << endl;
        print_json_usage(cerr, desc);
        params.config.reset();
        return;
    }

    if (params.config->output_format != json_config::output_format_type::none)
    {
        // Check to make sure the output path doesn't point to an existing
        // directory.
        if (fs::is_directory(params.config->output_path))
        {
            cerr << "Output file path points to an existing directory.  Aborting." << endl;
            params.config.reset();
            return;
        }
    }
}

/**
 * Parse the command-line options, populate the json_config object, and
 * return that to the caller.
 */
cmd_params parse_json_args(int argc, char** argv)
{
    cmd_params params;

    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "Print this help.")
        ("mode", po::value<std::string>(), build_mode_help_text().data())
        ("resolve-refs", "Resolve JSON references to external files.")
        ("output,o", po::value<string>(), help_json_output)
        ("output-format,f", po::value<string>(), help_json_output_format);

    po::options_description hidden("Hidden options");
    hidden.add_options()
        ("input", po::value<string>(), "input file");

    po::options_description cmd_opt;
    cmd_opt.add(desc).add(hidden);

    po::positional_options_description po_desc;
    po_desc.add("input", -1);

    po::variables_map vm;
    try
    {
        po::store(
            po::command_line_parser(argc, argv).options(cmd_opt).positional(po_desc).run(), vm);
        po::notify(vm);
    }
    catch (const exception& e)
    {
        // Unknown options.
        cerr << e.what() << endl;
        print_json_usage(cerr, desc);
        return params;
    }

    if (vm.count("help"))
    {
        print_json_usage(cout, desc);
        return params;
    }

    if (vm.count("mode"))
    {
        std::string s = vm["mode"].as<std::string>();
        params.mode = mode::get().find(s.data(), s.size());
        if (params.mode == mode::type::unknown)
        {
            cerr << "Unknown mode string '" << s << "'." << endl;
            return params;
        }
    }

    params.config = orcus::make_unique<json_config>();

    if (vm.count("input"))
        params.config->input_path = vm["input"].as<string>();

    if (params.config->input_path.empty())
    {
        // No input file is given.
        cerr << err_no_input_file << endl;
        print_json_usage(cerr, desc);
        params.config.reset();
        return params;
    }

    if (!fs::exists(params.config->input_path))
    {
        cerr << "Input file does not exist: " << params.config->input_path << endl;
        params.config.reset();
        return params;
    }

    if (vm.count("output"))
        params.config->output_path = vm["output"].as<string>();

    switch (params.mode)
    {
        case mode::type::structure:
            break;
        case mode::type::convert:
            parse_args_for_convert(params, desc, vm);
            break;
        case mode::type::map:
            cerr << "TODO: parse args for map mode." << endl;
            break;
        default:
            assert(!"This should not happen since the mode check is done way earlier.");
    }

    return params;
}

std::unique_ptr<json::document_tree> load_doc(const orcus::file_content& content, const json_config& config)
{
    std::unique_ptr<json::document_tree> doc(orcus::make_unique<json::document_tree>());
    doc->load(content.data(), content.size(), config);
    return doc;
}

void build_doc_and_dump(std::ostream& os, const orcus::file_content& content, const cmd_params& params)
{
    std::unique_ptr<json::document_tree> doc = load_doc(content, *params.config);

    switch (params.config->output_format)
    {
        case json_config::output_format_type::xml:
        {
            os << doc->dump_xml();
            break;
        }
        case json_config::output_format_type::json:
        {
            os << doc->dump();
            break;
        }
        case json_config::output_format_type::check:
        {
            string xml_strm = doc->dump_xml();
            xmlns_repository repo;
            xmlns_context ns_cxt = repo.create_context();
            dom::document_tree dom(ns_cxt);
            dom.load(xml_strm);

            dom.dump_compact(os);
            break;
        }
        default:
            ;
    }
}

} // anonymous namespace

int main(int argc, char** argv)
{
    file_content content;

    try
    {
        cmd_params params = parse_json_args(argc, argv);

        if (!params.config || params.mode == mode::type::unknown)
            return EXIT_FAILURE;

        assert(!params.config->input_path.empty());
        content.load(params.config->input_path.data());

        std::ostream* os = &cout;
        std::unique_ptr<std::ofstream> fs;

        if (!params.config->output_path.empty())
        {
            // Output to stdout when output path is not given.
            fs = std::make_unique<std::ofstream>(params.config->output_path.data());
            os = fs.get();
        }

        switch (params.mode)
        {
            case mode::type::structure:
            {
                json::structure_tree tree;
                tree.parse(content.data(), content.size());
                tree.dump_compact(*os);
                break;
            }
            case mode::type::map:
                cout << "TODO: implement this." << endl;
                break;
            case mode::type::convert:
            {
                build_doc_and_dump(*os, content, params);
                break;
            }
            default:
                cerr << "Unkonwn mode has been given." << endl;
                return EXIT_FAILURE;
        }
    }
    catch (const json::parse_error& e)
    {
        cerr << create_parse_error_output(content.str(), e.offset()) << endl;
        cerr << e.what() << endl;
        return EXIT_FAILURE;
    }
    catch (const std::exception& e)
    {
        cerr << e.what() << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
