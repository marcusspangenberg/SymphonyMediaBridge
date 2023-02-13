#pragma once

#include "bridge/endpointActions/ActionContext.h"
#include "httpd/HttpRequestHandler.h"
#include <functional>
#include <map>
#include <string>

#if ENABLE_LEGACY_API
#include "bridge/LegacyApiRequestHandler.h"
#endif

namespace config
{
class Config;
}

namespace api
{
struct AllocateEndpoint;
struct EndpointDescription;
struct Recording;
} // namespace api

namespace transport
{
class SslDtls;
class ProbeServer;
} // namespace transport

namespace utils
{
namespace StringTokenizer
{
struct Token;
}
} // namespace utils

namespace bridge
{

class Mixer;
class MixerManager;
struct AudioStreamDescription;
struct VideoStreamDescription;
class RequestLogger;

class ApiRequestHandler : public httpd::HttpRequestHandler, public ActionContext
{
public:
    ApiRequestHandler(bridge::MixerManager& mixerManager,
        transport::SslDtls& sslDtls,
        transport::ProbeServer& probeServer,
        const config::Config& config);
    httpd::Response onRequest(const httpd::Request& request) override;

private:
    std::atomic<uint32_t> _lastAutoRequestId;
#if ENABLE_LEGACY_API
    std::unique_ptr<LegacyApiRequestHandler> _legacyApiRequestHandler;
#endif

    httpd::Response callEndpointAction(RequestLogger&, const httpd::Request&);
};

} // namespace bridge
