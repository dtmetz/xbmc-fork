/*
 *      Copyright (C) 2005-2008 Team XBMC
 *      http://www.xbmc.org
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
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "include.h"
#include "AudioContext.h"
#include "IAudioDeviceChangedCallback.h"
#include "Settings.h"
#include "GUISettings.h"
#include "XBAudioConfig.h"

extern HWND g_hWnd;


CAudioContext g_audioContext;

CAudioContext::CAudioContext()
{
  m_iDevice=DEFAULT_DEVICE;
#ifdef HAS_AUDIO
#ifdef HAS_AUDIO_PASS_THROUGH
  m_pAC97Device=NULL;
#endif
  m_pDirectSoundDevice=NULL;
#endif  
}

CAudioContext::~CAudioContext()
{
}

void CAudioContext::SetSoundDeviceCallback(IAudioDeviceChangedCallback* pCallback)
{
  m_pCallback=pCallback;
}

// \brief Create a new device by type (DEFAULT_DEVICE, DIRECTSOUND_DEVICE, AC97_DEVICE)
// Note: DEFAULT_DEVICE is created by the IAudioDeviceChangedCallback
void CAudioContext::SetActiveDevice(int iDevice)
{
  /* if device is the same, no need to bother */
  if(m_iDevice == iDevice)
    return;

  if (iDevice==DEFAULT_DEVICE)
  {
    /* we just tell callbacks to init, it will setup audio */
    if (m_pCallback)
      m_pCallback->Initialize(iDevice);

    return;
  }

  /* deinit current device */
  RemoveActiveDevice();

  m_iDevice=iDevice;

#ifdef HAS_AUDIO
  if (iDevice==DIRECTSOUND_DEVICE)
  {
    // Create DirectSound
    if (FAILED(DirectSoundCreate( NULL, &m_pDirectSoundDevice, NULL )))
    {
      CLog::Log(LOGERROR, "DirectSoundCreate() Failed");
      return;
    }
    if (FAILED(m_pDirectSoundDevice->SetCooperativeLevel(g_hWnd, DSSCL_PRIORITY)))
    {
      CLog::Log(LOGERROR, "DirectSoundDevice::SetCooperativeLevel() Failed");
      return;
    }
  }
  else if (iDevice==AC97_DEVICE)
  {
#ifdef HAS_AUDIO_PASS_THROUGH
    // Create AC97 Device
    if (FAILED(Ac97CreateMediaObject(DSAC97_CHANNEL_DIGITAL, NULL, NULL, &m_pAC97Device)))
#endif
    {
      CLog::Log(LOGERROR, "Failed to create digital Ac97CreateMediaObject()");
      return;
    }
  }
#endif  

  if (m_pCallback)
    m_pCallback->Initialize(m_iDevice);
}

// \brief Return the active device type (NONE, DEFAULT_DEVICE, DIRECTSOUND_DEVICE, AC97_DEVICE)
int CAudioContext::GetActiveDevice()
{
  return m_iDevice;
}

// \brief Remove the current sound device, eg. to setup new speaker config
void CAudioContext::RemoveActiveDevice()
{
  if (m_pCallback)
    m_pCallback->DeInitialize(m_iDevice);

  m_iDevice=NONE;

#ifdef HAS_AUDIO
#ifdef HAS_AUDIO_PASS_THROUGH
  SAFE_RELEASE(m_pAC97Device);
#endif
  SAFE_RELEASE(m_pDirectSoundDevice);
#endif  
}

// \brief set a new speaker config
void CAudioContext::SetupSpeakerConfig(int iChannels, bool& bAudioOnAllSpeakers, bool bIsMusic)
{
  m_bAC3EncoderActive = false;
  bAudioOnAllSpeakers = false;

#ifdef HAS_AUDIO
  DWORD spconfig = DSSPEAKER_USE_DEFAULT;  
  if (g_guiSettings.GetInt("audiooutput.mode") == AUDIO_DIGITAL)
  {
    if (((g_guiSettings.GetBool("musicplayer.outputtoallspeakers")) && (bIsMusic)) || (g_stSettings.m_currentVideoSettings.m_OutputToAllSpeakers && !bIsMusic))
    {
      if( g_audioConfig.GetAC3Enabled() )
      {
        bAudioOnAllSpeakers = true;      
        m_bAC3EncoderActive = true;
        spconfig = DSSPEAKER_USE_DEFAULT; //Allows ac3 encoder should it be enabled
      }
      else
      {
        if (iChannels == 1)
          spconfig = DSSPEAKER_MONO;
        else
        { 
          spconfig = DSSPEAKER_STEREO;
        }
      }        
    }
    else
    {
      if (iChannels == 1)
        spconfig = DSSPEAKER_MONO;
      else if (iChannels == 2)
        spconfig = DSSPEAKER_STEREO;
      else
      {
        spconfig = DSSPEAKER_USE_DEFAULT; //Allows ac3 encoder should it be enabled
        m_bAC3EncoderActive = g_audioConfig.GetAC3Enabled();
      }
    }
  }
  else // We don't want to use the Dolby Digital Encoder output. Downmix to surround instead.
  {
    if (iChannels == 1)
      spconfig = DSSPEAKER_MONO;
    else
    { 
      // check if surround mode is allowed, if not then use normal stereo  
      // don't always set it to default as that enabled ac3 encoder if that is allowed in dash
      // ruining quality
      spconfig = DSSPEAKER_STEREO;
    }
  }

  DWORD spconfig_old = DSSPEAKER_USE_DEFAULT;
  if(m_pDirectSoundDevice)
  {
    m_pDirectSoundDevice->GetSpeakerConfig(&spconfig_old);
    spconfig_old = DSSPEAKER_USE_DEFAULT;
  }

  /* speaker config identical, no need to do anything */
  if(spconfig == spconfig_old) return;
#endif  

  /* speaker config has changed, caller need to recreate it */
  RemoveActiveDevice();
}

bool CAudioContext::IsAC3EncoderActive() const
{
  return m_bAC3EncoderActive;
}

bool CAudioContext::IsPassthroughActive() const
{
  return (m_iDevice == AC97_DEVICE);
}

