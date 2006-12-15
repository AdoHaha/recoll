#ifndef lint
static char rcsid[] = "@(#$Id: mh_text.cpp,v 1.6 2006-12-15 12:40:02 dockes Exp $ (C) 2005 J.F.Dockes";
#endif
/*
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
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <iostream>
#include <string>
#ifndef NO_NAMESPACES
using namespace std;
#endif /* NO_NAMESPACES */

#include "mh_text.h"
#include "csguess.h"
#include "debuglog.h"
#include "readfile.h"
#include "transcode.h"

// Process a plain text file
bool MimeHandlerText::set_document_file(const string &fn)
{
    string otext;
    if (!file_to_string(fn, otext))
	return false;
    return set_document_string(otext);
}
    
bool MimeHandlerText::set_document_string(const string& otext)
{
    m_text = otext;
    m_havedoc = true;
    return true;
}

bool MimeHandlerText::next_document()
{	
    if (m_havedoc == false)
	return false;
    m_havedoc = false;
    LOGDEB1(("MimeHandlerText::mkDoc: transcod from %s to utf-8\n", 
	     m_defcharset.c_str()));

    // Avoid unneeded copy. This gets a reference to an empty string which is
    // the entry for "content"
    string& utf8 = m_metaData["content"];

    // Note that we transcode always even if defcharset is already utf-8: 
    // this validates the encoding.
    if (!transcode(m_text, utf8, m_defcharset, "UTF-8")) {
	LOGERR(("MimeHandlerText::mkDoc: transcode to utf-8 failed "
		"for charset [%s]\n", m_defcharset.c_str()));
	utf8.erase();
	return false;
    }

    m_metaData["origcharset"] = m_defcharset;
    m_metaData["charset"] = "utf-8";
    m_metaData["mimetype"] = "text/plain";
    return true;
}
