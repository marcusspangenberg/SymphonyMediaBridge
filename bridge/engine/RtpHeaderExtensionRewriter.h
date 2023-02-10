#pragma once

#include "bridge/engine/SsrcInboundContext.h"
#include "bridge/engine/SsrcOutboundContext.h"
#include "rtp/RtpHeader.h"
#include <cassert>

namespace bridge
{

namespace RtpHeaderExtensionRewriter
{

inline void rewriteVideo(rtp::RtpHeader* rtpHeader,
    const bridge::SsrcInboundContext& senderInboundContext,
    const bridge::SsrcOutboundContext& receiverOutboundContext)
{
    assert(rtpHeader);

    const auto headerExtensions = rtpHeader->getExtensionHeader();
    if (!headerExtensions)
    {
        return;
    }

    const bool senderHasAbsSendTimeEx = senderInboundContext.rtpMap.absSendTimeExtId.isSet();
    const bool receiverHasAbsSendTimeEx = receiverOutboundContext.rtpMap.absSendTimeExtId.isSet();
    const bool absSendTimeExNeedToBeRewritten = senderHasAbsSendTimeEx && receiverHasAbsSendTimeEx &&
        senderInboundContext.rtpMap.absSendTimeExtId.get() != receiverOutboundContext.rtpMap.absSendTimeExtId.get();

    if (absSendTimeExNeedToBeRewritten)
    {
        for (auto& rtpHeaderExtension : headerExtensions->extensions())
        {
            if (rtpHeaderExtension.getId() == senderInboundContext.rtpMap.absSendTimeExtId.get())
            {
                rtpHeaderExtension.setId(receiverOutboundContext.rtpMap.absSendTimeExtId.get());
                return;
            }
        }
    }
}

inline void rewriteAudio(rtp::RtpHeader& rtpHeader,
    const bridge::SsrcInboundContext& senderInboundContext,
    const bridge::SsrcOutboundContext& receiverOutboundContext)
{
    const auto headerExtensions = rtpHeader.getExtensionHeader();
    if (!headerExtensions)
    {
        return;
    }

    for (auto& rtpHeaderExtension : headerExtensions->extensions())
    {
        if (senderInboundContext.rtpMap.audioLevelExtId.isSet() &&
            rtpHeaderExtension.getId() == senderInboundContext.rtpMap.audioLevelExtId.get())
        {
            rtpHeaderExtension.setId(receiverOutboundContext.rtpMap.audioLevelExtId.get());
        }
        else if (senderInboundContext.rtpMap.absSendTimeExtId.isSet() &&
            rtpHeaderExtension.getId() == senderInboundContext.rtpMap.absSendTimeExtId.get())
        {
            rtpHeaderExtension.setId(receiverOutboundContext.rtpMap.absSendTimeExtId.get());
        }
    }
}

} // namespace RtpHeaderExtensionRewriter

} // namespace bridge
