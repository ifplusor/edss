/*
	Copyright (c) 2012-2016 EasyDarwin.ORG.  All rights reserved.
	Github: https://github.com/EasyDarwin
	WEChat: EasyDarwin
	Website: http://www.easydarwin.org
*/
#include "OSMapEx.h"
#include "OS.h"
#ifndef _WIN32
#include<arpa/inet.h>
#endif

string OSMapEx::GenerateSessionId()//����32λ��SessionId
{
  SInt64 theMicroseconds =
      OS::Microseconds();//Windows��1ms�ڶ��ִ�л���ɲ����������ʱһ���ģ���ΪWindows��΢��Ļ�ȡҲ�ǿ�����*1000�����е�
  ::srand((unsigned int) theMicroseconds);
  UInt16 the1Random = ::rand();

  ::srand((unsigned int) the1Random);
  UInt16 the2Random = ::rand();

  ::srand((unsigned int) the2Random);
  UInt16 the3Random = ::rand();

  ::srand((unsigned int) the3Random);
  UInt16 the4Random = ::rand();

  ::srand((unsigned int) the4Random);
  UInt16 the5Random = ::rand();

  ::srand((unsigned int) the5Random);
  UInt16 the6Random = ::rand();

  ::srand((unsigned int) the6Random);
  UInt16 the7Random = ::rand();

  ::srand((unsigned int) the7Random);
  UInt16 the8Random = ::rand();

  char chTemp[33] = {0};
  sprintf(chTemp,
          "%04X%04X%04X%04X%04X%04X%04X%04X",
          the1Random,
          the2Random,
          the3Random,
          the4Random,
          the5Random,
          the6Random,
          the7Random,
          the8Random);
  return string(chTemp);
}

bool OSMapEx::Insert(const string &strSessionId, SInt64 lastingTime) {
  OSMutexLocker mutexLocker(&m_Mutex);
  if (m_Map.find(strSessionId) != m_Map.end())//�Ѿ�����
    return false;
  else {
    strMapData
        strTemp(lastingTime + OS::Microseconds(), 1);//1��ʾ���Զ�ɾ����ֱ����ʱ��ɾ��
    m_Map.insert(MapType::value_type(strSessionId, strTemp));
    return true;
  }
}

string OSMapEx::GererateAndInsert(SInt64 lastingTime) {
  OSMutexLocker mutexLocker(&m_Mutex);
  string strSessionId;
  do {
    strSessionId = OSMapEx::GenerateSessionId();//����sessionid
  } while (m_Map.find(strSessionId)
      != m_Map.end());//������ظ��ľ�һֱ����ֱ�������ظ���
  strMapData
      strTemp(lastingTime + OS::Microseconds(), 0);//0��ʾ�Զ�ɾ������֤1�ξ�ɾ��
  m_Map.insert(MapType::value_type(strSessionId, strTemp));
  return strSessionId;
}

bool OSMapEx::FindAndDelete(const string &strSessionID)//���Ҳ�ɾ�����̰߳�ȫ,�ҵ�����true,���򷵻�false
{
  OSMutexLocker mutexLocker(&m_Mutex);
  SInt64 sNowTime = OS::Microseconds();//��ȡ��ǰʱ��
  bool bReVal = true;
  MapType::iterator l_it = m_Map.find(strSessionID);
  if (l_it != m_Map.end())//�ҵ���
  {
    if (l_it->second.m_LastingTime < sNowTime)//���ʧЧ�˷���false
      bReVal = false;
    if (l_it->second.m_AllExist == 0)//������Զ�ɾ�����;�ɾ��
      m_Map.erase(l_it);
    return bReVal;
  } else
    return false;
}

void OSMapEx::CheckTimeoutAndDelete()//����map���SessionID,ɾ��ʧЧ��SessionID
{
  //unsigned int num=0;
  OSMutexLocker mutexLocker(&m_Mutex);
  SInt64 sNowTime = OS::Microseconds();//��ȡ��ǰʱ��
  for (MapType::iterator i = m_Map.begin(); i != m_Map.end(); /*i++*/) {
    if (i->second.m_LastingTime < sNowTime)//ʧЧ
    {
      m_Map.erase(i++);  //i++����1����i;2i=i+1;3���ر��ݵ�i
      //num++;
    } else {
      ++i;
    }
  }
  //cout<<"ʧЧ"<<num<<"��"<<endl;
}
//for redis

string OSMapEx::GenerateSessionIdForRedis(const string &strIP, UInt16 uPort) {
  SInt64 theMicroseconds =
      OS::Microseconds();//Windows��1ms�ڶ��ִ�л���ɲ����������ʱһ���ģ���ΪWindows��΢��Ļ�ȡҲ�ǿ�����*1000�����е�
  ::srand((unsigned int) theMicroseconds);
  UInt16 the1Random = ::rand();

  ::srand((unsigned int) the1Random);
  UInt16 the2Random = ::rand();

  ::srand((unsigned int) the2Random);
  UInt16 the3Random = ::rand();

  ::srand((unsigned int) the3Random);
  UInt16 the4Random = ::rand();

  ::srand((unsigned int) the4Random);
  UInt16 the5Random = ::rand();

  UInt32 uIP = inet_addr(strIP.c_str());

  char chTemp[33] = {0};
  sprintf(chTemp,
          "%08X%04X%04X%04X%04X%04X%04X",
          uIP,
          uPort,
          the1Random,
          the2Random,
          the3Random,
          the4Random,
          the5Random);//32λ16���ƣ�16�ֽڣ�ǰ�����ֽ���IP�Ͷ˿����
  return string(chTemp);
}
//for redis