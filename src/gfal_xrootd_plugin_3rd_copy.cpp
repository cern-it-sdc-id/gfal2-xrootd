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


#include <ctype.h>
#include <gfal_plugins_api.h>
#include <transfer/gfal_transfer.h>
#include <transfer/gfal_transfer_plugins.h>
#include <transfer/gfal_transfer_types.h>
#include "gfal_xrootd_plugin_interface.h"
#include "gfal_xrootd_plugin_utils.h"

#undef TRUE
#undef FALSE

#include <XrdCl/XrdClCopyProcess.hh>
#include <XrdVersion.hh>


class CopyFeedback: public XrdCl::CopyProgressHandler
{
public:
    CopyFeedback(gfal2_context_t context, gfalt_params_t p) :
            context(context), params(p), startTime(0)
    {
        this->monitorCallback = gfalt_get_monitor_callback(this->params, NULL);
        monitorCallbackData = gfalt_transfer_status_create(&this->status);
    }

    virtual ~CopyFeedback()
    {
        gfalt_transfer_status_delete(this->monitorCallbackData);
    }

    void BeginJob(uint16_t jobNum, uint16_t jobTotal, const XrdCl::URL *source,
            const XrdCl::URL *destination)
    {
        this->startTime = time(NULL);
        this->source = source->GetURL();
        this->destination = destination->GetURL();

        plugin_trigger_event(this->params, xrootd_domain, GFAL_EVENT_NONE,
                GFAL_EVENT_TRANSFER_ENTER, "%s => %s", this->source.c_str(),
                this->destination.c_str());
    }

#if XrdMajorVNUM(XrdVNUMBER) == 4
    void EndJob(uint16_t jobNum, const XrdCl::PropertyList* result)
    {
        std::ostringstream msg;
        msg << "Job finished";

        if (result->HasProperty("status")) {
            XrdCl::XRootDStatus status;
            result->Get("status", status);
            msg << ", " << status.ToStr();
        }
        if (result->HasProperty("realTarget")) {
            std::string value;
            result->Get("realTarget", value);
            msg << ", Real target: " << value;
        }

        plugin_trigger_event(this->params, xrootd_domain, GFAL_EVENT_NONE,
                GFAL_EVENT_TRANSFER_EXIT, "%s", msg.str().c_str());
    }
#else
    void EndJob(const XrdCl::XRootDStatus &status)
    {
        plugin_trigger_event(this->params, xrootd_domain,
                GFAL_EVENT_NONE, GFAL_EVENT_TRANSFER_EXIT,
                "%s", status.ToStr().c_str());
    }
#endif

#if XrdMajorVNUM(XrdVNUMBER) == 4
    void JobProgress(uint16_t jobNum, uint64_t bytesProcessed,
            uint64_t bytesTotal)
#else
    void JobProgress(uint64_t bytesProcessed, uint64_t bytesTotal)
#endif
    {
        if (this->monitorCallback) {
            time_t now = time(NULL);
            time_t elapsed = now - this->startTime;

            this->status.status = 0;
            this->status.bytes_transfered = bytesProcessed;
            this->status.transfer_time = elapsed;
            if (elapsed > 0)
                this->status.average_baudrate = bytesProcessed / elapsed;
            this->status.instant_baudrate = this->status.average_baudrate;

            this->monitorCallback(this->monitorCallbackData,
                    this->source.c_str(), this->destination.c_str(),
                    NULL);
        }
    }

    bool ShouldCancel()
    {
        return gfal2_is_canceled(this->context);
    }

private:
    gfal2_context_t context;
    gfalt_params_t params;
    gfalt_monitor_func monitorCallback;

    gfalt_transfer_status_t monitorCallbackData;
    gfalt_hook_transfer_plugin_t status;
    time_t startTime;

    std::string source, destination;
};



#if GFAL2_VERSION_MAJOR >=2 && GFAL2_VERSION_MINOR >= 6
int gfal_xrootd_3rdcopy_check(plugin_handle plugin_data,
        gfal2_context_t context, const char* src, const char* dst,
        gfal_url2_check check)
#else
int gfal_xrootd_3rdcopy_check(plugin_handle plugin_data, const char* src,
        const char* dst, gfal_url2_check check)
#endif
{
    if (check != GFAL_FILE_COPY && check != GFAL_BULK_COPY)
        return 0;

    return (strncmp(src, "root://", 7) == 0 && strncmp(dst, "root://", 7) == 0);
}


static void gfal_xrootd_3rd_init_url(gfal2_context_t context, XrdCl::URL& xurl,
        const char* url, const char* token)
{
    xurl.FromString(normalize_url(context, url));
    if (token) {
        XrdCl::URL::ParamsMap params;
        params.insert(std::make_pair("svcClass", token));
        xurl.SetParams(params);
    }
}


int gfal_xrootd_3rd_copy_bulk(plugin_handle plugin_data,
        gfal2_context_t context, gfalt_params_t params, size_t nbfiles,
        const char* const * srcs, const char* const * dsts,
        const char* const * checksums, GError** op_error,
        GError*** file_errors)
{
    GError* internalError = NULL;
    char checksumType[64] = { 0 };
    char checksumValue[512] = { 0 };
    bool performChecksum = gfalt_get_checksum_check(params, NULL);

    XrdCl::CopyProcess copy_process;
    XrdCl::PropertyList results[nbfiles];

    const char* src_spacetoken =  gfalt_get_src_spacetoken(params, NULL);
    const char* dst_spacetoken =  gfalt_get_dst_spacetoken(params, NULL);

    for (int i = 0; i < nbfiles; ++i) {
        XrdCl::URL source_url, dest_url;
        gfal_xrootd_3rd_init_url(context, source_url, srcs[i], src_spacetoken);
        gfal_xrootd_3rd_init_url(context, dest_url, dsts[i], dst_spacetoken);

#if XrdMajorVNUM(XrdVNUMBER) == 4
        XrdCl::PropertyList job;

        job.Set("source", source_url.GetURL());
        job.Set("target", dest_url.GetURL());
        job.Set("force", gfalt_get_replace_existing_file(params, NULL));
        job.Set("posc", true);
        job.Set("thirdParty", "only");
        job.Set("tpcTimeout", gfalt_get_timeout(params, NULL));
#else
        XrdCl::JobDescriptor job;

        job.force = gfalt_get_replace_existing_file(params, NULL);;
        job.posc = true;
        job.thirdParty = true;
        job.thirdPartyFallBack = false;
        job.checkSumPrint = false;
#endif

        if (performChecksum) {
            checksumType[0] = '\0';
            checksumValue[0] = '\0';
            sscanf(checksums[i], "%s:%s", checksumType, checksumValue);

            if (!checksumType[0] || !checksumValue[0]) {
                char* defaultChecksumType = gfal2_get_opt_string(context, XROOTD_CONFIG_GROUP, XROOTD_DEFAULT_CHECKSUM, &internalError);
                if (internalError) {
                    g_set_error(op_error, xrootd_domain, internalError->code, "[%s] %s", __func__, internalError->message);
                    g_error_free(internalError);
                    return -1;
                }

                strncpy(checksumType, defaultChecksumType, sizeof(checksumType));
                g_free(defaultChecksumType);
            }

#if XrdMajorVNUM(XrdVNUMBER) == 4
            job.Set("checkSumMode",
                    gfal2_get_opt_string_with_default(context, XROOTD_CONFIG_GROUP, XROOTD_CHECKSUM_MODE, "end2end"));
            job.Set("checkSumType", predefined_checksum_type_to_lower(checksumType));
            job.Set("checkSumPreset", checksumValue);
#else
            job.checkSumType = predefined_checksum_type_to_lower(checksumType);
            job.checkSumPreset = checksumValue;
#endif
        }

#if XrdMajorVNUM(XrdVNUMBER) == 4
        copy_process.AddJob(job, &(results[i]));
#else
        copy_process.AddJob(&job);
#endif

    }

    // Configuration job
#if XrdMajorVNUM(XrdVNUMBER) == 4
    int parallel = gfal2_get_opt_integer_with_default(context,
            XROOTD_CONFIG_GROUP, XROOTD_PARALLEL_COPIES,
            20);

    XrdCl::PropertyList config_job;
    config_job.Set("jobType", "configuration");
    config_job.Set("parallel", parallel);
    copy_process.AddJob(config_job, NULL);
#endif

    XrdCl::XRootDStatus status = copy_process.Prepare();
    if (!status.IsOK()) {
        g_set_error(op_error, xrootd_domain,
                xrootd_errno_to_posix_errno(status.errNo),
                "[%s] Error on XrdCl::CopyProcess::Prepare(): %s", __func__,
                status.ToStr().c_str());
        return -1;
    }

    CopyFeedback feedback(context, params);
    status = copy_process.Run(&feedback);

    // On bulk operations, even if there is one single failure we will get it
    // here, so ignore!
    if (nbfiles == 1 && !status.IsOK()) {
        g_set_error(op_error, xrootd_domain,
                xrootd_errno_to_posix_errno(status.errNo),
                "[%s] Error on XrdCl::CopyProcess::Run(): %s", __func__,
                status.ToStr().c_str());
        return -1;
    }

    // For bulk operations, here we do get the actual status per file
    int n_failed = 0;
    *file_errors = g_new0(GError*, nbfiles);
    for (int i = 0; i < nbfiles; ++i) {
        status = results[i].Get<XrdCl::XRootDStatus>("status");
        if (!status.IsOK()) {
            g_set_error(&((*file_errors)[i]), xrootd_domain,
                    xrootd_errno_to_posix_errno(status.errNo),
                    "[%s] Error on XrdCl::CopyProcess::Run(): %s", __func__,
                    status.ToStr().c_str());
            ++n_failed;
        }
    }

    return -n_failed;
}


int gfal_xrootd_3rd_copy(plugin_handle plugin_data, gfal2_context_t context,
        gfalt_params_t params, const char* src, const char* dst, GError** err)
{
    GError* op_error = NULL;
    GError** file_error = NULL;

    char checksumType[64] = { 0 };
    char checksumValue[512] = { 0 };
    gfalt_get_user_defined_checksum(params, checksumType, sizeof(checksumType),
            checksumValue, sizeof(checksumValue),
            NULL);
    char checksumConcat[576];
    snprintf(checksumConcat, sizeof(checksumConcat), "%s:%s", checksumType, checksumValue);

    int ret = gfal_xrootd_3rd_copy_bulk(plugin_data,
            context, params, 1,
            &src, &dst, (const char*const*)&checksumConcat,
            &op_error, &file_error);

    if (ret < 0) {
        if (op_error) {
            gfal2_propagate_prefixed_error(err, op_error, __func__);
        }
        else {
            gfal2_propagate_prefixed_error(err, file_error[0], __func__);
            g_free(file_error);
        }
    }

    return ret;
}
