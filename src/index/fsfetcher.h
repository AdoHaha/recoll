/* Copyright (C) 2012 J.F.Dockes
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#ifndef _FSFETCHER_H_INCLUDED_
#define _FSFETCHER_H_INCLUDED_

#include "fetcher.h"
#include "pathut.h"

/** 
 * The file-system fetcher: 
 */
class FSDocFetcher : public DocFetcher {
public:
    /** FSDocFetcher::fetch always returns a file name */
    virtual bool fetch(RclConfig* cnf, const Rcl::Doc& idoc, RawDoc& out);
    
    /** Calls stat to retrieve file signature data */
    virtual bool makesig(RclConfig* cnf,const Rcl::Doc& idoc, std::string& sig);
    virtual DocFetcher::Reason testAccess(RclConfig* cnf, const Rcl::Doc& idoc);
    FSDocFetcher() {}
    virtual ~FSDocFetcher() {}
    FSDocFetcher(const FSDocFetcher&) = delete;
    FSDocFetcher& operator=(const FSDocFetcher&) = delete;
};

extern void fsmakesig(const struct PathStat *stp, std::string& out);

#endif /* _FSFETCHER_H_INCLUDED_ */
