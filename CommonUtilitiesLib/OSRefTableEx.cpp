/*
	Copyleft (c) 2012-2016 EasyDarwin.ORG.  All rights reserved.
	Github: https://github.com/EasyDarwin
	WEChat: EasyDarwin
	Website: http://www.EasyDarwin.org
*/

#include"OSRefTableEx.h"

OSRefTableEx::OSRefEx *OSRefTableEx::Resolve(const string &key)//���ö��󣬴��ڷ���ָ�룬���򷵻�nullptr
{
  OSMutexLocker locker(&m_Mutex);
  if (m_Map.find(key) == m_Map.end())//������
    return nullptr;
  m_Map[key]->AddRef(1);

  return m_Map[key];
}

OS_Error OSRefTableEx::Release(const string &key)//�ͷ�����
{
  OSMutexLocker locker(&m_Mutex);
  if (m_Map.find(key) == m_Map.end())//������
    return EPERM;
  //make sure to wakeup anyone who may be waiting for this resource to be released
  m_Map[key]->AddRef(-1);//��������
  m_Map[key]->GetCondPtr()->Signal();

  return OS_NoErr;
}

OS_Error OSRefTableEx::Register(const string &key, void *pObject)//���뵽map��
{
  Assert(pObject != nullptr);
  if (pObject == nullptr)
    return EPERM;

  OSMutexLocker locker(&m_Mutex);//����
  if (m_Map.find(key) != m_Map.end())//�Ѿ����֣��ܾ��ظ���key
    return EPERM;
  OSRefEx
      *RefTemp = new OSRefEx(pObject);//����value���ڴ�new����UnRegister��delete
  m_Map[key] = RefTemp;//���뵽map��

  return OS_NoErr;
}

OS_Error OSRefTableEx::UnRegister(const string &key)//��map���Ƴ����������ü���Ϊ0�������Ƴ�
{
  OSMutexLocker locker(&m_Mutex);
  if (m_Map.find(key) == m_Map.end())//�����ڵ�ǰkey
    return EPERM;
  //make sure that no one else is using the object
  while (m_Map[key]->GetRefNum() > 0)
    m_Map[key]->GetCondPtr()->Wait(&m_Mutex);

  delete m_Map[key];//�ͷ�
  m_Map.erase(key);//�Ƴ�

  return OS_NoErr;
}

OS_Error OSRefTableEx::TryUnRegister(const string &key) {
  OSMutexLocker locker(&m_Mutex);
  if (m_Map.find(key) == m_Map.end())//�����ڵ�ǰkey
    return EPERM;
  if (m_Map[key]->GetRefNum() > 0)
    return EPERM;
  // At this point, this is guarenteed not to block, because
  // we've already checked that the refCount is low.
  delete m_Map[key];//�ͷ�
  m_Map.erase(key);//�Ƴ�

  return OS_NoErr;
}