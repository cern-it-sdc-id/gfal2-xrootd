/*
 * Copyright @ Members of the EMI Collaboration, 2010.
 * See www.eu-emi.eu for details on the copyright holders.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Authors : David Cameron, Alejandro Alvarez Ayllon, Adrien Devresse
 *
 */

#include <boost/thread.hpp>
#include <boost/thread/condition_variable.hpp>
#include <iostream>
#include <sys/stat.h>

// This header provides all the required functions except chmod
#include <XrdPosix/XrdPosixXrootd.hh>

// This header is required for chmod
#include <XrdClient/XrdClientAdmin.hh>

// For directory listing
#include <XrdCl/XrdClFileSystem.hh>
#include <XrdCl/XrdClXRootDResponses.hh>

#include <XrdVersion.hh>

// TRUE and FALSE are defined in Glib and xrootd headers
#ifdef TRUE
#undef TRUE
#endif
#ifdef FALSE
#undef FALSE
#endif

#include <gfal_plugins_api.h>
#include "gfal_xrootd_plugin_interface.h"
#include "gfal_xrootd_plugin_utils.h"

GQuark xrootd_domain = g_quark_from_static_string("xroot");

void set_xrootd_log_level()
{
    // Note: xrootd lib logs to stderr
    if (gfal_get_verbose() & GFAL_VERBOSE_TRACE_PLUGIN)
        XrdPosixXrootd::setDebug(4);
    else if (gfal_get_verbose() & GFAL_VERBOSE_TRACE)
        XrdPosixXrootd::setDebug(3);
    else if (gfal_get_verbose() & GFAL_VERBOSE_DEBUG)
        XrdPosixXrootd::setDebug(2);
    else if (gfal_get_verbose() & GFAL_VERBOSE_VERBOSE)
        XrdPosixXrootd::setDebug(1);
    else
        XrdPosixXrootd::setDebug(0);
}


int gfal_xrootd_statG(plugin_handle handle, const char* path, struct stat* buff,
        GError ** err)
{
    std::string sanitizedUrl = normalize_url((gfal2_context_t)handle, path);

    // reset stat fields
    reset_stat(*buff);

    if (XrdPosixXrootd::Stat(sanitizedUrl.c_str(), buff) != 0) {
        g_set_error(err, xrootd_domain, errno, "[%s] Failed to stat file",
                __func__);
        return -1;
    }
    return 0;
}


gfal_file_handle gfal_xrootd_openG(plugin_handle handle, const char *path,
        int flag, mode_t mode, GError ** err)
{

    std::string sanitizedUrl = normalize_url((gfal2_context_t)handle, path);

    int* fd = new int(XrdPosixXrootd::Open(sanitizedUrl.c_str(), flag, mode));
    if (*fd == -1) {
        g_set_error(err, xrootd_domain, errno, "[%s] Failed to open file",
                __func__);
        delete fd;
        return NULL;
    }
    return gfal_file_handle_new(gfal_xrootd_getName(), (gpointer) fd);
}


ssize_t gfal_xrootd_readG(plugin_handle handle, gfal_file_handle fd, void *buff,
        size_t count, GError ** err)
{

    int * fdesc = (int*) (gfal_file_handle_get_fdesc(fd));
    if (!fdesc) {
        g_set_error(err, xrootd_domain, errno, "[%s] Bad file handle",
                __func__);
        return -1;
    }
    ssize_t l = XrdPosixXrootd::Read(*fdesc, buff, count);
    if (l < 0) {
        g_set_error(err, xrootd_domain, errno,
                "[%s] Failed while reading from file", __func__);
        return -1;
    }
    return l;
}


ssize_t gfal_xrootd_writeG(plugin_handle handle, gfal_file_handle fd,
        const void *buff, size_t count, GError ** err)
{

    int * fdesc = (int*) (gfal_file_handle_get_fdesc(fd));
    if (!fdesc) {
        g_set_error(err, xrootd_domain, errno, "[%s] Bad file handle",
                __func__);
        return -1;
    }
    ssize_t l = XrdPosixXrootd::Write(*fdesc, buff, count);
    if (l < 0) {
        g_set_error(err, xrootd_domain, errno,
                "[%s] Failed while writing to file", __func__);
        return -1;
    }
    return l;
}


off_t gfal_xrootd_lseekG(plugin_handle handle, gfal_file_handle fd,
        off_t offset, int whence, GError **err)
{

    int * fdesc = (int*) (gfal_file_handle_get_fdesc(fd));
    if (!fdesc) {
        g_set_error(err, xrootd_domain, errno, "[%s] Bad file handle",
                __func__);
        return -1;
    }
    off_t l = XrdPosixXrootd::Lseek(*fdesc, offset, whence);
    if (l < 0) {
        g_set_error(err, xrootd_domain, errno,
                "[%s] Failed to seek within file", __func__);
        return -1;
    }
    return l;
}


int gfal_xrootd_closeG(plugin_handle handle, gfal_file_handle fd, GError ** err)
{

    int r = 0;
    int * fdesc = (int*) (gfal_file_handle_get_fdesc(fd));
    if (fdesc) {
        r = XrdPosixXrootd::Close(*fdesc);
        if (r != 0)
            g_set_error(err, xrootd_domain, errno, "[%s] Failed to close file",
                    __func__);
        delete (int*) (gfal_file_handle_get_fdesc(fd));
    }
    gfal_file_handle_delete(fd);
    return r;
}


int gfal_xrootd_mkdirpG(plugin_handle handle, const char *url, mode_t mode,
        gboolean pflag, GError **err)
{
    std::string sanitizedUrl = normalize_url((gfal2_context_t)handle, url);

    if (XrdPosixXrootd::Mkdir(sanitizedUrl.c_str(), mode) != 0) {
        if (errno == ECANCELED)
            errno = EEXIST;
        g_set_error(err, xrootd_domain, errno,
                "[%s] Failed to create directory", __func__);
        return -1;
    }
    return 0;
}


int gfal_xrootd_chmodG(plugin_handle handle, const char *url, mode_t mode,
        GError **err)
{
    std::string sanitizedUrl = normalize_url((gfal2_context_t)handle, url);

    XrdClientAdmin client(sanitizedUrl.c_str());
    set_xrootd_log_level();

    if (!client.Connect()) {
        g_set_error(err, xrootd_domain, errno,
                "[%s] Failed to connect to server", __func__);
        return -1;
    }

    int user, group, other;
    file_mode_to_xrootd_ints(mode, user, group, other);

    XrdClientUrlInfo xrdurl(sanitizedUrl.c_str());

    if (!client.Chmod(xrdurl.File.c_str(), user, group, other)) {
        g_set_error(err, xrootd_domain, errno,
                "[%s] Failed to change permissions", __func__);
        return -1;
    }
    return 0;
}


int gfal_xrootd_unlinkG(plugin_handle handle, const char *url,
        GError **err)
{

    std::string sanitizedUrl = normalize_url((gfal2_context_t)handle, url);

    if (XrdPosixXrootd::Unlink(sanitizedUrl.c_str()) != 0) {
        g_set_error(err, xrootd_domain, errno, "[%s] Failed to delete file",
                __func__);
        return -1;
    }
    return 0;
}


int gfal_xrootd_rmdirG(plugin_handle handle, const char *url, GError **err)
{

    std::string sanitizedUrl = normalize_url((gfal2_context_t)handle, url);

    if (XrdPosixXrootd::Rmdir(sanitizedUrl.c_str()) != 0) {
        if (errno == ECANCELED) errno =  ENOTEMPTY;
        else if (errno == ENOSYS) errno = ENOTDIR;
        g_set_error(err, xrootd_domain, errno,
                "[%s] Failed to delete directory", __func__);
        return -1;
    }
    return 0;
}


int gfal_xrootd_accessG(plugin_handle handle, const char *url, int mode,
        GError **err)
{

    std::string sanitizedUrl = normalize_url((gfal2_context_t)handle, url);

    if (XrdPosixXrootd::Access(sanitizedUrl.c_str(), mode) != 0) {
        g_set_error(err, xrootd_domain, errno,
                "[%s] Failed to access file or directory", __func__);
        return -1;
    }
    return 0;
}


int gfal_xrootd_renameG(plugin_handle handle, const char *oldurl,
        const char *urlnew, GError **err)
{

    std::string oldSanitizedUrl = normalize_url((gfal2_context_t)handle, oldurl);
    std::string newSanitizedUrl = normalize_url((gfal2_context_t)handle, urlnew);

    if (XrdPosixXrootd::Rename(oldSanitizedUrl.c_str(), newSanitizedUrl.c_str())
            != 0) {
        g_set_error(err, xrootd_domain, errno,
                "[%s] Failed to rename file or directory", __func__);
        return -1;
    }
    return 0;
}

// Callback class for directory listing
class DirListHandler: public XrdCl::ResponseHandler
{
private:
    XrdCl::URL url;
    XrdCl::FileSystem fs;
    std::list<XrdCl::DirectoryList::ListEntry*> entries;

    struct dirent dbuffer;

    boost::mutex mutex;
    boost::condition_variable cv;
    bool done;

public:
    int errcode;
    std::string errstr;

    DirListHandler(const XrdCl::URL& url): url(url), fs(url), done(false), errcode(0)
    {
        memset(&dbuffer, 0, sizeof(dbuffer));
    }

    int List()
    {
        XrdCl::XRootDStatus status = fs.DirList(url.GetPath(), XrdCl::DirListFlags::Stat, this);
        if (!status.IsOK()) {
            errcode = status.code;
            errstr = status.ToString();
            return -1;
        }
        return 0;
    }

    // AFAIK, this is called only once
    void HandleResponse(XrdCl::XRootDStatus* status, XrdCl::AnyObject* response)
    {
        boost::mutex::scoped_lock lock(mutex);
        if (status->IsOK()) {
            XrdCl::DirectoryList* list;
            response->Get<XrdCl::DirectoryList*>(list);
            if (list) {
                XrdCl::DirectoryList::ConstIterator i;
                for (i = list->Begin(); i != list->End(); ++i) {
                    entries.push_back(*i);
                }
            }
        }
        else {
            errcode = status->code;
            errstr = status->ToString();
        }
        done = true;
        cv.notify_all();
    }

    void StatInfo2Stat(const XrdCl::StatInfo* stinfo, struct stat* st)
    {
        st->st_size = stinfo->GetSize();
        st->st_mtime = stinfo->GetModTime();
        st->st_mode = 0;
        if (stinfo->TestFlags(XrdCl::StatInfo::IsDir))
            st->st_mode |= S_IFDIR;
        if (stinfo->TestFlags(XrdCl::StatInfo::IsReadable))
            st->st_mode |= (S_IRUSR | S_IRGRP | S_IROTH);
        if (stinfo->TestFlags(XrdCl::StatInfo::IsWritable))
            st->st_mode |= (S_IWUSR | S_IWGRP | S_IWOTH);
        if (stinfo->TestFlags(XrdCl::StatInfo::XBitSet))
            st->st_mode |= (S_IXUSR | S_IXGRP | S_IXOTH);
    }

    struct dirent* Get(struct stat* st = NULL)
    {
        if (!done) {
            boost::unique_lock<boost::mutex> lock(mutex);
            cv.timed_wait(lock, boost::get_system_time()+ boost::posix_time::seconds(60));
            if (!done) {
                return NULL;
            }
        }

        if (entries.empty())
            return NULL;

        XrdCl::DirectoryList::ListEntry* entry = entries.front();
        entries.pop_front();

        XrdCl::StatInfo* stinfo = entry->GetStatInfo();

        g_strlcpy(dbuffer.d_name, entry->GetName().c_str(), sizeof(dbuffer.d_name));
        dbuffer.d_reclen = strlen(dbuffer.d_name);

        if (stinfo && stinfo->TestFlags(XrdCl::StatInfo::IsDir))
            dbuffer.d_type = DT_DIR;
        else
            dbuffer.d_type = DT_REG;

        if (st != NULL) {
            if (stinfo != NULL) {
                StatInfo2Stat(stinfo, st);
            }
            else {
#if XrdMajorVNUM(XrdVNUMBER) == 4
                stinfo = new XrdCl::StatInfo();
#else
                stinfo = new XrdCl::StatInfo("");
#endif
                std::string fullPath = url.GetPath() + "/" + dbuffer.d_name;
                XrdCl::XRootDStatus status = this->fs.Stat(fullPath, stinfo);
                if (!status.IsOK()) {
                    errcode = status.code;
                    errstr = status.ToString();
                    return NULL;
                }
                StatInfo2Stat(stinfo, st);
                delete stinfo;
            }
        }

        delete entry;
        return &dbuffer;
    }
};


gfal_file_handle gfal_xrootd_opendirG(plugin_handle handle,
        const char* url, GError** err)
{
    std::string sanitizedUrl = normalize_url((gfal2_context_t)handle, url);
    XrdCl::URL parsed(sanitizedUrl);

    // Need to do stat first so we can fail syncrhonously for some errors!
    struct stat st;
    if (XrdPosixXrootd::Stat(sanitizedUrl.c_str(), &st) != 0) {
        g_set_error(err, xrootd_domain, errno, "[%s] Failed to stat file", __func__);
        return NULL;
    }

    if (!S_ISDIR(st.st_mode)) {
        g_set_error(err, xrootd_domain, ENOTDIR, "[%s] Not a directory", __func__);
        return NULL;
    }

    DirListHandler* handler = new DirListHandler(parsed);

    if (handler->List() != 0) {
        g_set_error(err, xrootd_domain, handler->errcode, "[%s] Failed to open dir: %s",
                __func__, handler->errstr.c_str());
        return NULL;
    }

    return gfal_file_handle_new2(gfal_xrootd_getName(), (gpointer) handler, NULL, url);
}


struct dirent* gfal_xrootd_readdirG(plugin_handle plugin_data,
        gfal_file_handle dir_desc, GError** err)
{
    DirListHandler* handler = (DirListHandler*)(gfal_file_handle_get_fdesc(dir_desc));
    if (!handler) {
        g_set_error(err, xrootd_domain, errno, "[%s] Bad dir handle", __func__);
        return NULL;
    }
    dirent* entry = handler->Get();
    if (!entry && handler->errcode != 0) {
        g_set_error(err, xrootd_domain, handler->errcode, "[%s] Failed reading directory: %s",
                __func__, handler->errstr.c_str());
        return NULL;
    }
    return entry;
}


struct dirent* gfal_xrootd_readdirppG(plugin_handle plugin_data,
        gfal_file_handle dir_desc, struct stat* st, GError** err)
{
    DirListHandler* handler = (DirListHandler*)(gfal_file_handle_get_fdesc(dir_desc));
    if (!handler) {
        g_set_error(err, xrootd_domain, errno, "[%s] Bad dir handle", __func__);
        return NULL;
    }
    dirent* entry = handler->Get(st);
    if (!entry && handler->errcode != 0) {
        g_set_error(err, xrootd_domain, handler->errcode, "[%s] Failed reading directory: %s",
                __func__, handler->errstr.c_str());
        return NULL;
    }
    return entry;
}


int gfal_xrootd_closedirG(plugin_handle plugin_data, gfal_file_handle dir_desc,
        GError** err)
{
    // Free all objects associated with this client
    DirListHandler* handler = (DirListHandler*)(gfal_file_handle_get_fdesc(dir_desc));
    if (handler) {
        delete handler;
    }
    gfal_file_handle_delete(dir_desc);
    return 0;
}


int gfal_xrootd_checksumG(plugin_handle handle, const char* url,
        const char* check_type, char * checksum_buffer, size_t buffer_length,
        off_t start_offset, size_t data_length, GError ** err)
{

    std::string sanitizedUrl = normalize_url((gfal2_context_t)handle, url);
    std::string lowerChecksumType = predefinedChecksumTypeToLower(check_type);

    if (start_offset != 0 || data_length != 0) {
        g_set_error(err, xrootd_domain, ENOTSUP,
                "[%s] XROOTD does not support partial checksums", __func__);
        return -1;
    }

    time_t mTime;
    if (XrdPosixXrootd::QueryChksum(sanitizedUrl.c_str(), mTime,
            checksum_buffer, buffer_length) < 0) {
        g_set_error(err, xrootd_domain, errno,
                "[%s] Could not get the checksum", __func__);
        return -1;
    }

    // Note that the returned value is "type value"
    char* space = ::index(checksum_buffer, ' ');
    if (!space) {
        g_set_error(err, xrootd_domain, errno,
                "[%s] Could not get the checksum (Wrong format)", __func__);
        return -1;
    }
    *space = '\0';

    if (strncmp(checksum_buffer, lowerChecksumType.c_str(),
            lowerChecksumType.length()) != 0) {
        g_set_error(err, xrootd_domain, errno,
                "[%s] Got '%s' while expecting '%s'", __func__, checksum_buffer,
                lowerChecksumType.c_str());
        return -1;
    }

    strcpy(checksum_buffer, space + 1);

    return 0;
}

const char* gfal_xrootd_getName()
{
    return "xrootd";
}
