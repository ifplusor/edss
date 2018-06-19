/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 1999-2008 Apple Inc.  All Rights Reserved.
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 *
 */
/*
    File:       QTSSDictionary.h

    Contains:   Definitions of two classes: QTSSDictionary and QTSSDictionaryMap.
                Collectively, these classes implement the "dictionary" APIs in QTSS
                API. A QTSSDictionary corresponds to a QTSS_Object,
                a QTSSDictionaryMap corresponds to a QTSS_ObjectType.

    Created: Tue, Mar 2, 1999 @ 4:23 PM
*/

/*
 * QTSS objects consist of attributes that are used to store data. Every attribute has a name, an attribute ID, a
 * data type, and permissions for reading and writing the attribute’s value. There are two attribute types:
 *
 * - static attributes. Static attributes are present in all instances of an object type. A module can add static
 *   attributes to objects from its Register role only. All of the server’s built-in attributes are static attributes.
 *
 * - instance attributes. Instance attributes are added to a specific instance of any object type. A module can use
 *   any role to add an instance attribute to an object and can also remove instance attributes that it has added to an
 *   object.
 */


#ifndef _QTSSDICTIONARY_H_
#define _QTSSDICTIONARY_H_

#include <stdlib.h>

#include <CF/Types.h>
#include <CF/Core/Mutex.h>
#include <CF/StrPtrLen.h>

#include "QTSS.h"
#include "QTSSStream.h"

class QTSSDictionary;

class QTSSDictionaryMap;

class QTSSAttrInfoDict;

#define __DICTIONARY_TESTING__ 0

//
// Function prototype for attr functions
typedef void *(*QTSS_AttrFunctionPtr)(QTSSDictionary *, UInt32 *);

/**
 * Dictionary is a data storage base class that implements key and value access to object data. This base class is
 * inherited by all server objects defined by the API.
 *
 * 该类有指向 QTSSDictionaryMap 类对象的成员 fMap、fInstanceMap。
 */
class QTSSDictionary : public QTSSStream {
 public:

  //
  // CONSTRUCTOR / DESTRUCTOR

  explicit QTSSDictionary(QTSSDictionaryMap *inMap, CF::Core::Mutex *inMutex = NULL);

  ~QTSSDictionary() override;

  //
  // QTSS API CALLS

  // Flags used by internal callers of these routines
  enum {
    kNoFlags = 0,
    kDontObeyReadOnly = 1,
    kDontCallCompletionRoutine = 2
  };

  /**
   * @brief This version of GetValue copies the element into a buffer provided by the caller
   *
   * @return QTSS_BadArgument,
   * @return QTSS_NotPreemptiveSafe (if attribute is not preemptive safe),
   * @return QTSS_BadIndex (if inIndex is bad)
   */
  QTSS_Error GetValue(QTSS_AttributeID inAttrID, UInt32 inIndex, void *ioValueBuffer, UInt32 *ioValueLen);

  /**
   * @brief  This version of GetValue returns a pointer to the internal buffer for the attribute.
   * @note   Only usable if the attribute is preemptive safe.
   * @return Same as above, but also QTSS_NotEnoughSpace, if value is too big for buffer.
   */
  QTSS_Error GetValuePtr(QTSS_AttributeID inAttrID, UInt32 inIndex, void **outValueBuffer, UInt32 *outValueLen) {
    return GetValuePtr(inAttrID, inIndex, outValueBuffer, outValueLen, false);
  }

  /**
   * @brief  This version of GetValue converts the value to a string before returning it.
   * @note   Memory for the string is allocated internally.
   * @return QTSS_BadArgument,
   * @return QTSS_BadIndex,
   * @return QTSS_ValueNotFound
   */
  QTSS_Error GetValueAsString(QTSS_AttributeID inAttrID, UInt32 inIndex, char **outString);

  /**
   * @return QTSS_BadArgument,
   * @return QTSS_ReadOnly (if attribute is read only),
   * @return QTSS_BadIndex (attempt to set indexed parameter with param retrieval)
   * @note this method will copy inBuffer#inLen to internally memory
   */
  QTSS_Error SetValue(QTSS_AttributeID inAttrID, UInt32 inIndex, const void *inBuffer, UInt32 inLen, UInt32 inFlags = kNoFlags);

  /**
   * @return QTSS_BadArgument
   * @return QTSS_ReadOnly (if attribute is read only),
   */
  QTSS_Error SetValuePtr(QTSS_AttributeID inAttrID, const void *inBuffer, UInt32 inLen, UInt32 inFlags = kNoFlags);

  /**
   * @return QTSS_BadArgument,
   * @return QTSS_ReadOnly (if attribute is read only),
   */
  QTSS_Error CreateObjectValue(QTSS_AttributeID inAttrID, UInt32 *outIndex, QTSSDictionary **newObject, QTSSDictionaryMap *inMap = NULL, UInt32 inFlags = kNoFlags);

  /**
   * @return QTSS_BadArgument,
   * @return QTSS_ReadOnly,
   * @return QTSS_BadIndex
   */
  QTSS_Error RemoveValue(QTSS_AttributeID inAttrID, UInt32 inIndex, UInt32 inFlags = kNoFlags);

  /**
   * @brief Utility routine used by the two external flavors of GetValue
   */
  QTSS_Error GetValuePtr(QTSS_AttributeID inAttrID, UInt32 inIndex, void **outValueBuffer, UInt32 *outValueLen, bool isInternal);

  //
  // ACCESSORS

  QTSSDictionaryMap *GetDictionaryMap() { return fMap; }

  // Returns the Instance dictionary map for this dictionary. This may return NULL
  // if there are no instance attributes in this dictionary
  QTSSDictionaryMap *GetInstanceDictMap() { return fInstanceMap; }

  // Returns the number of values associated with a given attribute
  UInt32 GetNumValues(QTSS_AttributeID inAttrID);

  void SetNumValues(QTSS_AttributeID inAttrID, UInt32 inNumValues);

  // Meant only for internal server use. Does no error checking,
  // doesn't invoke the param retrieval function.
  CF::StrPtrLen *GetValue(QTSS_AttributeID inAttrID) {
    return &fAttributes[inAttrID].fAttributeData;
  }

  CF::Core::Mutex *GetMutex() { return fMutexP; }

  void SetLocked(bool inLocked) { fLocked = inLocked; }

  bool IsLocked() { return fLocked; }

  //
  // GETTING ATTRIBUTE INFO
  QTSS_Error GetAttrInfoByIndex(UInt32 inIndex, QTSSAttrInfoDict **outAttrInfoDict);

  QTSS_Error GetAttrInfoByName(const char *inAttrName, QTSSAttrInfoDict **outAttrInfoDict);

  QTSS_Error GetAttrInfoByID(QTSS_AttributeID inAttrID, QTSSAttrInfoDict **outAttrInfoDict);


  //
  // INSTANCE ATTRIBUTES

  QTSS_Error AddInstanceAttribute(const char *inAttrName, QTSS_AttrFunctionPtr inFuncPtr, QTSS_AttrDataType inDataType, QTSS_AttrPermission inPermission);

  QTSS_Error RemoveInstanceAttribute(QTSS_AttributeID inAttr);

  //
  // MODIFIERS

  // These functions are meant to be used by the server when it is setting up the
  // dictionary attributes. They do no error checking.

  // They don't set fNumAttributes & fAllocatedInternally.
  void SetVal(QTSS_AttributeID inAttrID, void *inValueBuffer, UInt32 inBufferLen);

  void SetVal(QTSS_AttributeID inAttrID, CF::StrPtrLen *inNewValue) {
    this->SetVal(inAttrID, inNewValue->Ptr, inNewValue->Len);
  }

  // Call this if you want to assign empty storage to an attribute
  void SetEmptyVal(QTSS_AttributeID inAttrID, void *inBuf, UInt32 inBufLen);

#if __DICTIONARY_TESTING__
  static void Test(); // API test for these objects
#endif

 protected:

  /*
   * Hooks, derived classes can provide a completion routine for some dictionary functions
   */

  virtual void RemoveValueComplete(UInt32 inAttrIndex, QTSSDictionaryMap *inMap, UInt32 inValueIndex) {}
  virtual void SetValueComplete(UInt32 inAttrIndex, QTSSDictionaryMap *inMap, UInt32 inValueIndex, void *inNewValue, UInt32 inNewValueLen) {}
  virtual void RemoveInstanceAttrComplete(UInt32 inAttrindex, QTSSDictionaryMap *inMap) {}
  virtual QTSSDictionary *CreateNewDictionary(QTSSDictionaryMap *inMap, CF::Core::Mutex *inMutex);

 private:

  struct DictValueElement {
    // This stores all necessary information for each attribute value.

    DictValueElement() : fAllocatedLen(0), fNumAttributes(0), fAllocatedInternally(false), fIsDynamicDictionary(false) {}

    // NOTE: Does not delete! You Must call DeleteAttributeData for that
    ~DictValueElement() {}

    CF::StrPtrLen fAttributeData; // The data. Ptr: value array pointer, Len: every value size
    UInt32 fAllocatedLen;         // How much space do we have allocated? memory size, not value num
    UInt32 fNumAttributes;        // If this is an iterated attribute, how many?
    bool fAllocatedInternally;    // Should we delete this memory?
    bool fIsDynamicDictionary;    // is this a dictionary object?
  };

  DictValueElement fAttributes[QTSS_MAX_ATTRIBUTE_NUMS]; // not dynamic allocate
  DictValueElement *fInstanceAttrs; // create by AddInstanceAttribute()
  UInt32 fInstanceArraySize;
  QTSSDictionaryMap *fMap;
  QTSSDictionaryMap *fInstanceMap;
  CF::Core::Mutex *fMutexP;
  bool fMyMutex;
  bool fLocked;

  void DeleteAttributeData(DictValueElement *inDictValues, UInt32 inNumValues, QTSSDictionaryMap *theMap);
};

/**
 * 描述 QTSS_Object 的具体一个属性。
 *
 * @note QTSSAttrInfoDict 具有4个属性，分别指向fAttrInfo(AttrInfo)对应的4个字段
 *
 * @note QTSSAttrInfoDict 具有自引用的递归结构：</br>
 *   QTSSDictionary </br>
 *       +->fMap/fInstanceMap(QTSSDictionaryMap) </br>
 *           +->fAttrArray(QTSSAttrInfoDict)                      ---+ </br>
 *               +->fMap=QTSSDictionaryMap::kAttrInfoDictIndex       | </br>
 *                   +->fAttrArray(QTSSAttrInfoDict)              ---+ </br>
 *                       +->...
 *
 * @see QTSSDictionaryMap::kAttrInfoDictIndex
 *
 */
class QTSSAttrInfoDict : public QTSSDictionary {
 public:

  // This is all the relevent information for each dictionary attribute.
  struct AttrInfo {
    char fAttrName[QTSS_MAX_ATTRIBUTE_NAME_SIZE + 1];
    QTSS_AttrFunctionPtr fFuncPtr;
    QTSS_AttrDataType fAttrDataType;
    QTSS_AttrPermission fAttrPermission;
  };

  QTSSAttrInfoDict();

  virtual ~QTSSAttrInfoDict();

 private:

  AttrInfo fAttrInfo;
  QTSS_AttributeID fID;

  static AttrInfo sAttributes[];

  friend class QTSSDictionaryMap;
};

/**
 * 这个类对象对应着一个具体的QTSS Object,用来描述这个Object的所有属性(或者说参数)。
 *
 * 它有一个QTSSAttrInfoDict**类型的成员fAttrArray,用来记录这个dict map所对应的Object的所有属性信息。
 */
class QTSSDictionaryMap {
 public:

  //
  // This must be called before using any QTSSDictionary or QTSSDictionaryMap functionality
  static void Initialize();

  // Stores all meta-information for attributes

  // CONSTRUCTOR FLAGS
  enum {
    kNoFlags = 0,
    kAllowRemoval = 1,
    kIsInstanceMap = 2,
    kInstanceAttrsAllowed = 4,
    kCompleteFunctionsAllowed = 8
  };

  //
  // CONSTRUCTOR / DESTRUCTOR

  QTSSDictionaryMap(UInt32 inNumReservedAttrs, UInt32 inFlags = kNoFlags);

  ~QTSSDictionaryMap() {
    for (UInt32 i = 0; i < fAttrArraySize; i++)
      delete fAttrArray[i];
    delete[] fAttrArray;
  }

  //
  // QTSS API CALLS

  // All functions either return QTSS_BadArgument or QTSS_NoErr
  QTSS_Error AddAttribute(const char *inAttrName, QTSS_AttrFunctionPtr inFuncPtr, QTSS_AttrDataType inDataType,
                          QTSS_AttrPermission inPermission);

  //
  // Marks this attribute as removed
  QTSS_Error RemoveAttribute(QTSS_AttributeID inAttrID);

  QTSS_Error UnRemoveAttribute(QTSS_AttributeID inAttrID);

  QTSS_Error CheckRemovePermission(QTSS_AttributeID inAttrID);

  //
  // Searching / Iteration. These never return removed attributes
  QTSS_Error GetAttrInfoByName(const char *inAttrName, QTSSAttrInfoDict **outAttrInfoDict, bool returnRemovedAttr = false);

  QTSS_Error GetAttrInfoByID(QTSS_AttributeID inID, QTSSAttrInfoDict **outAttrInfoDict);

  QTSS_Error GetAttrInfoByIndex(UInt32 inIndex, QTSSAttrInfoDict **outAttrInfoDict);

  QTSS_Error GetAttrID(const char *inAttrName, QTSS_AttributeID *outID);

  //
  // PRIVATE ATTR PERMISSIONS
  enum {
    qtssPrivateAttrModeRemoved = 0x80000000
  };

  //
  // CONVERTING attribute IDs to array indexes. Returns -1 if inAttrID doesn't exist
  inline SInt32 ConvertAttrIDToArrayIndex(QTSS_AttributeID inAttrID);

  static bool IsInstanceAttrID(QTSS_AttributeID inAttrID) {
    return (inAttrID & 0x80000000) != 0;
  }

  //
  // ACCESSORS
  // These functions do no error checking. Be careful.

  /**
   * Return the number of attributes
   * @note Includes removed attributes
   */
  UInt32 GetNumAttrs() { return fNextAvailableID; }

  /**
   * Return the number of valid attributes
   */
  UInt32 GetNumNonRemovedAttrs() { return fNumValidAttrs; }

  bool IsPreemptiveSafe(UInt32 inIndex) {
    Assert(inIndex < fNextAvailableID);
    return (bool) (fAttrArray[inIndex]->fAttrInfo.fAttrPermission & qtssAttrModePreempSafe);
  }

  bool IsWriteable(UInt32 inIndex) {
    Assert(inIndex < fNextAvailableID);
    return (bool) (fAttrArray[inIndex]->fAttrInfo.fAttrPermission & qtssAttrModeWrite);
  }

  bool IsCacheable(UInt32 inIndex) {
    Assert(inIndex < fNextAvailableID);
    return (bool) (fAttrArray[inIndex]->fAttrInfo.fAttrPermission & qtssAttrModeCacheable);
  }

  bool IsRemoved(UInt32 inIndex) {
    Assert(inIndex < fNextAvailableID);
    return (bool) (fAttrArray[inIndex]->fAttrInfo.fAttrPermission & qtssPrivateAttrModeRemoved);
  }

  QTSS_AttrFunctionPtr GetAttrFunction(UInt32 inIndex) {
    Assert(inIndex < fNextAvailableID);
    return fAttrArray[inIndex]->fAttrInfo.fFuncPtr;
  }

  char *GetAttrName(UInt32 inIndex) {
    Assert(inIndex < fNextAvailableID);
    return fAttrArray[inIndex]->fAttrInfo.fAttrName;
  }

  QTSS_AttributeID GetAttrID(UInt32 inIndex) {
    Assert(inIndex < fNextAvailableID);
    return fAttrArray[inIndex]->fID;
  }

  QTSS_AttrDataType GetAttrType(UInt32 inIndex) {
    Assert(inIndex < fNextAvailableID);
    return fAttrArray[inIndex]->fAttrInfo.fAttrDataType;
  }

  bool InstanceAttrsAllowed() {
    return (bool) (fFlags & kInstanceAttrsAllowed);
  }

  bool CompleteFunctionsAllowed() {
    return (bool) (fFlags & kCompleteFunctionsAllowed);
  }

  //
  // MODIFIERS

  /**
   * Sets this attribute ID to have this information
   */
  void SetAttribute(QTSS_AttributeID inID, const char *inAttrName, QTSS_AttrFunctionPtr inFuncPtr,
                    QTSS_AttrDataType inDataType, QTSS_AttrPermission inPermission);


  //
  // DICTIONARY MAPS

  // All dictionary maps are stored here, and are accessable
  // through these routines

  // This enum allows all QTSSDictionaryMaps to be stored in an array
  enum {
    kServerDictIndex = 0,
    kPrefsDictIndex = 1,
    kTextMessagesDictIndex = 2,
    kServiceDictIndex = 3,

    kRTPStreamDictIndex = 4,
    kClientSessionDictIndex = 5,
    kRTSPSessionDictIndex = 6,
    kRTSPRequestDictIndex = 7,
    kRTSPHeaderDictIndex = 8,
    kFileDictIndex = 9,
    kModuleDictIndex = 10,
    kModulePrefsDictIndex = 11,
    kAttrInfoDictIndex = 12,
    kQTSSUserProfileDictIndex = 13,
    kQTSSConnectedUserDictIndex = 14,

    kHTTPSessionDictIndex = 15,

    kNumDictionaries = 16,

    kNumDynamicDictionaryTypes = 500,
    kIllegalDictionary = kNumDynamicDictionaryTypes + kNumDictionaries
  };

  // This function converts a QTSS_ObjectType to an index
  static UInt32 GetMapIndex(QTSS_ObjectType inType);

  // Using one of the above predefined indexes, this returns the corresponding map
  static QTSSDictionaryMap *GetMap(UInt32 inIndex) {
    Assert(inIndex < kNumDynamicDictionaryTypes + kNumDictionaries);
    return sDictionaryMaps[inIndex];
  }

  static QTSS_ObjectType CreateNewMap();

 private:

  //
  // Repository for dictionary maps

  static QTSSDictionaryMap *sDictionaryMaps[kNumDictionaries + kNumDynamicDictionaryTypes];
  static UInt32 sNextDynamicMap;

  enum {
    kMinArraySize = 20
  };

  UInt32 fNextAvailableID;
  UInt32 fNumValidAttrs;   // 有效的属性数量
  UInt32 fAttrArraySize;
  QTSSAttrInfoDict **fAttrArray;
  UInt32 fFlags;

  friend class QTSSDictionary;
};

inline SInt32 QTSSDictionaryMap::ConvertAttrIDToArrayIndex(QTSS_AttributeID inAttrID) {
  SInt32 theIndex = inAttrID & 0x7FFFFFFF;
  if ((theIndex < 0) || (theIndex >= (SInt32) fNextAvailableID))
    return -1;
  else
    return theIndex;
}

#endif
