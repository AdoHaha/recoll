/* Copyright (C) 2014 J.F.Dockes
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
#ifndef _APPFORMIME_H_INCLUDED_
#define _APPFORMIME_H_INCLUDED_

#include <string>

/**
 * Rather strangely, I could not find a reasonably simple piece of
 * code which would parse /usr/share/applications to return a list of
 * apps for a given mime type. So here goes. Note that the implementation
 * is very primitive for now (no use of cache file, no updating once built).
 */
class DesktopDb {
public:
    class AppDef {
    public:
        AppDef(const std::string& nm, const std::string& cmd)
            : name(nm), command(cmd)
            {}
        std::string name;
        std::string command;
    };

    static DesktopDb* getDb();
    static const string& getReason();
    ~DesktopDb();

    /**
     * Get a list of applications able to process a given MIME type.
     * @param mime MIME type we want the apps for
     * @param[output] apps appropriate applications 
     * @param[output] reason if we fail, an explanation ?
     * @return true for no error (apps may still be empty). false if a serious
     *   problem was detected.
     */
    bool appForMime(const std::string& mime, vector<AppDef> *apps, 
                    std::string *reason);

private:
    DesktopDb();
    DesktopDb(const DesktopDb &);
    DesktopDb& operator=(const DesktopDb &);
};


#endif /* _APPFORMIME_H_INCLUDED_ */
