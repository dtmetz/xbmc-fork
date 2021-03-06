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

#include "stdafx.h"
#include "CPUInfo.h"
#include <string>
#include <string.h>

#ifdef __APPLE__
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#include "log.h"

using namespace std;

// In seconds
#define MINIMUM_TIME_BETWEEN_READS 2

#ifdef _WIN32PC
/* replacement gettimeofday implementation, copy from dvdnav_internal.h */
#include <sys/timeb.h>
static inline int _private_gettimeofday( struct timeval *tv, void *tz )
{
  struct timeb t;
  ftime( &t );
  tv->tv_sec = t.time;
  tv->tv_usec = t.millitm * 1000;
  return 0;
}
#define gettimeofday(TV, TZ) _private_gettimeofday((TV), (TZ))
#endif

CCPUInfo::CCPUInfo(void)
{
#ifdef __APPLE__
  size_t len = 4;

  // The number of cores.
  if (sysctlbyname("hw.activecpu", &m_cpuCount, &len, NULL, 0) == -1)
      m_cpuCount = 1;

  // Get CPU frequency, scaled to MHz.
  long long hz = 0;
  len = sizeof(hz);
  if (sysctlbyname("hw.cpufrequency", &hz, &len, NULL, 0) == -1)
    m_cpuFreq = 0.0;
  else
    m_cpuFreq = hz / 1000000.0;

  // The model.
  char buffer[512];
  len = 512;
  if (sysctlbyname("machdep.cpu.brand_string", &buffer, &len, NULL, 0) == 0)
    m_cpuModel = buffer;

  // Go through each core.
  for (int i=0; i<m_cpuCount; i++)
  {
    CoreInfo core;
    core.m_id = i;
    m_cores[core.m_id] = core;
  }
#elif defined(_WIN32PC)
  char rgValue [128];
  HKEY hKey;
  DWORD dwSize=128;
  DWORD dwMHz=0;
  LONG ret = RegOpenKeyEx(HKEY_LOCAL_MACHINE,"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",0, KEY_READ, &hKey);
  ret = RegQueryValueEx(hKey,"ProcessorNameString", NULL, NULL, (LPBYTE)rgValue, &dwSize);
  if(ret == 0)
    m_cpuModel = rgValue;
  else
    m_cpuModel = "Unknown";

  dwSize = sizeof(dwMHz);
  ret = RegQueryValueEx(hKey,"~MHz", NULL, NULL, (LPBYTE)&dwMHz, &dwSize);
  if(ret == 0)
  {
    m_cpuFreq = float(dwMHz)/1000;
  }
  else
    m_cpuFreq = 0.0;

  RegCloseKey(hKey);

  SYSTEM_INFO siSysInfo;
  GetSystemInfo(&siSysInfo); 
  m_cpuCount = siSysInfo.dwNumberOfProcessors;

  CoreInfo core;
  m_cores[0] = core;

#else
  m_fProcStat = fopen("/proc/stat", "r");
  m_fProcTemperature = fopen("/proc/acpi/thermal_zone/THRM/temperature", "r");
  if (m_fProcTemperature == NULL)
    m_fProcTemperature = fopen("/proc/acpi/thermal_zone/THR1/temperature", "r");
  if (m_fProcTemperature == NULL)
    m_fProcTemperature = fopen("/proc/acpi/thermal_zone/THM/temperature", "r");
  m_lastUsedPercentage = 0;

  FILE* cpuinfo = fopen("/proc/cpuinfo", "r");
  m_cpuCount = 0;
  m_cpuFreq = 0;
  if (cpuinfo)
  {
    char buffer[512];

    int nCurrId = 0;
    while (fgets(buffer, sizeof(buffer), cpuinfo))
    {
      if (strncmp(buffer, "processor", strlen("processor"))==0)
      {
        char *needle = strstr(buffer, ":");
        CoreInfo core;
        if (needle)
        {
          core.m_id = atoi(needle+2);
          nCurrId = core.m_id;
          m_cores[core.m_id] = core;
        }
        m_cpuCount++;
      }
      else if (strncmp(buffer, "cpu MHz", strlen("cpu MHz"))==0)
      {
        char *needle = strstr(buffer, ":");
        if (needle && strlen(needle)>3)
        {
          needle+=2;
          sscanf(needle, "%f", &m_cpuFreq);
          m_cores[nCurrId].m_fSpeed = m_cpuFreq;
        }
      }
      else if (strncmp(buffer, "vendor_id", strlen("vendor_id"))==0)
      {
        char *needle = strstr(buffer, ":");
        if (needle && strlen(needle)>3)
        {
          needle+=2;
          m_cores[nCurrId].m_strVendor = needle;
          m_cores[nCurrId].m_strVendor.Trim();
        }
      }
      else if (strncmp(buffer, "model name", strlen("model name"))==0)
      {
        char *needle = strstr(buffer, ":");
        if (needle && strlen(needle)>3)
        {
          needle+=2;
          m_cpuModel = needle;
          m_cores[nCurrId].m_strModel = m_cpuModel;
          m_cores[nCurrId].m_strModel.Trim();
        }
      }
    }
    fclose(cpuinfo);
  }
  else
  {
    m_cpuCount = 1;
    m_cpuModel = "Unknown";
  }

  readProcStat(m_userTicks, m_niceTicks, m_systemTicks, m_idleTicks);
#endif
}

CCPUInfo::~CCPUInfo()
{
  if (m_fProcStat != NULL)
    fclose(m_fProcStat);

  if (m_fProcTemperature != NULL)
    fclose(m_fProcTemperature);
}

int CCPUInfo::getUsedPercentage()
{
  if (m_lastReadTime + MINIMUM_TIME_BETWEEN_READS > time(NULL))
  {
    return m_lastUsedPercentage;
  }

  unsigned long long userTicks;
  unsigned long long niceTicks;
  unsigned long long systemTicks;
  unsigned long long idleTicks;

  if (!readProcStat(userTicks, niceTicks, systemTicks, idleTicks))
  {
    return 0;
  }

  userTicks -= m_userTicks;
  niceTicks -= m_niceTicks;
  systemTicks -= m_systemTicks;
  idleTicks -= m_idleTicks;

#ifdef _WIN32PC
  int result = (int) ((userTicks + systemTicks - idleTicks) * 100 / (userTicks + systemTicks));
#else
  int result = (int) ((userTicks + niceTicks + systemTicks) * 100 / (userTicks + niceTicks + systemTicks + idleTicks));
#endif

  m_userTicks += userTicks;
  m_niceTicks += niceTicks;
  m_systemTicks += systemTicks;
  m_idleTicks += idleTicks;

  m_lastUsedPercentage = result;

  return result;
}

CTemperature CCPUInfo::getTemperature()
{
  int value;
  char scale;

  if (m_fProcTemperature == NULL)
    return CTemperature();

  rewind(m_fProcTemperature);
  fflush(m_fProcTemperature);

  char buf[256];
  if (!fgets(buf, sizeof(buf), m_fProcTemperature))
    return CTemperature();

  int num = sscanf(buf, "temperature: %d %c", &value, &scale);
  if (num != 2)
    return CTemperature();

  if (scale == 'C' || scale == 'c')
    return CTemperature::CreateFromCelsius(value);
  if (scale == 'F' || scale == 'f')
    return CTemperature::CreateFromFahrenheit(value);
  else
    return CTemperature();
}

bool CCPUInfo::HasCoreId(int nCoreId) const
{
  std::map<int, CoreInfo>::const_iterator iter = m_cores.find(nCoreId);
  if (iter != m_cores.end())
    return true;
  return false;
}

const CoreInfo &CCPUInfo::GetCoreInfo(int nCoreId)
{
  std::map<int, CoreInfo>::iterator iter = m_cores.find(nCoreId);
  if (iter != m_cores.end())
    return iter->second;

  static CoreInfo dummy;
  return dummy;
}

bool CCPUInfo::readProcStat(unsigned long long& user, unsigned long long& nice,
    unsigned long long& system, unsigned long long& idle)
{

#ifdef _WIN32PC
  FILETIME idleTime;
  FILETIME kernelTime;
  FILETIME userTime;
  ULARGE_INTEGER ulTime;
  unsigned long long coreUser, coreSystem, coreIdle;
  GetSystemTimes( &idleTime, &kernelTime, &userTime );
  ulTime.HighPart = userTime.dwHighDateTime;
  ulTime.LowPart = userTime.dwLowDateTime;
  user = coreUser = ulTime.QuadPart;

  ulTime.HighPart = kernelTime.dwHighDateTime;
  ulTime.LowPart = kernelTime.dwLowDateTime;
  system = coreSystem = ulTime.QuadPart;

  ulTime.HighPart = idleTime.dwHighDateTime;
  ulTime.LowPart = idleTime.dwLowDateTime;
  idle = coreIdle = ulTime.QuadPart;

  nice = 0;

  coreUser -= m_cores[0].m_user;
  coreSystem -= m_cores[0].m_system;
  coreIdle -= m_cores[0].m_idle;
  m_cores[0].m_fPct = ((double)(coreUser + coreSystem - coreIdle) * 100.0) / (double)(coreUser + coreSystem);
  m_cores[0].m_user += coreUser;
  m_cores[0].m_system += coreSystem;
  m_cores[0].m_idle += coreIdle;
  gettimeofday(&m_cores[0].m_lastSample, NULL);

#else
  if (m_fProcStat == NULL)
    return false;

  rewind(m_fProcStat);
  fflush(m_fProcStat);

  char buf[256];
  if (!fgets(buf, sizeof(buf), m_fProcStat))
    return false;

  int num = sscanf(buf, "cpu %llu %llu %llu %llu %*s\n", &user, &nice, &system, &idle);
  while (fgets(buf, sizeof(buf), m_fProcStat) && num >= 4)
  {
    unsigned long long coreUser, coreNice, coreSystem, coreIdle;
    int nCpu=0;
    num = sscanf(buf, "cpu%d %llu %llu %llu %llu %*s\n", &nCpu, &coreUser, &coreNice, &coreSystem, &coreIdle);
    std::map<int, CoreInfo>::iterator iter = m_cores.find(nCpu);
    if (num > 4 && iter != m_cores.end())
    {
      coreUser -= iter->second.m_user;
      coreNice -= iter->second.m_nice;
      coreSystem -= iter->second.m_system;
      coreIdle -= iter->second.m_idle;
      iter->second.m_fPct = ((double)(coreUser + coreNice + coreSystem) * 100.0) / (double)(coreUser + coreNice + coreSystem + coreIdle);
      iter->second.m_user += coreUser;
      iter->second.m_nice += coreNice;
      iter->second.m_system += coreSystem;
      iter->second.m_idle += coreIdle;
      gettimeofday(&iter->second.m_lastSample, NULL);
    }
  }
#endif

  m_lastReadTime = time(NULL);
  return true;
}

CStdString CCPUInfo::GetCoresUsageString() const
{
  CStdString strCores;
  std::map<int, CoreInfo>::const_iterator iter = m_cores.begin();
  while (iter != m_cores.end())
  {
    CStdString strCore;
    strCore.Format("CPU%d: %3.1f%% ",iter->first, iter->second.m_fPct);
    strCores+=strCore;
    iter++;
  }
  return strCores;
}

CCPUInfo g_cpuInfo;

/*
int main()
{
  CCPUInfo c;
  usleep(...);
  int r = c.getUsedPercentage();
  printf("%d\n", r);
}
*/
