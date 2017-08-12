#ifndef _OSREFTABLEEX_H_
#define _OSREFTABLEEX_H_

//��Ϊdarwin�����ñ�������ͬ��key���������ַ���hash�����ֲ���������һ�㡣��˿���ʹ��STL�е�map���д��档
//ע��������Ǻ������ϵģ�����Darwin�Դ���

#include <unordered_map>

using namespace std;

#include "OSCond.h"
#include "OSHeaders.h"

class OSRefTableEx {
 public:
  class OSRefEx {

   public:
    OSRefEx() : mp_Object(nullptr), m_Count(0) {}
    OSRefEx(void *pobject) : mp_Object(pobject), m_Count(0) {}
    void *GetObjectPtr() const { return mp_Object; }
    int GetRefNum() const { return m_Count; }
    OSCond *GetCondPtr() { return &m_Cond; }
    void AddRef(int num) { m_Count += num; }

   private:
    void *mp_Object;//���õĶ������������
    int m_Count;//��ǰ���ö��������ֻ��Ϊ0ʱ�������������
    OSCond m_Cond;//to block threads waiting for this ref.
  };

  OSRefEx *Resolve(const string &key);//���ö���
  OS_Error Release(const string &key);//�ͷ�����
  OS_Error Register(const string &key, void *pObject);//���뵽map��
  OS_Error UnRegister(const string &key);//��map���Ƴ�

  OS_Error TryUnRegister(const string &key);//�����Ƴ����������Ϊ0,���Ƴ�������true�����򷵻�false
  int GetEleNumInMap() const { return m_Map.size(); }//���map�е�Ԫ�ظ���
  OSMutex *GetMutex() { return &m_Mutex; }//�������ṩ����ӿ�
  unordered_map<string, OSRefEx *> *GetMap() { return &m_Map; }

 private:
  unordered_map<string, OSRefEx *> m_Map;//��stringΪkey����OSRefExΪvalue
  OSMutex m_Mutex;//�ṩ��map�Ļ������
};
typedef unordered_map<string, OSRefTableEx::OSRefEx *> OSHashMap;
typedef unordered_map<string, OSRefTableEx::OSRefEx *>::iterator OSRefIt;

class OSRefReleaserEx {
 public:
  OSRefReleaserEx(OSRefTableEx *pTable, const string &key)
      : fOSRefTable(pTable), fOSRefKey(key) {}
  ~OSRefReleaserEx() { fOSRefTable->Release(fOSRefKey); }

  string GetRefKey() const { return fOSRefKey; }

 private:
  OSRefTableEx *fOSRefTable;
  string fOSRefKey;
};

#endif //_OSREFTABLEEX_H_