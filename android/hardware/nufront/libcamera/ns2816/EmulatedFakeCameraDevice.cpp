/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Contains implementation of a class EmulatedFakeCameraDevice that encapsulates
 * fake camera device.
 */

#define LOG_NDEBUG 0
#define LOG_TAG "EmulatedCamera_FakeDevice"
#include <cutils/log.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include "EmulatedFakeCamera.h"
#include "EmulatedFakeCameraDevice.h"

#include "Utils.h"

namespace android {

EmulatedFakeCameraDevice::EmulatedFakeCameraDevice(EmulatedFakeCamera* camera_hal)
    : EmulatedCameraDevice(camera_hal),
      m_cam_fd(-1),
      mBlackYUV(kBlack32),
      mWhiteYUV(kWhite32),
      mRedYUV(kRed8),
      mGreenYUV(kGreen8),
      mBlueYUV(kBlue8),
      mLastRedrawn(0),
      mCheckX(0),
      mCheckY(0),
      mCcounter(0)
#if EFCD_ROTATE_FRAME
      , mLastRotatedAt(0),
        mCurrentFrameType(0),
        mCurrentColor(&mWhiteYUV)
#endif  // EFCD_ROTATE_FRAME
{
    // Makes the image with the original exposure compensation darker.
    // So the effects of changing the exposure compensation can be seen.
    mBlackYUV.Y = mBlackYUV.Y / 4;
    mWhiteYUV.Y = mWhiteYUV.Y / 4;
    mRedYUV.Y = mRedYUV.Y / 4;
    mGreenYUV.Y = mGreenYUV.Y / 4;
    mBlueYUV.Y = mBlueYUV.Y / 4;

    memset(&m_capture_buf, 0, sizeof(m_capture_buf));
}

EmulatedFakeCameraDevice::~EmulatedFakeCameraDevice()
{
}

int EmulatedFakeCameraDevice::powerOnNuCamera()
{
    LOGV("%s :", __func__);
    int ret = 0;

#define CAMERA_POWER_ON_BY_EC

#ifdef CAMERA_POWER_ON_BY_EC
    LOGV("powerOnNuCamera(),NOTICE: power on by camera HAL");
    ret = power_on_camera();
    if (ret < 0) {
        LOGE("power_on_camera() failed.%s(%d)",strerror(errno),errno);
        return -1;
    }
    LOGV("@TEI Open EC Power device success!");

    //sleep 1.5 second to wait input-subsystem to detect camera device
    usleep(WAIT_POWER_ON_TIME);
#endif

//        setExifFixedAttribute();
    LOGI("%s : initialized", __FUNCTION__);
    return 0;
}

int EmulatedFakeCameraDevice::initNuCamera()
{
    LOGV("%s :", __func__);
    int ret = 0;
    int on = 0;

#define CAMERA_POWER_ON_BY_EC

#ifdef CAMERA_POWER_ON_BY_EC
        for(int j = 0;j < WAIT_POWER_ON_TRY_TIMES; j++){
            if ((m_cam_fd = open(CAMERA_DEV_NAME, O_RDWR)) < 0) {
              usleep(WAIT_POWER_ON_TIME_TRY);
              continue;
            }
            on = 1;
            break;
        }
        if (!on) {
            LOGE ("ERROR: open camera device(%s) failed! cause:%s(%d) \n",CAMERA_DEV_NAME,strerror(errno),errno);
            return -1;
        }
#else
        if ((m_cam_fd = open(CAMERA_DEV_NAME, O_RDWR)) < 0) {
            LOGE ("ERROR: open camera device(%s) failed! cause:%s(%d) \n",CAMERA_DEV_NAME,strerror(errno),errno);
            return -1;
        }
#endif
        LOGV("%s: open(%s) --> m_cam_fd %d", __FUNCTION__, CAMERA_DEV_NAME, m_cam_fd);


        ret = fimc_v4l2_querycap(m_cam_fd);
        CHECK(ret);

    /* enum_fmt, s_fmt sample */
    ret = fimc_v4l2_enum_fmt(m_cam_fd,V4L2_PIX_FMT_YUYV);
    CHECK(ret);
    ret = fimc_v4l2_s_fmt(m_cam_fd, mFrameWidth, mFrameHeight, V4L2_PIX_FMT_YUYV, 0);
    CHECK(ret);
    ret = fimc_v4l2_reqbufs(m_cam_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, MAX_BUFFERS);
    CHECK(ret);

    for (int i = 0; i < MAX_BUFFERS; i++) {
        ret = fimc_v4l2_querybuf(m_cam_fd, &m_capture_buf[i], V4L2_BUF_TYPE_VIDEO_CAPTURE, i);
        CHECK(ret);
    }


    /* start with all buffers in queue */
    for (int i = 0; i < MAX_BUFFERS; i++) {
        ret = fimc_v4l2_qbuf(m_cam_fd, i);
        CHECK(ret);
    }
    ret = fimc_v4l2_streamon(m_cam_fd);
    CHECK(ret);


//        setExifFixedAttribute();

        LOGI("%s : initialized", __FUNCTION__);
    return 0;
}
/****************************************************************************
 * Emulated camera device abstract interface implementation.
 ***************************************************************************/

status_t EmulatedFakeCameraDevice::connectDevice()
{
    LOGV("%s", __FUNCTION__);

    Mutex::Autolock locker(&mObjectLock);
    if (!isInitialized()) {
        LOGE("%s: Fake camera device is not initialized.", __FUNCTION__);
        return EINVAL;
    }
    if (isConnected()) {
        LOGW("%s: Fake camera device is already connected.", __FUNCTION__);
        return NO_ERROR;
    }

    if (powerOnNuCamera() < 0) {
        LOGE("%s: powerOnCamera() failed.", __FUNCTION__);
        return EINVAL;
    }

    /* There is no device to connect to. */
    mState = ECDS_CONNECTED;

    return NO_ERROR;
}

status_t EmulatedFakeCameraDevice::disconnectDevice()
{
    LOGV("%s", __FUNCTION__);

    Mutex::Autolock locker(&mObjectLock);
    if (!isConnected()) {
        LOGW("%s: Fake camera device is already disconnected.", __FUNCTION__);
        return NO_ERROR;
    }
    if (isStarted()) {
        LOGE("%s: Cannot disconnect from the started device.", __FUNCTION__);
        return EINVAL;
    }

    power_off_camera();

    /* There is no device to disconnect from. */
    mState = ECDS_INITIALIZED;

    return NO_ERROR;
}

status_t EmulatedFakeCameraDevice::startDevice(int width,
                                               int height,
                                               uint32_t pix_fmt,
                                               int effectType)
{
    LOGV("%s", __FUNCTION__);

    Mutex::Autolock locker(&mObjectLock);
    if (!isConnected()) {
        LOGE("%s: Fake camera device is not connected.", __FUNCTION__);
        return EINVAL;
    }
    if (isStarted()) {
        LOGE("%s: Fake camera device is already started.", __FUNCTION__);
        return EINVAL;
    }

    /* Initialize the base class. */
    const status_t res = EmulatedCameraDevice::commonStartDevice(width, height, pix_fmt, effectType);
    if (res == NO_ERROR) {
        /* Calculate U/V panes inside the framebuffer. */
        switch (mPixelFormat) {
            case V4L2_PIX_FMT_YVU420:
                mFrameV = mCurrentFrame + mTotalPixels;
                mFrameU = mFrameU + mTotalPixels / 4;
                mUVStep = 1;
                mUVTotalNum = mTotalPixels / 4;
                break;

            case V4L2_PIX_FMT_YUV420:
                mFrameU = mCurrentFrame + mTotalPixels;
                mFrameV = mFrameU + mTotalPixels / 4;
                mUVStep = 1;
                mUVTotalNum = mTotalPixels / 4;
                break;

            case V4L2_PIX_FMT_NV21:
                /* Interleaved UV pane, V first. */
                mFrameV = mCurrentFrame + mTotalPixels;
                mFrameU = mFrameV + 1;
                mUVStep = 2;
                mUVTotalNum = mTotalPixels / 4;
                break;

            case V4L2_PIX_FMT_NV12:
                /* Interleaved UV pane, U first. */
                mFrameU = mCurrentFrame + mTotalPixels;
                mFrameV = mFrameU + 1;
                mUVStep = 2;
                mUVTotalNum = mTotalPixels / 4;
                break;

            case V4L2_PIX_FMT_YUYV:
                /* Packed. */
                break;

            default:
                LOGE("%s: Unknown pixel format %.4s", __FUNCTION__,
                     reinterpret_cast<const char*>(&mPixelFormat));
                return EINVAL;
        }

    int ret = initNuCamera();
    CHECK(ret);

        /* Number of items in a single row inside U/V panes. */
        mUVInRow = (width / 2) * mUVStep;
        mState = ECDS_STARTED;
    } else {
        LOGE("%s: commonStartDevice failed", __FUNCTION__);
    }

    return res;
}

status_t EmulatedFakeCameraDevice::stopDevice()
{
    LOGV("%s", __FUNCTION__);

    Mutex::Autolock locker(&mObjectLock);
    if (!isStarted()) {
        LOGW("%s: Fake camera device is not started.", __FUNCTION__);
        return NO_ERROR;
    }

    mFrameU = mFrameV = NULL;
    EmulatedCameraDevice::commonStopDevice();

    LOGI("%s :", __func__);
    for (int i = 0; i < MAX_BUFFERS; i++) {
        if (m_capture_buf[i].start) {
            munmap(m_capture_buf[i].start, m_capture_buf[i].length);
            LOGI("munmap():virt. addr %p size = %d\n",
                 m_capture_buf[i].start, m_capture_buf[i].length);
            m_capture_buf[i].start = NULL;
            m_capture_buf[i].length = 0;
        }
    }

    if (m_cam_fd <= 0) {
        LOGE("ERR(%s):Camera was closed\n", __func__);
        return -1;
    }

    fimc_v4l2_streamoff(m_cam_fd);

    close(m_cam_fd);
    m_cam_fd = -1;

    mState = ECDS_CONNECTED;

    return NO_ERROR;
}

/****************************************************************************
 * Worker thread management overrides.
 ***************************************************************************/

bool EmulatedFakeCameraDevice::inWorkerThread()
{
    /* Wait till FPS timeout expires, or thread exit message is received. */
    WorkerThread::SelectRes res =
        getWorkerThread()->Select(-1, 1000000 / mEmulatedFPS);
    if (res == WorkerThread::EXIT_THREAD) {
        LOGV("%s: Worker thread has been terminated.", __FUNCTION__);
        return false;
    }

    /* Lets see if we need to generate a new frame. */
    if ((systemTime(SYSTEM_TIME_MONOTONIC) - mLastRedrawn) >= mRedrawAfter) {
        /*
         * Time to generate a new frame.
         */

#if EFCD_ROTATE_FRAME
        const int frame_type = rotateFrame();
        switch (frame_type) {
            case 0:
                drawCheckerboard();
                break;
            case 1:
                drawStripes();
                break;
            case 2:
                drawSolid(mCurrentColor);
                break;
        }
#else
        /* Draw the checker board. */
        //drawCheckerboard();
        int ret = grabFrame();
        if (ret < 0) {
            return false;
        }

#endif  // EFCD_ROTATE_FRAME

        mLastRedrawn = systemTime(SYSTEM_TIME_MONOTONIC);
    }

    /* Timestamp the current frame, and notify the camera HAL about new frame. */
    mCurFrameTimestamp = systemTime(SYSTEM_TIME_MONOTONIC);
    //LOGV("EmulatedFakeCameraDevice::inWorkerThread(), call mCameraHAL->onNextFrameAvailable");
    mCameraHAL->onNextFrameAvailable(mCurrentFrame, mCurFrameTimestamp, this);
    return true;
}

int EmulatedFakeCameraDevice::grabFrame()
{
    int index = fimc_v4l2_dqbuf(m_cam_fd);
    if (!(0 <= index && index < MAX_BUFFERS)) {
        LOGE("ERR(%s):wrong index = %d\n", __func__, index);
        return -1;
    }

    //LOGV("%s: donglongjiang@TEI VIDIOC_DQBUF index = %d, mFrameWidth = %d, mFrameHeight = %d, mCcounter = %d.", __FUNCTION__, index, mFrameWidth, mFrameHeight, mCcounter);
    /*
    if (mCcounter < 3) {
        dumpYUYV(m_capture_buf[index].start, mFrameWidth, mFrameHeight, mCcounter);
    }
    */
    yuyv422ToYuv420((unsigned char *)(m_capture_buf[index].start));
    //LOGV("%s: donglongjiang@TEI passed yuyv422_to_yuv420sp()", __FUNCTION__);


    int ret = fimc_v4l2_qbuf(m_cam_fd, index);
    CHECK(ret);

    mCcounter++;
    return 0;
}

void EmulatedFakeCameraDevice::yuyv422ToYuv420(unsigned char* src) {
    uint8_t* Y = mCurrentFrame;
    uint8_t* U_pos = mFrameU;
    uint8_t* V_pos = mFrameV;
    uint8_t* U = U_pos;
    uint8_t* V = V_pos;
    uint8_t* src_pos = src;

    for (int y = 0; y < mFrameHeight; y++) {
        for (int x = 0; x < mFrameWidth; x += 2) {
            *Y++ = *src_pos++;
            *U = *src_pos++;
            *Y++ = *src_pos++;
            *V = *src_pos++;
            U += mUVStep; V += mUVStep;
        }
        if (y & 0x1) {
            U_pos = U;
            V_pos = V;
        } else {
            U = U_pos;
            V = V_pos;
        }
    }
}

/****************************************************************************
 * Fake camera device private API
 ***************************************************************************/

void EmulatedFakeCameraDevice::drawCheckerboard()
{
    const int size = mFrameWidth / 10;
    bool black = true;

    if((mCheckX / size) & 1)
        black = false;
    if((mCheckY / size) & 1)
        black = !black;

    int county = mCheckY % size;
    int checkxremainder = mCheckX % size;
    uint8_t* Y = mCurrentFrame;
    uint8_t* U_pos = mFrameU;
    uint8_t* V_pos = mFrameV;
    uint8_t* U = U_pos;
    uint8_t* V = V_pos;

    for(int y = 0; y < mFrameHeight; y++) {
        int countx = checkxremainder;
        bool current = black;
        for(int x = 0; x < mFrameWidth; x += 2) {
            if (current) {
                mBlackYUV.get(Y, U, V);
            } else {
                mWhiteYUV.get(Y, U, V);
            }
            *Y = changeExposure(*Y);
            Y[1] = *Y;
            Y += 2; U += mUVStep; V += mUVStep;
            countx += 2;
            if(countx >= size) {
                countx = 0;
                current = !current;
            }
        }
        if (y & 0x1) {
            U_pos = U;
            V_pos = V;
        } else {
            U = U_pos;
            V = V_pos;
        }
        if(county++ >= size) {
            county = 0;
            black = !black;
        }
    }
    mCheckX += 3;
    mCheckY++;

    /* Run the square. */
    int sqx = ((mCcounter * 3) & 255);
    if(sqx > 128) sqx = 255 - sqx;
    int sqy = ((mCcounter * 5) & 255);
    if(sqy > 128) sqy = 255 - sqy;
    const int sqsize = mFrameWidth / 10;
    drawSquare(sqx * sqsize / 32, sqy * sqsize / 32, (sqsize * 5) >> 1,
               (mCcounter & 0x100) ? &mRedYUV : &mGreenYUV);
    mCcounter++;
}

void EmulatedFakeCameraDevice::drawSquare(int x,
                                          int y,
                                          int size,
                                          const YUVPixel* color)
{
    const int square_xstop = min(mFrameWidth, x + size);
    const int square_ystop = min(mFrameHeight, y + size);
    uint8_t* Y_pos = mCurrentFrame + y * mFrameWidth + x;

    // Draw the square.
    for (; y < square_ystop; y++) {
        const int iUV = (y / 2) * mUVInRow + (x / 2) * mUVStep;
        uint8_t* sqU = mFrameU + iUV;
        uint8_t* sqV = mFrameV + iUV;
        uint8_t* sqY = Y_pos;
        for (int i = x; i < square_xstop; i += 2) {
            color->get(sqY, sqU, sqV);
            *sqY = changeExposure(*sqY);
            sqY[1] = *sqY;
            sqY += 2; sqU += mUVStep; sqV += mUVStep;
        }
        Y_pos += mFrameWidth;
    }
}

#if EFCD_ROTATE_FRAME

void EmulatedFakeCameraDevice::drawSolid(YUVPixel* color)
{
    /* All Ys are the same. */
    memset(mCurrentFrame, changeExposure(color->Y), mTotalPixels);

    /* Fill U, and V panes. */
    uint8_t* U = mFrameU;
    uint8_t* V = mFrameV;
    for (int k = 0; k < mUVTotalNum; k++, U += mUVStep, V += mUVStep) {
        *U = color->U;
        *V = color->V;
    }
}

void EmulatedFakeCameraDevice::drawStripes()
{
    /* Divide frame into 4 stripes. */
    const int change_color_at = mFrameHeight / 4;
    const int each_in_row = mUVInRow / mUVStep;
    uint8_t* pY = mCurrentFrame;
    for (int y = 0; y < mFrameHeight; y++, pY += mFrameWidth) {
        /* Select the color. */
        YUVPixel* color;
        const int color_index = y / change_color_at;
        if (color_index == 0) {
            /* White stripe on top. */
            color = &mWhiteYUV;
        } else if (color_index == 1) {
            /* Then the red stripe. */
            color = &mRedYUV;
        } else if (color_index == 2) {
            /* Then the green stripe. */
            color = &mGreenYUV;
        } else {
            /* And the blue stripe at the bottom. */
            color = &mBlueYUV;
        }

        /* All Ys at the row are the same. */
        memset(pY, changeExposure(color->Y), mFrameWidth);

        /* Offset of the current row inside U/V panes. */
        const int uv_off = (y / 2) * mUVInRow;
        /* Fill U, and V panes. */
        uint8_t* U = mFrameU + uv_off;
        uint8_t* V = mFrameV + uv_off;
        for (int k = 0; k < each_in_row; k++, U += mUVStep, V += mUVStep) {
            *U = color->U;
            *V = color->V;
        }
    }
}

int EmulatedFakeCameraDevice::rotateFrame()
{
    if ((systemTime(SYSTEM_TIME_MONOTONIC) - mLastRotatedAt) >= mRotateFreq) {
        mLastRotatedAt = systemTime(SYSTEM_TIME_MONOTONIC);
        mCurrentFrameType++;
        if (mCurrentFrameType > 2) {
            mCurrentFrameType = 0;
        }
        if (mCurrentFrameType == 2) {
            LOGD("********** Rotated to the SOLID COLOR frame **********");
            /* Solid color: lets rotate color too. */
            if (mCurrentColor == &mWhiteYUV) {
                LOGD("----- Painting a solid RED frame -----");
                mCurrentColor = &mRedYUV;
            } else if (mCurrentColor == &mRedYUV) {
                LOGD("----- Painting a solid GREEN frame -----");
                mCurrentColor = &mGreenYUV;
            } else if (mCurrentColor == &mGreenYUV) {
                LOGD("----- Painting a solid BLUE frame -----");
                mCurrentColor = &mBlueYUV;
            } else {
                /* Back to white. */
                LOGD("----- Painting a solid WHITE frame -----");
                mCurrentColor = &mWhiteYUV;
            }
        } else if (mCurrentFrameType == 0) {
            LOGD("********** Rotated to the CHECKERBOARD frame **********");
        } else {
            LOGD("********** Rotated to the STRIPED frame **********");
        }
    }

    return mCurrentFrameType;
}

#endif  // EFCD_ROTATE_FRAME

}; /* namespace android */