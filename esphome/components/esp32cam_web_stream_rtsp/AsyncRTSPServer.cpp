/**
 * This library is an adaptation of https://github.com/geeksville/Micro-RTSP
 * suited for use with the AsyncTCP library https://github.com/me-no-dev/AsyncTCP
 * JPEG over RTP packet format: https://datatracker.ietf.org/doc/html/rfc2435
 *
 */

#include "AsyncRTSP.h"
//#include "JPEGHelpers.h"

namespace esphome {
namespace esp32cam_web_stream_rtsp {

// Image header bytes
#define JEPG_StartOfImage 0xd8
#define JPEG_APP_0 0xe0
#define JPEG_DefineQuantizationTable 0xdb
#define JPEG_DefineHuffmanTable 0xc4
#define JPEG_StartOfScan 0xda
#define JPEG_StartBaselineDCTFrame 0xc0
#define JPEG_EndOfImage 0xd9

typedef unsigned char *BufPtr;

// most of the JPEG headers contain two bytes after the marker
// specifying the length of the header (including these length bits)
// this function moves the supplied pointer PAST the currently
// pointed-at header using this length description
// NOTE: not necessarily safe for all header types; use wisely.
void nextJpegBlock(BufPtr *bytes) {
  uint32_t len = (*bytes)[0] * 256 + (*bytes)[1];
  *bytes += len;
}

// TODO: Replace this function with one that returns a struct containing the details of the
// header it just found (including things like length, code, etc) so that
// we can remove duplicate logic later on (for things like figuring out the length of the SOS markers)

// search for a particular JPEG marker, moves *start to just after that marker
// This function fixes up the provided start ptr to point to the
// actual JPEG stream data and returns the number of bytes skipped

// AHHHAHHHHHH - BufPtr (which I replaced with unsigned char *) was used as a pointer-to-a-pointer here
// So this is not actually modifying the incoming pointer
bool findJPEGheader(BufPtr *start, uint32_t *len, char marker) {
  // per https://en.wikipedia.org/wiki/JPEG_File_Interchange_Format
  BufPtr bytes = *start;

  // kinda skanky, will break if unlucky and the headers inxlucde 0xffda
  // might fall off array if jpeg is invalid
  // FIXME - return false instead
  while (bytes - *start < *len) {
    char framing = *bytes++;  // better be 0xff since all of the JPEG_ header codes start with 0xff
    if (framing != 0xff) {
      // printf("malformed jpeg, framing=%x %p\n", framing, bytes);
      return false;
    }

    char typecode = *bytes++;
    if (typecode == marker) {
      char skipped = bytes - *start;
      // printf("found marker 0x%x at %p, skipped %d\n", marker, bytes, skipped);

      *start = bytes;

      // shrink len for the bytes we just skipped
      *len -= skipped;

      return true;
    } else {
      // not the section we were looking for, skip the entire section
      switch (typecode) {
        case JEPG_StartOfImage:  // start of image
        {
          break;  // no data to skip
        }
          // All of these JPEG headers contain two bytes following the marker
          // specifying the length of the segment
          // Use that defined length to skip the whole segment
          // and advance the bytes pointer to the next segment
        case JPEG_APP_0:                    // app0
        case JPEG_DefineQuantizationTable:  // dqt
        case JPEG_DefineHuffmanTable:       // dht
        case JPEG_StartBaselineDCTFrame:    // sof0
        case JPEG_StartOfScan:              // sos
        {
          nextJpegBlock(&bytes);
          break;
        }
        default:
          // printf("unexpected jpeg typecode 0x%x\n", typecode);
          break;
      }
    }
  }

  // printf("failed to find jpeg marker 0x%x", marker);
  return false;
}

// the scan data uses byte stuffing to guarantee anything that starts with 0xff
// followed by something not zero, is a new section.  Look for that marker and return the ptr
// pointing there
void skipScanBytes(BufPtr *start, uint32_t len) {
  unsigned char *bytes = *start;

  while (bytes - *start < len) {
    while (*bytes++ != 0xff)
      ;
    if (*bytes++ == 0xd9) {
      *start = bytes - 2;  // back up to the 0xff marker we just found
      return;
    }
  }
  // printf("could not find end of scan");
}

// When JPEG is stored as a file it is wrapped in a container
// This function fixes up the provided start ptr to point to the
// actual JPEG stream data and returns the number of bytes skipped
bool decodeJPEGfile(BufPtr *start, uint32_t *len, BufPtr *qtable0, BufPtr *qtable1) {
  // per https://en.wikipedia.org/wiki/JPEG_File_Interchange_Format
  unsigned char *bytes = *start;

  // move the start pointer to the beginning of the image (marker FFD8); fail if we can't.
  // Updates len to the number of bytes remaining in the buffer
  if (!findJPEGheader(&bytes, len, JEPG_StartOfImage))  // better at least look like a jpeg file
    return false;                                       // FAILED!

  // Look for quant tables if they are present
  *qtable0 = NULL;
  *qtable1 = NULL;
  BufPtr quantstart = *start;
  uint32_t quantlen = *len;
  // using the start pointer as a base, move the quantstart pointer to where quant tables
  // begin (marker FFDB)
  if (!findJPEGheader(&quantstart, &quantlen, JPEG_DefineQuantizationTable)) {
    // printf("error can't find quant table 0\n");
  } else {
    *qtable0 = quantstart + 3;  // 3 bytes of header skipped
    nextJpegBlock(&quantstart);
    if (!findJPEGheader(&quantstart, &quantlen, JPEG_DefineQuantizationTable)) {
      // printf("error can't find quant table 1\n");
    }
    *qtable1 = quantstart + 3;
    nextJpegBlock(&quantstart);
  }

  // move the start pointer (which was passed to this function) to the address of the start of
  // scan data (marker FFDA))
  if (!findJPEGheader(start, len, JPEG_StartOfScan))
    return false;  // FAILED!

  // Skip the header bytes of the SOS marker
  uint32_t soslen = (*start)[0] * 256 + (*start)[1];
  *start += soslen;
  *len -= soslen;

  // start scanning the data portion of the scan to find the end marker
  BufPtr endmarkerptr = *start;
  uint32_t endlen = *len;

  skipScanBytes(&endmarkerptr, *len);
  if (!findJPEGheader(&endmarkerptr, &endlen, JPEG_EndOfImage))
    return false;  // FAILED!

  // endlen must now be the # of bytes between the start of our scan and
  // the end marker, tell the caller to ignore bytes afterwards
  *len = endmarkerptr - *start;

  return true;
}

AsyncRTSPServer::AsyncRTSPServer(uint16_t port, dimensions dim) : _server(port), _dim(dim) {
  this->client = nullptr;
  this->prevMsec = millis();
  this->curMsec = this->prevMsec;

  this->RTPBuffer = new char[2048];  // Note: we assume single threaded, this large buf we keep off of the tiny stack

  _server.onClient(
      [this](void *s, AsyncClient *c) {
        AsyncRTSPServer *rtps = (AsyncRTSPServer *) s;

        rtps->client = new AsyncRTSPClient(c, this);
        rtps->connectCallback(rtps->that);
      },
      this);

  this->loggerCallback = NULL;
}

void AsyncRTSPServer::writeLog(String log) {
  if (this->loggerCallback != NULL) {
    this->loggerCallback(log);
  }
}

boolean AsyncRTSPServer::hasClients() { return this->client != nullptr && this->client->getIsCurrentlyStreaming(); }

void AsyncRTSPServer::pushFrame(uint8_t *data, size_t length) {
#define units 90000  // Hz per RFC 2435
  this->curMsec = millis();
  this->deltams = (this->curMsec >= this->prevMsec) ? this->curMsec - this->prevMsec : 100;
  this->m_Timestamp += (units * deltams / 1000);

  if (this->hasClients()) {
    // TODO: When we support multiple clients, this will end up being a nested loop
    // where the buffer is prepared once, and sent out to each client individually
    // for now, just do one.

    struct RTPBuffferPreparationResult bpr = {0, 0, false};

    unsigned char *quant0tbl;
    unsigned char *quant1tbl;

    if (!decodeJPEGfile(&data, &length, &quant0tbl, &quant1tbl)) {
      this->loggerCallback("Cannot decode JPEG Data");
      return;
    }

    // at this point, "data" points to the addres of the scan frames
    do {
      PrepareRTPBufferForClients(this->RTPBuffer, data, length, &bpr, quant0tbl, quant1tbl);
      this->client->PushRTPBuffer(this->RTPBuffer, bpr.bufferSize);
      yield();  // TODO: Not sure if this is necessary?
    } while (!bpr.isLastFragment);
    this->loggerCallback("RTSP server pushed camera frame");
  }
}

void AsyncRTSPServer::onClient(RTSPConnectHandler callback, void *that) {
  this->connectCallback = callback;
  this->that = that;
}

void AsyncRTSPServer::setLogFunction(LogFunction callback, void *that) {
  this->loggerCallback = callback;
  this->thatlog = that;
}

int AsyncRTSPServer::GetRTSPServerPort() { return this->RtpServerPort; }

int AsyncRTSPServer::GetRTCPServerPort() { return this->RtcpServerPort; }

void AsyncRTSPServer::PrepareRTPBufferForClients(char *RtpBuf, uint8_t *data, int length,
                                                 RTPBuffferPreparationResult *bpr, unsigned const char *quant0tbl,
                                                 unsigned const char *quant1tbl) {
#define KRtpHeaderSize 12  // size of the RTP header
#define KJpegHeaderSize 8  // size of the special JPEG payload header

#define MAX_FRAGMENT_SIZE 1100  // FIXME, pick more carefully
  int fragmentLen = MAX_FRAGMENT_SIZE;
  if (fragmentLen + bpr->offset >= length) {  // Shrink last fragment if needed
    fragmentLen =
        length - bpr->offset - 2;  // the JPEG end marker (FFD9) will be in this fragment.  drop it from the end
    bpr->isLastFragment = true;
  }

  // Do we have custom quant tables? If so include them per RFC

  bool includeQuantTbl = quant0tbl && quant1tbl && bpr->offset == 0;
  uint8_t q = includeQuantTbl ? 128 : 0x5e;

  bpr->bufferSize = 4 + fragmentLen + KRtpHeaderSize + KJpegHeaderSize + (includeQuantTbl ? (4 + 64 * 2) : 0);

  memset(RtpBuf, 0x00, sizeof(RtpBuf));
  // Prepare the first 4 byte of the packet. This is the Rtp over Rtsp header in case of TCP based transport
  RtpBuf[0] = '$';  // magic number
  RtpBuf[1] = 0;    // number of multiplexed subchannel on RTPS connection - here the RTP channel
  RtpBuf[2] = (bpr->bufferSize & 0x0000FF00) >> 8;
  RtpBuf[3] = (bpr->bufferSize & 0x000000FF);
  // Prepare the 12 byte RTP header
  RtpBuf[4] = 0x80;                                        // RTP version
  RtpBuf[5] = 0x1a | (bpr->isLastFragment ? 0x80 : 0x00);  // JPEG payload (26) and marker bit
  RtpBuf[7] = m_SequenceNumber & 0x0FF;                    // each packet is counted with a sequence counter
  RtpBuf[6] = m_SequenceNumber >> 8;
  RtpBuf[8] = (m_Timestamp & 0xFF000000) >> 24;  // each image gets a timestamp
  RtpBuf[9] = (m_Timestamp & 0x00FF0000) >> 16;
  RtpBuf[10] = (m_Timestamp & 0x0000FF00) >> 8;
  RtpBuf[11] = (m_Timestamp & 0x000000FF);
  RtpBuf[12] = 0x13;  // 4 byte SSRC (sychronization source identifier)
  RtpBuf[13] = 0xf9;  // we just an arbitrary number here to keep it simple
  RtpBuf[14] = 0x7e;
  RtpBuf[15] = 0x67;

  // Prepare the 8 byte payload JPEG header
  RtpBuf[16] = 0x00;                              // type specific
  RtpBuf[17] = (bpr->offset & 0x00FF0000) >> 16;  // 3 byte fragmentation offset for fragmented images
  RtpBuf[18] = (bpr->offset & 0x0000FF00) >> 8;
  RtpBuf[19] = (bpr->offset & 0x000000FF);

  /*    These sampling factors indicate that the chrominance components of
     type 0 video is downsampled horizontally by 2 (often called 4:2:2)
     while the chrominance components of type 1 video are downsampled both
     horizontally and vertically by 2 (often called 4:2:0). */
  RtpBuf[20] = 0x00;                  // type (fixme might be wrong for camera data) https://tools.ietf.org/html/rfc2435
  RtpBuf[21] = q;                     // quality scale factor was 0x5e
  RtpBuf[22] = this->_dim.width / 8;  // width  / 8
  RtpBuf[23] = this->_dim.height / 8;  // height / 8

  int headerLen = 24;     // Inlcuding jpeg header but not qant table header
  if (includeQuantTbl) {  // we need a quant header - but only in first packet of the frame
    RtpBuf[24] = 0;       // MBZ
    RtpBuf[25] = 0;       // 8 bit precision
    RtpBuf[26] = 0;       // MSB of lentgh

    int numQantBytes = 64;          // Two 64 byte tables
    RtpBuf[27] = 2 * numQantBytes;  // LSB of length

    headerLen += 4;

    memcpy(RtpBuf + headerLen, quant0tbl, numQantBytes);
    headerLen += numQantBytes;

    memcpy(RtpBuf + headerLen, quant1tbl, numQantBytes);
    headerLen += numQantBytes;
  }

  // append the JPEG scan data to the RTP buffer
  memcpy(RtpBuf + headerLen, data + bpr->offset, fragmentLen);
  bpr->offset += fragmentLen;

  m_SequenceNumber++;  // prepare the packet counter for the next packet
}

void AsyncRTSPServer::begin() {
  _server.setNoDelay(true);
  _server.begin();
}

}  // namespace esp32cam_web_stream_rtsp
}  // namespace esphome