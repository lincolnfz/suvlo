/************************************************************************
    This file is part of VLCWrapper.
    
    File:    VLCWrapper.h
    Desc.:   An simple C++-interface to libvlc.

    Author:  Alex Skoruppa
    Date:    08/10/2009
	Updated: 09/11/2010
    eM@il:   alex.skoruppa@googlemail.com

    VLCWrapper is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
     
    VLCWrapper is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.
     
    You should have received a copy of the GNU General Public License
    along with VLCWrapper.  If not, see <http://www.gnu.org/licenses/>.
************************************************************************/
#ifndef __VLCWRAPPER_H__
#define __VLCWRAPPER_H__

#include <memory>

class VLCWrapperImpl;

// Typedefs for old MS compilers
typedef unsigned __int32	uint32_t;
typedef __int64             int64_t;

// Typedefs for VLC types
typedef struct libvlc_event_t           VLCEvent;                     ///< A vlc event.
typedef void (*VLCEventHandler)         (const VLCEvent *, void *);   ///< Event handler callback.

class VLCWrapper
{
	std::auto_ptr<VLCWrapperImpl> m_apImpl; ///< VLCWrapper's private Implementation
public:
	VLCWrapper(void);
	~VLCWrapper(void);

    /** Set window for media output.
    *   @param [in] pHwnd window, on Windows a HWND handle. */
    void SetOutputWindow(void* pHwnd);

    /** Register an event handler for libvlc-events.
    *   @param [in] evt The event handler.
	*   @param [in] pointer on user data.
	*/
    void SetEventHandler(VLCEventHandler evt, void* userData);

    /** Open a media file.
    *   @param [in] pszMediaPathName PathName of the media file. */
    void OpenMedia(const char* pszMediaPathName);

    /** Start playback. */
    void Play();

    /** Pause playback. */
    void Pause();
    
    /** Stop playback. */
    void Stop();

    /** Get length of media in milliseconds. Call this in the event handler,
    *   otherwise the result is not reliable!!!
    *   @return The media length in milliseconds. */
    int64_t GetLength();

    /** Get actual position of media in milliseconds. Call this in the event handler,
    *   otherwise the result is not reliable!!!
    *   @return The media position in milliseconds. */
    int64_t GetTime();

    /** Set new position of media in milliseconds.
    *   @param [in] llNewTime The new media position in milliseconds. */
    void SetTime(int64_t llNewTime);

    /** Mutes the audio output of playback.
    *   @param [in] bMute True or false. */
    void Mute(bool bMute=true);

    /** Get mute state of playback.
    *   @return True or false. */
    bool GetMute();

    /** Returns the actual audio volume.
    *   @return The actual audio volume. */
    int  GetVolume();

    /** Set the actual audio volume.
    *   @param [in] iVolume New volume level. */
    void SetVolume(int iVolume);    
};

#endif // __VLCWRAPPER_H__