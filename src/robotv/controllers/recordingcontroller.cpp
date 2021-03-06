/*
 *      vdr-plugin-robotv - roboTV server plugin for VDR
 *
 *      Copyright (C) 2016 Alexander Pipelka
 *
 *      https://github.com/pipelka/vdr-plugin-robotv
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "recordingcontroller.h"
#include "recordings/packetplayer.h"
#include "recordings/recordingscache.h"
#include "robotv/robotvclient.h"
#include "tools/recid2uid.h"

RecordingController::RecordingController(RoboTvClient* parent) : m_parent(parent), m_recPlayer(NULL) {
}

RecordingController::RecordingController(const RecordingController& orig) {
}

RecordingController::~RecordingController() {
    delete m_recPlayer;
}

bool RecordingController::process(MsgPacket* request, MsgPacket* response) {
    switch(request->getMsgID()) {
        case ROBOTV_RECSTREAM_OPEN:
            return processOpen(request, response);

        case ROBOTV_RECSTREAM_CLOSE:
            return processClose(request, response);

        case ROBOTV_RECSTREAM_REQUEST:
            return processRequest(request, response);

        case ROBOTV_RECSTREAM_SEEK:
            return processSeek(request, response);

        case ROBOTV_RECSTREAM_PAUSE:
            return processPause(request, response);
    }

    return false;
}

bool RecordingController::processOpen(MsgPacket* request, MsgPacket* response) {
    cRecording* recording = NULL;

    const char* recid = request->get_String();
    unsigned int uid = recid2uid(recid);
    DEBUGLOG("lookup recid: %s (uid: %u)", recid, uid);
    recording = RecordingsCache::instance().lookup(uid);

    if(recording && m_recPlayer == NULL) {
        m_recPlayer = new PacketPlayer(recording);

        delete m_recPlayer->requestPacket(false);
        m_recPlayer->reset();

        uint32_t length = (m_recPlayer->endTime().count() - m_recPlayer->startTime().count()) / 1000;

        response->put_U32(ROBOTV_RET_OK);
        response->put_U32(0);
        response->put_U64(m_recPlayer->getLengthBytes());
        response->put_U8(recording->IsPesRecording());//added for TS
        response->put_U32(length);

    }
    else {
        response->put_U32(ROBOTV_RET_DATAUNKNOWN);
        ERRORLOG("%s - unable to start recording !", __FUNCTION__);
    }

    return true;
}

bool RecordingController::processClose(MsgPacket* request, MsgPacket* response) {
    if(m_recPlayer) {
        delete m_recPlayer;
        m_recPlayer = NULL;
    }

    response->put_U32(ROBOTV_RET_OK);
    return true;
}

bool RecordingController::processRequest(MsgPacket* request, MsgPacket* response) {
    if(!m_recPlayer) {
        return false;
    }

    bool keyFrameMode = request->get_U8();

    MsgPacket* p = m_recPlayer->requestPacket(keyFrameMode);

    if(p == NULL) {
        return true;
    }

    int packetLen = p->getPayloadLength();
    uint8_t* packetData = p->consume(packetLen);

    response->put_Blob(packetData, packetLen);
    delete p;

    return true;
}

bool RecordingController::processSeek(MsgPacket* request, MsgPacket* response) {
    if(m_recPlayer == NULL) {
        return false;
    }

    int64_t position = request->get_S64();
    int64_t pts = m_recPlayer->seek(position);

    response->put_S64(pts);
    return true;
}

bool RecordingController::processPause(MsgPacket* request, MsgPacket* response) {
    if(m_recPlayer == NULL) {
        return false;
    }

    return true;
}
