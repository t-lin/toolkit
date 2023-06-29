#ifndef MPLEX_MSG_GROUP_H
#define MPLEX_MSG_GROUP_H

// C headers
#include <stdint.h>
#include <time.h>
#include <endian.h>

// C++ headers
#include <iostream>
#include <memory>
#include <exception>
#include <type_traits>

// libtins
#include <tins/tins.h>
#include <tins/small_uint.h>

#include "../crc/crc.hpp"
#include "mplex_msg_frame.hpp"
#include "headers/group_headers.hpp"

// MplexMsgGroup class
// Encapsulates group of MplexMsgFrames
template <typename MsgGroupHeader, typename MsgFrameHeader>
class MplexMsgGroup {
  public:
    // Header-dependent types
    typedef Tins::small_uint<MsgGroupHeader::NUM_FRAMES_WIDTH> nFrames_t;
    typedef Tins::small_uint<MsgGroupHeader::HLEN_WIDTH> hlen_t;
    typedef Tins::small_uint<MsgGroupHeader::HCRC_WIDTH> hcrc_t;

    // Frame-dependent type
    typedef MplexMsgFrame<MsgFrameHeader> msg_t;

  private:
    // NOTE (t-lin): Due to compilation ordering issues, we can't use the
    //               expression 'msg_t::MAX_SIZE' directly in the definitions
    //               below w/ optimization level O0. Thus, pre-compute it here.
    constexpr static const uint16_t FRAME_MAX_SIZE = msg_t::MAX_SIZE;

    std::unique_ptr<uint8_t> rawBuf_ = nullptr;
    uint16_t rawBufSize_ = 0;

    MsgGroupHeader header_;

    // Internal MsgFrame instance and a raw pointer to its buffer location
    mutable msg_t currFrame_;
    uint8_t* currFramePos_ = nullptr;

    // Number of frames processed (i.e. written)
    nFrames_t nFramesProcessed_ = 0;

    // Operational mode
    MplexOpMode::opmode mode_ = MplexOpMode::WRITE;

    /*
     * Calls CRC() w/ an initial value based on the CRC bit-width. This is
     * to prevent 0 from being the default CRC and prevents arbitrary-length
     * all-0 data from having the same CRC.
     */
    hcrc_t calcCRC_(const uint8_t* buf, uint64_t sz) const {
      hcrc_t init = CRC_INIT & hcrc_t::max_value;
      return CRCUtils::CRC<MsgGroupHeader::HCRC_WIDTH>(buf, sz, init);
    }

    /**
     * @brief Common routine for constructors. Responsible for allocating
     *        the underlying buffer and resetting the frame position.
     *
     * @param bufSize Size of buffer to allocate.
     */
    void resetBuffer_(const uint16_t bufSize) {
      rawBuf_.reset(new uint8_t[bufSize]);
      if (rawBuf_.get() == nullptr) {
        throw std::bad_alloc();
      }

      rawBufSize_ = bufSize;
      currFramePos_ = rawBuf_.get() + sizeof(MsgGroupHeader);
    }

    /**
     * @brief Returns the buffer size remaining that hasn't been read from
     *        or written to.
     *
     * @return Size of remaining buffer space that hasn't been processed.
     */
    uint16_t unprocessedSz_() {
      return (uint16_t)((rawBuf_.get() + rawBufSize_) - currFramePos_);
    }

  public:
    // Useful header-dependent constants
    static const vers_t VERS = MsgGroupHeader::VERS;

    // Minimum size of a valid Message Group w/ no Message Frames
    static const uint16_t MIN_SIZE = sizeof(MsgGroupHeader) +
                                     sizeof(MsgFrameHeader); // Header + EoMG

    // Bound max size to the maximum value of a 16-bit integer. We choose
    // this primarily for the assumption that this protocol will be used
    // to send packets over a TCP/IP network, where TCP & UDP packets sizes
    // are also bounded to 16-bits (i.e. 65535).
    static const uint16_t MAX_SIZE = Tins::small_uint<16>::max_value;

    /**
     * @brief Constructor for a MsgGroup of a given version.
     *        Will automatically allocate a buffer for the MsgFrames, whose
     *        size will be the maximum of a uint16_t (i.e. 65535).
     *
     *        Note: Using this method puts the object in WRITE mode.
     */
    MplexMsgGroup() {
      // Pre-allocate maximum possible buffer size.
      resetBuffer_(MAX_SIZE);
      mode_ = MplexOpMode::WRITE;

      uint16_t maxFrameSz = std::min(unprocessedSz_(), FRAME_MAX_SIZE);
      if (currFrame_.reset(currFramePos_, maxFrameSz, mode_) == false) {
        throw std::logic_error("Unable to set currFrame object");
      }
    }

    /**
     * @brief Constructor for a MsgGroup of a given version, with a given size
     *        for the underlying buffer.
     *
     *        Note: Using this method puts the object in WRITE mode.
     *
     * @param sz Requested size of the underying buffer to allocate.
     */
    MplexMsgGroup(const uint16_t sz) {
      if (sz < MIN_SIZE) {
        throw std::invalid_argument("Requested buffer size < minimum size");
      } else if (sz > MAX_SIZE) {
        throw std::invalid_argument("Requested buffer size > maximum size");
      }

      resetBuffer_(sz);
      mode_ = MplexOpMode::WRITE;

      uint16_t maxFrameSz = std::min(unprocessedSz_(), FRAME_MAX_SIZE);
      if (currFrame_.reset(currFramePos_, maxFrameSz, mode_) == false) {
        throw std::logic_error("Unable to set currFrame object");
      }
    }

    /**
     * @brief Constructor for a MsgGroup by loading an existing buffer.
     *        The buffer is assumed to contain a serialzied The MsgGroup object
     *        and its contents will copied to the MsgGroup object's underlying
     *        buffer, so the user is free to discard it.
     *
     *        Note: Using this method puts the object in READ mode.
     *
     * @param rawBuf Pointer to the existing buffer.
     * @param sz Size of the existing buffer.
     */
    MplexMsgGroup(const uint8_t* rawBuf, const uint16_t sz) {
      if (sz < MIN_SIZE) {
        throw std::invalid_argument("Requested buffer size < header size");
      } else if (sz > MAX_SIZE) {
        throw std::invalid_argument("Requested buffer size > maximum size");
      }

      if (rawBuf == nullptr) {
        throw std::invalid_argument(
            "Cannot construct MsgGroup with an empty buffer");
      }

      // Allocate & copy buffer contents, then load its header
      resetBuffer_(sz);
      memcpy((void*)rawBuf_.get(), (void*)rawBuf, sz);
      memcpy((void*)&header_, (void*)rawBuf, sizeof(MsgGroupHeader));

      mode_ = MplexOpMode::READ;

      uint16_t maxFrameSz = std::min(unprocessedSz_(), FRAME_MAX_SIZE);
      if (currFrame_.reset(currFramePos_, maxFrameSz, mode_) == false) {
        throw std::logic_error("Unable to set currFrame object");
      }
    }

    /**
     * @brief Reset the Message Group, as if it was just created in WRITE mode.
     *        Note that this operation will also wipe the header.
     *
     * @return Returns true on success, false if something went wrong.
     */
    bool reset() {
      mode_ = MplexOpMode::WRITE;
      currFramePos_ = rawBuf_.get() + sizeof(MsgGroupHeader);
      uint16_t maxFrameSz = std::min(unprocessedSz_(), FRAME_MAX_SIZE);
      if (currFrame_.reset(currFramePos_, maxFrameSz, mode_) == false) {
        // TODO: Use log
        std::cerr << "ERROR: Unable to reset currFrame object\n";
        return false;
      }
      nFramesProcessed_ = 0;
      memset((void*)&header_, 0x0, sizeof(MsgGroupHeader));

      return true;
    }

    // TODO: Create copy/assignment constructor (due to unique_ptr buffer)

    /**
     * @brief Returns pointer to current frame to be processed.
     *
     * @return Pointer to the current frame in the MsgGroup.
     */
    msg_t* currFrame() const {
      // TODO: stricter error checking
      // TODO: return shared_ptr?
      return &currFrame_;
    }

    /**
     * @brief Scans the underlying buffer, after the position of the current
     *        frame, to find the next valid frame (i.e. isValid() returns
     *        true) and returns a pointer to it.
     *
     *        NOTE: This method is only valid in READ mode.
     *
     * @return Returns a pointer to the next valid frame in the underlying
     *         buffer.
     *         Returns NULL if any of the following are true:
     *          - If the end of the underlying buffer or an End of Message Group
     *            Frame is encountered, before a valid frame is found.
     *          - If the object is not in READ mode.
     *
     */
    msg_t* nextValidFrame() {
      if (mode_ != MplexOpMode::READ) {
        return nullptr;
      }

      // Increment current frame pointer to location of next potental frame.
      if (currFrame_.isValid()) {
        currFramePos_ += currFrame_.msgSize();
      } else {
        currFramePos_++;
      }

      // Scan through buffer to find potential new frame.
      const uint8_t* bufEnd = rawBuf_.get() + rawBufSize_;
      for (; currFramePos_ < bufEnd; currFramePos_++) {
        if (*currFramePos_ != MsgFrameHeader::MAGIC_NUMBER) {
          continue;
        }

        // Check if the leftover space can't fit a new frame.
        uint16_t remainSz = std::min(unprocessedSz_(), FRAME_MAX_SIZE);
        if (remainSz < sizeof(MsgFrameHeader)) {
          return nullptr;
        }

        if (currFrame_.reset(currFramePos_, remainSz, mode_) == false) {
          // TODO: Use log
          std::cerr << "Unable to reset currFrame object" << std::endl;
          return nullptr;
        }

        // Check if we've reached an End of Message Group frame
        if (currFrame_.isEndOfMsgGroup()) {
          return nullptr;
        }

        if (currFrame_.isValid()) {
          return &currFrame_;
        }
      }

      return nullptr;
    }

    /**
     * @brief This method should only be invoked when the current message
     *        frame is valid.
     *
     *        Commits the current frame by incrementing the internal frame
     *        pointer to the location immediately proceeding the current frame.
     *        Internally, this will use the current frame's msgSize() method
     *        to determine how far to increment the pointer. A pointer to a
     *        Message Frame, with its underlying buffer starting at this new
     *        location, will be returned.
     *
     *        NOTE: This method is only valid in WRITE mode.
     *
     * @return Returns a pointer to a Message Frame object, whose underlying
     *         buffer is set to the location immediately proceeding the current
     *         frame.
     *
     *         Returns NULL if any of the following are true:
     *          - If the maximum number of frames in the Message Group has been
     *            reached, or if there is insufficient space left in the buffer
     *            for an End of Message Group frame.
     *          - If the object is not in WRITE mode.
     */
    msg_t* commitFrame() {
      if (mode_ != MplexOpMode::WRITE) {
        return nullptr;
      }

      if (currFrame_.isValid() == false) {
        // TODO: use log
        std::cerr << "Current frame not valid, cannot step" << std::endl;
        return nullptr;
      }

      nFramesProcessed_ =
          static_cast<typename nFrames_t::repr_type>(nFramesProcessed_ + 1);
      currFramePos_ += currFrame_.msgSize();

      // Check if the leftover space can't fit a new frame.
      uint16_t remainSz = std::min(unprocessedSz_(), FRAME_MAX_SIZE);
      if (remainSz < sizeof(MsgFrameHeader)) {
          return nullptr;
      }

      if (currFrame_.reset(currFramePos_, remainSz, mode_) == false) {
        // TODO: Use log
        std::cerr << "Unable to reset currFrame object" << std::endl;
        return nullptr;
      }

      return &currFrame_;
    }

    /**
     * @brief Updates & writes the header to the beginning of the underlying
     *        buffer, as well as an End of Message Group frame to location of
     *        the current frame pointer.
     *
     *        NOTE: This method is only valid in WRITE mode.
     *
     * @return Returns true if the header & End of Message Group trailer frame
     *         has been successfully written to the underlying buffer.
     *
     *         Returns false if an error occurred, or if the object is not
     *         in WRITE mode.
     */
    bool writeHeaderTrailer() {
      if (mode_ != MplexOpMode::WRITE) {
        return false;
      }

      // Write an end-of-frame message at current frame pointer.
      // Don't increment currFrame_ afterwards, in case this fails and/or
      // we decide to update and/or write more MsgFrames.
      MsgFrameHeader endOfGroup;
      memcpy((void*)currFramePos_, (void*)&endOfGroup, sizeof(endOfGroup));

      // Fill header contents
      struct timespec ts;
      timespec_get(&ts, TIME_UTC);
      header_.setMagic();
      header_.timestamp((uint32_t)ts.tv_sec, (uint32_t)ts.tv_nsec);
      header_.headerLen(sizeof(MsgGroupHeader)); // Ignore options for now
      header_.numFrames(nFramesProcessed_);

      // The CRC field in the header is calculated over just the header.
      // Set the CRC to 0 before calculating CRC.
      header_.hcrc(0);
      try {
        header_.hcrc(calcCRC_(reinterpret_cast<const uint8_t*>(&header_),
                              sizeof(MsgGroupHeader)));
      } catch (std::exception& exc) {
        // TODO: Replace w/ log
        std::cerr << exc.what() << std::endl;
        return false;
      }
      memcpy((void*)rawBuf_.get(), (void*)&header_, sizeof(MsgGroupHeader));

      return true;
    }

    /**
     * @brief Get pointer to underlying buffer. This pointer will be invalid
     *        once this object goes out-of-scope or is manually destroyed.
     *
     * @return Returns pointer to underlying buffer.
     */
    uint8_t* getBuf() const {
      return rawBuf_.get();
    }

    /**
     * @brief Checks to see if the Msg Group header is valid.
     *
     * @return Returns true if the header is valid (i.e. magic number and
     *         calculated CRC matches), false otherwise.
     */
    bool headerIsValid() {
      // Set header CRC to 0 before calculating CRC, then restore
      MsgGroupHeader* pHead = (MsgGroupHeader*)rawBuf_.get();
      hcrc_t origCRC = pHead->hcrc();
      pHead->hcrc(0);
      hcrc_t calcCRC = 0;
      try {
        calcCRC = calcCRC_(rawBuf_.get(), sizeof(MsgGroupHeader));
      } catch (std::exception& exc) {
        // TODO: Replace /w log
        std::cerr << exc.what() << std::endl;
        pHead->hcrc(origCRC);
        return false;
      }
      pHead->hcrc(origCRC);

      return (MsgGroupHeader::MAGIC_NUMBER == header_.magic() &&
              calcCRC == header_.hcrc());
    }

    /**
     * @brief Calculates & returns the total size of the Message Group, taking
     * into account potential bit-flips in the underlying buffer. As a pre-
     * requisite, this method requires the header to be valid.
     *
     * To account for possible bit-flips, the size is conservatively calculated
     * by linearly processing the underlying buffer until either the number of
     * frames (indicated in the header) has been seen, until an End of Message
     * Group frame has been reached, or until the end of the buffer is reached,
     * whichever comes first. Thus, this method may undercalculate the size of
     * severely corrupted Message Groups.
     *
     * This operation is O(n) with respect to the size of the underlying buffer.
     * It is ideally used by processes that receive/read Message Groups.
     *
     * @return Returns the calculated total size of the message group including
     * the Message Group Header and End of Message Group frame.
     * Returns 0 if the group header is not in a valid state.
     */
    uint16_t calcGroupSize() {
      if (this->headerIsValid() == false) {
        return 0;
      }

      msg_t tmpMsg;
      uint16_t maxFrameSz = 0;
      nFrames_t validFrames = 0;
      uint8_t* ptr = rawBuf_.get() + sizeof(MsgGroupHeader);
      uint8_t* endOfBuf = rawBuf_.get() + rawBufSize_;

      // Stop search if end of buffer reached or if number of frames found.
      // Leave room before end of buffer for an End of Message Group frame.
      while (validFrames < this->numFrames() &&
             ptr < (endOfBuf - sizeof(MsgFrameHeader))) {
        if (*ptr != MsgFrameHeader::MAGIC_NUMBER) {
          ptr++;
          continue;
        }

        maxFrameSz = std::min(static_cast<uint16_t>(endOfBuf - ptr),
                              FRAME_MAX_SIZE);
        tmpMsg.reset(ptr, maxFrameSz);
        if (tmpMsg.isEndOfMsgGroup()) {
          break;
        }

        if (tmpMsg.isValid()) {
          validFrames =
              static_cast<typename nFrames_t::repr_type>(validFrames + 1);
          ptr += tmpMsg.msgSize();
        } else {
          // Resume scanning buffer to find next frame
          ptr++;
        }
      }

      // Calculate size traversed by 'ptr' and add size for EoMG frame
      return static_cast<uint16_t>(
          static_cast<uint16_t>(ptr - rawBuf_.get()) + sizeof(MsgFrameHeader));
    }

    /**
     * @brief Returns the size of the underlying buffer that has been processed
     * (i.e. read or written) so far. Includes the sizes of both the Message
     * Group Header (validity ignored) and the End of Message Group frame.
     *
     * Internally, this is simply the length between the start of the underlying
     * buffer and the current frame position pointer, plus the size of an End of
     * Message Group frame.
     *
     * @return Total size of all Message Frames up to but not including the
     * current frame.
     */
    uint16_t processedSize() const {
      return static_cast<uint16_t>(
          static_cast<uint16_t>(currFramePos_ - rawBuf_.get()) +
                                          sizeof(MsgFrameHeader));
    }

#ifdef DEBUG
    void printGroup() {
      uint8_t cnt = 0;
      for (const uint8_t* i = rawBuf_.get();
            i < rawBuf_.get() + this->calcGroupSize(); i++) {
        printf("%02X ", *i);

        if (++cnt == 16) {
          cnt = 0;
          printf("\n");
        }
      }
      printf("\n\n");
    }
#endif

    /**
     * @brief Returns the number of frames in this Message Group.
     *
     * @return Returns the number of frames in this Message Group.
     */
    nFrames_t numFrames() const {
      return header_.numFrames();
    }

    /**
     * @brief Returns the length of the Message Group Header.
     *
     * @return Returns the length of the Message Group Header.
     */
    hlen_t hlen() const {
      return header_.hlen();
    }

    /**
     * @brief Returns the CRC of the Message Group Header.
     *
     * @return Returns the CRC of the Message Group Header.
     */
    hcrc_t hcrc() const {
      return header_.hcrc();
    }
};

#endif
