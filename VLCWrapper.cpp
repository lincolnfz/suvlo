/************************************************************************
    This file is part of VLCWrapper.
    
    File:    VLCWrapper.cpp
    Desc.:   VLCWrapper Implementation.

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
#include "VLCWrapper.h"
#include "VLCWrapperImpl.h"

VLCWrapper::VLCWrapper(void)
:   m_apImpl(std::auto_ptr<VLCWrapperImpl>(new VLCWrapperImpl()))
{
}

VLCWrapper::~VLCWrapper(void)
{
}

void VLCWrapper::SetOutputWindow(void* pHwnd)
{
    m_apImpl->SetOutputWindow(pHwnd);
}

void VLCWrapper::SetEventHandler(VLCEventHandler evt, void* userData)
{
    m_apImpl->SetEventHandler(evt, userData);
}

void VLCWrapper::Play()
{
    m_apImpl->Play();
}

void VLCWrapper::Pause()
{
    m_apImpl->Pause();
}

void VLCWrapper::Stop()
{
    m_apImpl->Stop();
}

int64_t VLCWrapper::GetLength()
{
    return m_apImpl->GetLength();
}

int64_t VLCWrapper::GetTime()
{
    return m_apImpl->GetTime();
}

void VLCWrapper::SetTime(int64_t llNewTime)
{
    m_apImpl->SetTime(llNewTime);
}

void VLCWrapper::Mute(bool bMute)
{
    m_apImpl->Mute(bMute);
}

bool VLCWrapper::GetMute()
{
    return m_apImpl->GetMute();
}

int  VLCWrapper::GetVolume()
{
    return m_apImpl->GetVolume();
}

void VLCWrapper::SetVolume(int iVolume)
{
    m_apImpl->SetVolume(iVolume);
}

void VLCWrapper::OpenMedia(const char* pszMediaPathName)
{
    m_apImpl->OpenMedia(pszMediaPathName);
}