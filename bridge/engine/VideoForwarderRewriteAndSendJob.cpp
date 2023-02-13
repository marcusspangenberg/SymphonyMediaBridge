#include "bridge/engine/VideoForwarderRewriteAndSendJob.h"
#include "bridge/engine/EngineMessageListener.h"
#include "bridge/engine/PacketCache.h"
#include "bridge/engine/RtpHeaderExtensionRewriter.h"
#include "bridge/engine/SsrcInboundContext.h"
#include "bridge/engine/SsrcOutboundContext.h"
#include "bridge/engine/Vp8Rewriter.h"
#include "codec/H264Header.h"
#include "codec/Vp8Header.h"
#include "transport/Transport.h"

namespace bridge
{

VideoForwarderRewriteAndSendJob::VideoForwarderRewriteAndSendJob(SsrcOutboundContext& outboundContext,
    SsrcInboundContext& senderInboundContext,
    memory::UniquePacket packet,
    transport::Transport& transport,
    const uint32_t extendedSequenceNumber,
    EngineMessageListener& mixerManager,
    size_t endpointIdHash,
    EngineMixer& mixer)
    : jobmanager::CountedJob(transport.getJobCounter()),
      _outboundContext(outboundContext),
      _senderInboundContext(senderInboundContext),
      _packet(std::move(packet)),
      _transport(transport),
      _extendedSequenceNumber(extendedSequenceNumber),
      _mixerManager(mixerManager),
      _endpointIdHash(endpointIdHash),
      _mixer(mixer)
{
    assert(_packet);
    assert(_packet->getLength() > 0);
}

void VideoForwarderRewriteAndSendJob::run()
{
    auto rtpHeader = rtp::RtpHeader::fromPacket(*_packet);
    if (!rtpHeader)
    {
        assert(false); // should have been checked multiple times
        return;
    }

    if (_outboundContext.rtpMap.format == RtpMap::Format::RTX)
    {
        logger::warn("%s rtx packet should not reach rewrite and send. ssrc %u, seq %u",
            "VideoForwarderRewriteAndSendJob",
            _transport.getLoggableId().c_str(),
            rtpHeader->ssrc.get(),
            _extendedSequenceNumber);
        return;
    }

    if (!_outboundContext.packetCache.isSet())
    {
        logger::debug("New ssrc %u seen on %s, sending request to add videoPacketCache to transport",
            "VideoForwarderRewriteAndSendJob",
            _outboundContext.ssrc,
            _transport.getLoggableId().c_str());

        _outboundContext.packetCache.set(nullptr);

        EngineMessage::Message message(EngineMessage::Type::AllocateVideoPacketCache);
        message.command.allocateVideoPacketCache.mixer = &_mixer;
        message.command.allocateVideoPacketCache.ssrc = _outboundContext.ssrc;
        message.command.allocateVideoPacketCache.endpointIdHash = _endpointIdHash;
        _mixerManager.onMessage(std::move(message));
    }

    const bool isKeyFrame = _outboundContext.rtpMap.format == RtpMap::Format::VP8
        ? codec::Vp8Header::isKeyFrame(rtpHeader->getPayload(),
              codec::Vp8Header::getPayloadDescriptorSize(rtpHeader->getPayload(),
                  _packet->getLength() - rtpHeader->headerLength()))
        : codec::H264::isKeyFrame(rtpHeader->getPayload(), 1);

    const auto ssrc = rtpHeader->ssrc.get();
    if (ssrc != _outboundContext.originalSsrc)
    {
        if (!isKeyFrame)
        {
            _outboundContext.needsKeyframe = true;
            _senderInboundContext.pliScheduler.triggerPli();
        }
    }

    if (_outboundContext.needsKeyframe)
    {
        if (!isKeyFrame)
        {
            // dropping P-frames until key frame appears
            return;
        }
        else
        {
            _outboundContext.needsKeyframe = false;

            logger::debug("%s requested key frame from %u on ssrc %u",
                "VideoForwarderRewriteAndSendJob",
                _transport.getLoggableId().c_str(),
                _senderInboundContext.ssrc,
                _outboundContext.ssrc);
        }
    }

    if (!_outboundContext.shouldSend(rtpHeader->ssrc, _extendedSequenceNumber))
    {
        logger::debug("%s dropping packet. Rewrite not suitable ssrc %u, seq %u",
            "VideoForwarderRewriteAndSendJob",
            _transport.getLoggableId().c_str(),
            rtpHeader->ssrc.get(),
            _extendedSequenceNumber);

        return;
    }

    if (!_transport.isConnected())
    {
        return;
    }

    uint32_t rewrittenExtendedSequenceNumber = 0;

    bool rewriteResult = false;
    if (_outboundContext.rtpMap.format == RtpMap::Format::VP8)
    {
        rewriteResult = Vp8Rewriter::rewrite(_outboundContext,
            *_packet,
            _extendedSequenceNumber,
            _transport.getLoggableId().c_str(),
            rewrittenExtendedSequenceNumber,
            isKeyFrame);
    }
    else if (_outboundContext.rtpMap.format == RtpMap::Format::H264)
    {
        rewriteResult = Vp8Rewriter::rewriteH264(_outboundContext,
            *_packet,
            _extendedSequenceNumber,
            _transport.getLoggableId().c_str(),
            rewrittenExtendedSequenceNumber,
            isKeyFrame);
    }

    if (!rewriteResult)
    {
        return;
    }

    rtpHeader->payloadType = _outboundContext.rtpMap.payloadType;
    RtpHeaderExtensionRewriter::rewriteVideo(rtpHeader, _senderInboundContext, _outboundContext);

    if (_outboundContext.packetCache.isSet() && _outboundContext.packetCache.get())
    {
        if (!_outboundContext.packetCache.get()->add(*_packet, rtpHeader->sequenceNumber))
        {
            logger::warn("%s failed to add packet to cache. ssrc %u, seq %u",
                "VideoForwarderRewriteAndSendJob",
                _transport.getLoggableId().c_str(),
                rtpHeader->ssrc.get(),
                rtpHeader->sequenceNumber.get());
        }
    }

    _transport.protectAndSend(std::move(_packet));
}

} // namespace bridge
