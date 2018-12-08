/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_ORCUS_DETAIL_CPU_FEATURES_HPP
#define INCLUDED_ORCUS_DETAIL_CPU_FEATURES_HPP

namespace orcus { namespace detail { namespace cpu {

#ifdef __ORCUS_CPU_FEATURES

constexpr bool has_sse42()
{
#ifdef __SSE4_2__
    return true;
#else
    return false;
#endif
}

#else

constexpr bool has_sse42() { return false; }

#endif

}}}

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */