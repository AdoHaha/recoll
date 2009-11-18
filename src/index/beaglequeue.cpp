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
#ifndef lint
static char rcsid[] = "@(#$Id: $ (C) 2005 J.F.Dockes";
#endif
#include "autoconfig.h"

#include <sys/types.h>
#include <string.h>

#include "autoconfig.h"
#include "pathut.h"
#include "debuglog.h"
#include "fstreewalk.h"
#include "beaglequeue.h"
#include "smallut.h"
#include "fileudi.h"
#include "internfile.h"
#include "wipedir.h"
#include "circache.h"
#include "indexer.h"
#include "readfile.h"
#include "conftree.h"
#include "transcode.h"
#include "cancelcheck.h"

#include <vector>
#include <fstream>
#include <sstream>
using namespace std;

#include <sys/stat.h>

const string keybght("beagleHitType");


// Beagle creates a file named .xxx (where xxx is the name for the main file
// in the queue), to hold external metadata (http or created by Beagle).
// This class reads the .xxx, dotfile, and turns it into an Rcl::Doc holder
class BeagleDotFile {
public:
    BeagleDotFile(RclConfig *conf, const string& fn)
        : m_conf(conf), m_fn(fn)
    {}

    // Read input line, strip it of eol and return as c++ string
    bool readLine(string& line)
    {
        static const int LL = 2048;
        char cline[LL]; 
        cline[0] = 0;
        m_input.getline(cline, LL-1);
        if (!m_input.good()) {
            if (m_input.bad()) {
                LOGERR(("beagleDotFileRead: input.bad()\n"));
            }
            return false;
        }
        int ll = strlen(cline);
        while (ll > 0 && (cline[ll-1] == '\n' || cline[ll-1] == '\r')) {
            cline[ll-1] = 0;
            ll--;
        }
        line.assign(cline, ll);
        LOGDEB2(("BeagleDotFile:readLine: [%s]\n", line.c_str()));
        return true;
    }

    // Process a beagle dot file and set interesting stuff in the doc
    bool toDoc(Rcl::Doc& doc)
    {
        string line;

	m_input.open(m_fn.c_str(), ios::in);
        if (!m_input.good()) {
            LOGERR(("BeagleDotFile: open failed for [%s]\n", m_fn.c_str()));
            return false;
        }

        // Read the 3 first lines: 
        // - url
        // - hit type: we only know about Bookmark and WebHistory for now
        // - content-type.
        if (!readLine(line))
            return false;
        doc.url = line;
        if (!readLine(line))
            return false;
        doc.meta[keybght] = line;
        if (!readLine(line))
            return false;
        doc.mimetype = line;

        // We set the bookmarks mtype as html (the text is empty
        // anyway), so that the html viewer will be called on 'Open'
        bool isbookmark = false;
        if (!stringlowercmp("bookmark", doc.meta[keybght])) {
            isbookmark = true;
            doc.mimetype = "text/html";
        }

        string confstr;
        string ss(" ");
        // Read the rest: fields and keywords. We do a little
        // massaging of the input lines, then use a ConfSimple to
        // parse, and finally insert the key/value pairs into the doc
        // meta[] array
        for (;;) {
            if (!readLine(line)) {
                // Eof hopefully
                break;
            }
            if (line.find("t:") != 0)
                continue;
            line = line.substr(2);
            confstr += line + "\n";
        }
        ConfSimple fields(confstr, 1);
        list<string> names = fields.getNames("");
        for (list<string>::iterator it = names.begin();
             it != names.end(); it++) {
            string value;
            fields.get(*it, value, "");
            if (!value.compare("undefined") || !value.compare("null"))
                continue;

            string *valuep = &value;
            string cvalue;
            if (isbookmark) {
                // It appears that bookmarks are stored in the users'
                // locale charset (not too sure). No idea what to do
                // for other types, would have to check the plugin.
                string charset = m_conf->getDefCharset(true);
                transcode(value, cvalue, charset,  "UTF-8"); 
                valuep = &cvalue;
            }
                
            string caname = m_conf->fieldCanon(*it);
            doc.meta[caname].append(ss + *valuep);
        }

        // Finally build the confsimple that we will save to the
        // cache, from the doc fields. This could also be done in
        // parallel with the doc.meta build above, but simpler this
        // way.  We need it because not all interesting doc fields are
        // in the meta array (ie: mimetype, url), and we want
        // something homogenous and easy to save.
        for (map<string,string>::const_iterator it = doc.meta.begin();
             it != doc.meta.end(); it++) {
            m_fields.set((*it).first, (*it).second, "");
        }
        m_fields.set("url", doc.url, "");
        m_fields.set("mimetype", doc.mimetype, "");

        return true;
    }    

    RclConfig *m_conf;
    ConfSimple m_fields;
    string m_fn;
    ifstream m_input;
};

const string badtmpdirname = "/no/such/dir/really/can/exist";

// Initialize. Compute paths and create a temporary directory that will be
// used by internfile()
BeagleQueueIndexer::BeagleQueueIndexer(RclConfig *cnf, Rcl::Db *db,
                                       DbIxStatusUpdater *updfunc)
    : m_config(cnf), m_db(db), m_cache(0), m_updater(updfunc), 
      m_nocacheindex(false)
{

    if (!m_config->getConfParam("beaglequeuedir", m_queuedir))
        m_queuedir = path_tildexpand("~/.beagle/ToIndex/");
    path_catslash(m_queuedir);

    if (m_db && (m_tmpdir.empty() || access(m_tmpdir.c_str(), 0) < 0)) {
	string reason;
        if (!maketmpdir(m_tmpdir, reason)) {
	    LOGERR(("DbIndexer: cannot create temporary directory: %s\n",
		    reason.c_str()));
            m_tmpdir = badtmpdirname;
	}
    }

    string ccdir;
    m_config->getConfParam("webcachedir", ccdir);
    if (ccdir.empty())
        ccdir = "webcache";
    ccdir = path_tildexpand(ccdir);
    // If not an absolute path, compute relative to config dir
    if (ccdir.at(0) != '/')
        ccdir = path_cat(m_config->getConfDir(), ccdir);

    int maxmbs = 20;
    m_config->getConfParam("webcachemaxmbs", &maxmbs);
    m_cache = new CirCache(ccdir);
    m_cache->create(off_t(maxmbs)*1000*1024, true);
}

BeagleQueueIndexer::~BeagleQueueIndexer()
{
    LOGDEB(("BeagleQueueIndexer::~\n"));
    if (m_tmpdir.length() && m_tmpdir.compare(badtmpdirname)) {
	wipedir(m_tmpdir);
	if (rmdir(m_tmpdir.c_str()) < 0) {
	    LOGERR(("BeagleQueueIndexer::~: cannot clear temp dir %s\n",
		    m_tmpdir.c_str()));
	}
    }
    deleteZ(m_cache);
}

// Read  document from cache. Return the metadata as an Rcl::Doc
// @param htt Beagle Hit Type 
bool BeagleQueueIndexer::getFromCache(const string& udi, Rcl::Doc &dotdoc, 
                                      string& data, string *htt)
{
    string dict;

    if (!m_cache->get(udi, dict, data))
        return false;

    ConfSimple cf(dict, 1);
    
    if (htt)
        cf.get(keybght, *htt, "");

    // Build a doc from saved metadata 
    cf.get("url", dotdoc.url, "");
    cf.get("mimetype", dotdoc.mimetype, "");
    cf.get("fmtime", dotdoc.fmtime, "");
    cf.get("fbytes", dotdoc.fbytes, "");
    dotdoc.sig = "";
    list<string> names = cf.getNames("");
    for (list<string>::const_iterator it = names.begin();
         it != names.end(); it++) {
        cf.get(*it, dotdoc.meta[*it], "");
    }
    return true;
}

// Index document stored in the cache. 
bool BeagleQueueIndexer::indexFromCache(const string& udi)
{
    if (!m_db)
        return false;

    CancelCheck::instance().checkCancel();

    Rcl::Doc dotdoc;
    string data;
    string hittype;

    if (!getFromCache(udi, dotdoc, data, &hittype))
        return false;

    if (hittype.empty()) {
        LOGERR(("BeagleIndexer::index: cc entry has no hit type\n"));
        return false;
    }
        
    if (!stringlowercmp("bookmark", hittype)) {
        // Just index the dotdoc
        dotdoc.meta[Rcl::Doc::keybcknd] = "BGL";
        return m_db->addOrUpdate(udi, "", dotdoc);
    } else if (stringlowercmp("webhistory", dotdoc.meta[keybght]) ||
               (dotdoc.mimetype.compare("text/html") &&
                dotdoc.mimetype.compare("text/plain"))) {
        LOGDEB(("BeagleQueueIndexer: skipping: hittype %s mimetype %s\n",
                dotdoc.meta[keybght].c_str(), dotdoc.mimetype.c_str()));
        return true;
    } else {
        Rcl::Doc doc;
        FileInterner interner(data, m_config, m_tmpdir, 
                              FileInterner::FIF_doUseInputMimetype,
                              dotdoc.mimetype);
        string ipath;
        FileInterner::Status fis;
        try {
            fis = interner.internfile(doc, ipath);
        } catch (CancelExcept) {
            LOGERR(("BeagleQueueIndexer: interrupted\n"));
            return false;
        }
        if (fis != FileInterner::FIDone) {
            LOGERR(("BeagleQueueIndexer: bad status from internfile\n"));
            return false;
        }

        doc.mimetype = dotdoc.mimetype;
        doc.fmtime = dotdoc.fmtime;
        doc.url = dotdoc.url;
        doc.fbytes = dotdoc.fbytes;
        doc.sig = "";
        doc.meta[Rcl::Doc::keybcknd] = "BGL";
        return m_db->addOrUpdate(udi, "", doc);
    }
}

bool BeagleQueueIndexer::index()
{
    if (!m_db)
        return false;
    LOGDEB(("BeagleQueueIndexer::processqueue: [%s]\n", m_queuedir.c_str()));
    m_config->setKeyDir(m_queuedir);

    // First check/index files found in the cache. If the index was reset,
    // this actually does work, else it sets the existence flags (avoid
    // purging). We don't do this when called from indexFiles
    if (!m_nocacheindex) {
        bool eof;
        if (!m_cache->rewind(eof)) {
            // rewind can return eof if the cache is empty
            if (!eof)
                return false;
        }

        // The cache is walked in chronogical order, but we want to
        // index the newest files first (there can be several versions
        // of a given file in the cache). Have to revert the
        // list. This would be a problem with a big cache, because the
        // udis can be big (ie 150 chars), and would be more
        // efficiently performed by the cache, which could use the
        // smaller offsets.
        //
        // Another approach would be to just walk chronogical and
        // reindex all versions: would waste processing but save
        // memory
        vector<string> alludis;
        alludis.reserve(20000);
        while (m_cache->next(eof)) {
            string dict;
            m_cache->getcurrentdict(dict);
            ConfSimple cf(dict, 1);
            string udi;
            if (!cf.get("udi", udi, ""))
                continue;
            alludis.push_back(udi);
        }
        for (vector<string>::reverse_iterator it = alludis.rbegin();
             it != alludis.rend(); it++) {
            if (m_db->needUpdate(*it, "")) {
                try {
                    indexFromCache(*it);
                } catch (CancelExcept) {
                    LOGERR(("BeagleQueueIndexer: interrupted\n"));
                    return false;
                }
            }
        }
    }

    // Finally index the queue
    FsTreeWalker walker(FsTreeWalker::FtwNoRecurse);
    walker.addSkippedName(".*");
    FsTreeWalker::Status status =walker.walk(m_queuedir, *this);
    LOGDEB(("BeagleQueueIndexer::processqueue: done: status %d\n", status));
    return true;
}

// Index a list of files (sent by the real time monitor)
bool BeagleQueueIndexer::indexFiles(list<string>& files)
{
    LOGDEB(("BeagleQueueIndexer::indexFiles\n"));

    if (!m_db) {
        LOGERR(("BeagleQueueIndexer::indexfiles no db??\n"));
        return false;
    }
    for (list<string>::iterator it = files.begin(); it != files.end();) {
        if (it->empty()) {//??
            it++; continue;
        }
        string father = path_getfather(*it);
        if (father.compare(m_queuedir)) {
            LOGDEB(("BeagleQueueIndexer::indexfiles: skipping [%s] (nq)\n", 
                    it->c_str()));
            it++; continue;
        }
        // Pb: we are often called with the dot file, before the
        // normal file exists, and sometimes never called for the
        // normal file afterwards (ie for bookmarks where the normal
        // file is empty). So we perform a normal queue run at the end
        // of the function to catch older stuff. Still this is not
        // perfect, sometimes some files will not be indexed before
        // the next run.
        string fn = path_getsimple(*it);
        if (fn.empty() || fn.at(0) == '.') {
            it++; continue;
        }
        struct stat st;
        if (lstat(it->c_str(), &st) != 0) {
            LOGERR(("BeagleQueueIndexer::indexfiles: cant stat [%s]\n", 
                    it->c_str()));
            it++; continue;
        }
	if (!S_ISREG(st.st_mode)) {
	    LOGDEB(("BeagleQueueIndexer::indexfiles: skipping [%s] (nr)\n", 
		    it->c_str()));
            it++; continue;
	}

        processone(*it, &st, FsTreeWalker::FtwRegular);
        it = files.erase(it);
    }
    m_nocacheindex = true;
    index();
    // Note: no need to reset nocacheindex, we're in the monitor now
    return true;
}

FsTreeWalker::Status 
BeagleQueueIndexer::processone(const string &path,
                               const struct stat *stp,
                               FsTreeWalker::CbFlag flg)
{
    if (!m_db) //??
        return FsTreeWalker::FtwError;

    bool dounlink = false;

    if (flg != FsTreeWalker::FtwRegular) 
        return FsTreeWalker::FtwOk;

    string dotpath = path_cat(path_getfather(path), 
                              string(".") + path_getsimple(path));
    LOGDEB(("BeagleQueueIndexer: prc1: [%s]\n", path.c_str()));

    BeagleDotFile dotfile(m_config, dotpath);
    Rcl::Doc dotdoc;
    string udi, udipath;
    if (!dotfile.toDoc(dotdoc))
        goto out;
    //dotdoc.dump(1);

    // Have to use the hit type for the udi, because the same url can exist
    // as a bookmark or a page.
    udipath = path_cat(dotdoc.meta[keybght], url_gpath(dotdoc.url));
    make_udi(udipath, "", udi);

    LOGDEB(("BeagleQueueIndexer: prc1: udi [%s]\n", udi.c_str()));
    char ascdate[20];
    sprintf(ascdate, "%ld", long(stp->st_mtime));

    // We only process bookmarks or text/html and text/plain files.
    if (!stringlowercmp("bookmark", dotdoc.meta[keybght])) {
        // For bookmarks, we just index the doc that was built from the
        // metadata.
        if (dotdoc.fmtime.empty())
            dotdoc.fmtime = ascdate;

        char cbuf[100]; 
        sprintf(cbuf, "%ld", (long)stp->st_size);
        dotdoc.fbytes = cbuf;

        // Document signature for up to date checks: none. 
        dotdoc.sig = "";
        
        dotdoc.meta[Rcl::Doc::keybcknd] = "BGL";
        if (!m_db->addOrUpdate(udi, "", dotdoc)) 
            return FsTreeWalker::FtwError;

    } else if (stringlowercmp("webhistory", dotdoc.meta[keybght]) ||
               (dotdoc.mimetype.compare("text/html") &&
                dotdoc.mimetype.compare("text/plain"))) {
        LOGDEB(("BeagleQueueIndexer: skipping: hittype %s mimetype %s\n",
                dotdoc.meta[keybght].c_str(), dotdoc.mimetype.c_str()));
        // Unlink them anyway
        dounlink = true;
        goto out;
    } else {
        Rcl::Doc doc;
        FileInterner interner(path, stp, m_config, m_tmpdir, 
                              FileInterner::FIF_doUseInputMimetype,
                              &dotdoc.mimetype);
        string ipath;
        FileInterner::Status fis;
        try {
            fis = interner.internfile(doc, ipath);
        } catch (CancelExcept) {
            LOGERR(("BeagleQueueIndexer: interrupted\n"));
            goto out;
        }
        if (fis != FileInterner::FIDone) {
            LOGERR(("BeagleQueueIndexer: bad status from internfile\n"));
            goto out;
        }

        if (doc.fmtime.empty())
            doc.fmtime = ascdate;

        char cbuf[100]; 
        sprintf(cbuf, "%ld", (long)stp->st_size);
        doc.fbytes = cbuf;
        // Document signature for up to date checks: none. 
        doc.sig = "";
        doc.url = dotdoc.url;

        doc.meta[Rcl::Doc::keybcknd] = "BGL";
        if (!m_db->addOrUpdate(udi, "", doc)) 
            return FsTreeWalker::FtwError;
    }

    // Copy to cache
    {
        // doc fields not in meta, needing saving to the cache
        dotfile.m_fields.set("fmtime", dotdoc.fmtime, "");
        dotfile.m_fields.set("fbytes", dotdoc.fbytes, "");
        dotfile.m_fields.set("udi", udi, "");
        string fdata;
        file_to_string(path, fdata);
        if (!m_cache->put(udi, &dotfile.m_fields, fdata, 0)) {
            LOGERR(("BeagleQueueIndexer::prc1: cache_put failed; %s\n",
                    m_cache->getReason().c_str()));
            goto out;
        }
    }

    dounlink = true;
out:
    if (dounlink) {
        unlink(path.c_str());
        unlink(dotpath.c_str());
    }
    return FsTreeWalker::FtwOk;
}
