#pragma once

#include "utils/archive.h"
#include "utils/ScraperParser.h"

#include <vector>

struct SScraperInfo
{
  CStdString strTitle;
  CStdString strPath;
  CStdString strThumb;
  CStdString strContent; // dupe, whatever
};

class CVideoInfoTag : public ISerializable
{
public:
  CVideoInfoTag() { Reset(); };
  void Reset();
  bool Load(const TiXmlElement *node);
  bool Save(TiXmlNode *node, const CStdString &tag);
  virtual void Serialize(CArchive& ar);

  CStdString m_strDirector;
  CStdString m_strWritingCredits;
  CStdString m_strGenre;
  CStdString m_strTagLine;
  CStdString m_strPlotOutline;
  CStdString m_strPlot;
  CScraperUrl m_strPictureURL;
  CStdString m_strTitle;
  CStdString m_strVotes;
  vector< pair<CStdString, CStdString> > m_cast;
  typedef vector< pair<CStdString, CStdString> >::const_iterator iCast;

  CStdString m_strSearchString;
  CStdString m_strRuntime;
  CStdString m_strFile;
  CStdString m_strPath;
  CStdString m_strIMDBNumber;
  CStdString m_strMPAARating;
  CStdString m_strFileNameAndPath;
  CStdString m_strOriginalTitle;
  CStdString m_strEpisodeGuide;
  CStdString m_strPremiered;
  CStdString m_strStatus;
  CStdString m_strProductionCode;
  CStdString m_strFirstAired;
  CStdString m_strShowTitle;
  bool m_bWatched;
  int m_iTop250;
  int m_iYear;
  int m_iSeason;
  int m_iEpisode;
  float m_fRating;
};
