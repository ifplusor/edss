/*
	Copyright (c) 2012-2016 EasyDarwin.ORG.  All rights reserved.
	Github: https://github.com/EasyDarwin
	WEChat: EasyDarwin
	Website: http://www.easydarwin.org
*/
/*
	File:       EasyCMSSession.cpp
	Contains:   Implementation of object defined in EasyCMSSession.h.
*/
#include "EasyCMSSession.h"
#include "OSArrayObjectDeleter.h"
#include "SocketUtils.h"
#include "QTSServerInterface.h"

EasyCMSSession::EasyCMSSession()
    : Task(),
      fSocket(new TCPClientSocket(Socket::kNonBlockingSocketType)),
      fTimeoutTask(nullptr),
      fState(kIdle),
      fInputStream(fSocket),
      fOutputStream(fSocket, &fTimeoutTask),
      fRequest(nullptr),
      fReadMutex(),
      fMutex(),
      fContentBuffer(nullptr),
      fContentBufferOffset(0),
      fStreamName(nullptr),
      fLiveSession(true),
      fChannelNum(1) {
  this->SetTaskName("EasyCMSSession");
  fTimeoutTask.SetTask(this);
  fTimeoutTask.SetTimeout(30 * 1000);
}

EasyCMSSession::~EasyCMSSession() {
  delete[] fStreamName;

  if (fSocket) {
    delete fSocket;
    fSocket = nullptr;
  }

  if (fRequest) {
    delete fRequest;
    fRequest = nullptr;
  }

  if (fContentBuffer) {
    delete[] fContentBuffer;
    fContentBuffer = nullptr;
  }
}

void EasyCMSSession::CleanupRequest() {
  if (fRequest != nullptr) {
    // NULL out any references to the current request
    delete fRequest;
    fRequest = nullptr;
  }

  //��շ��ͻ�����
  fOutputStream.ResetBytesWritten();
}

SInt64 EasyCMSSession::Run() {
  OSMutexLocker locker(&fMutex);

  OS_Error theErr = OS_NoErr;
  EventFlags events = this->GetEvents();

  if ((events & Task::kTimeoutEvent) || (events & Task::kKillEvent))
    fLiveSession = false;

  while (fLiveSession) {
    switch (fState) {
      case kIdle: {
        //�����¼�����ִ�в�ͬ�Ķ���
        if (events & Task::kStartEvent) {
          switch (fEasyMsgType) {
            case MSG_CS_FREE_STREAM_REQ: {
              CSFreeStream();
              break;
            }
            default: break;
          }
        }

        if (events & Task::kReadEvent) {
          // �����ӣ�������Ϣ��Ҫ��ȡ(���ݻ��߶Ͽ�)
          fState = kReadingMessage;
        }

        // �������Ϣ��Ҫ��������뷢������
        if (fOutputStream.GetBytesWritten() > 0) {
          fState = kSendingMessage;
        }

        //��ǰ״̬û�н������ݷ��ͣ���ȴ���һ���¼�����
        if (kIdle == fState) {
          return 0;
        }
        break;
      }

      case kReadingMessage: {
        // ���������Ĵ洢��fInputStream��
        if ((theErr = fInputStream.ReadRequest()) == QTSS_NoErr) {
          //���RequestStream����QTSS_NoErr���ͱ�ʾ�Ѿ���ȡ��Ŀǰ���������������
          //���������ܹ���һ�����屨��Header���֣���Ҫ�����ȴ���ȡ...
          fSocket->GetSocket()->SetTask(this);
          fSocket->GetSocket()->RequestEvent(EV_RE);

          fState = kIdle;
          return 0;
        }

        if ((theErr != QTSS_RequestArrived) && (theErr != E2BIG)
            && (theErr != QTSS_BadArgument)) {
          //Any other error implies that the input connection has gone away.
          // We should only kill the whole session if we aren't doing HTTP.
          // (If we are doing HTTP, the POST connection can go away)
          Assert(theErr > 0);
          // If we've gotten here, this must be an HTTP session with
          // a dead input connection. If that's the case, we should
          // clean up immediately so as to not have an open socket
          // needlessly lingering around, taking up space.
          Assert(!fSocket->GetSocket()->IsConnected());

          if (fSocket) {
            fSocket->GetSocket()->Cleanup();
            delete fSocket;
            fSocket = nullptr;
          }

          this->CleanupRequest();
          return 0;
          //��ȡ����ʧ�ܣ�ֱ�ӽ�������
        }
        // �������󳬹��˻�����������Bad Request
        if ((theErr == E2BIG) || (theErr == QTSS_BadArgument)) {
          //����HTTP���ģ�������408
          //(void)QTSSModuleUtils::SendErrorResponse(fRequest, qtssClientBadRequest, qtssMsgBadBase64);
          fState = kCleaningUp;
        } else {
          fState = kProcessingMessage;
        }
        break;
      }
      case kProcessingMessage: {
        // �������籨��
        Assert(fInputStream.GetRequestBuffer());
        Assert(fRequest == NULL);

        // ���ݾ��������Ĺ���HTTPRequest������
        fRequest = new HTTPRequest(&QTSServerInterface::GetServerHeader(),
                                   fInputStream.GetRequestBuffer());

        // ��շ��ͻ�����
        fOutputStream.ResetBytesWritten();

        Assert(theErr == QTSS_RequestArrived);

        // �����յ��ľ��屨��
        ProcessMessage();

        // ÿһ���������Ӧ�����Ƿ�����ɣ������ֱ�ӽ��лظ���Ӧ
        if (fOutputStream.GetBytesWritten() > 0) {
          fState = kSendingMessage;
        } else {
          fState = kCleaningUp;
        }
        break;
      }
      case kSendingMessage: {
        //������Ӧ����
        theErr = fOutputStream.Flush();

        if (theErr == 115) {
          fSocket->GetSocket()->SetTask(this);
          fSocket->GetSocket()->RequestEvent(EV_WR);
          this->ForceSameThread();
          return 0;
        }

        if (theErr == EAGAIN || theErr == EINPROGRESS) {
          // If we get this error, we are currently flow-controlled and should
          // wait for the socket to become writeable again
          // ����յ�Socket EAGAIN������ô������Ҫ��Socket�ٴο�д��ʱ���ٵ��÷���
          fSocket->GetSocket()->SetTask(this);
          fSocket->GetSocket()->RequestEvent(fSocket->GetEventMask());
          this->ForceSameThread();
          // We are holding mutexes, so we need to force
          // the same thread to be used for next Run()
          return 0;
        } else if (theErr != QTSS_NoErr) {
          // Any other error means that the client has disconnected, right?
          Assert(!this->IsConnected());
          //���������������ʧ�ܣ�ֱ�ӽ�������
          //ResetClientSocket();
          return -1;
        }

        fState = kCleaningUp;
        break;
      }
      case kCleaningUp: {
        // һ������Ķ�ȡ��������Ӧ�����������ȴ���һ�����籨�ģ�
        this->CleanupRequest();
        fState = kIdle;
        if (IsConnected()) {
          fSocket->GetSocket()->SetTask(this);
          fSocket->GetSocket()->RequestEvent(EV_RE | EV_WR);//�����¼�����
        }
        return 0;
      }
      default: break;
    }
  }
  return -1;
}

QTSS_Error EasyCMSSession::ProcessMessage() {
  if (NULL == fRequest) return QTSS_BadArgument;

  //����HTTPRequest����
  QTSS_Error theErr = fRequest->Parse();
  if (theErr != QTSS_NoErr) return QTSS_BadArgument;

  //��ȡ����Content json���ݲ���
  StrPtrLen *lengthPtr = fRequest->GetHeaderValue(httpContentLengthHeader);
  StringParser theContentLenParser(lengthPtr);
  theContentLenParser.ConsumeWhitespace();
  UInt32 content_length = theContentLenParser.ConsumeInteger(nullptr);

  if (content_length) {
    qtss_printf("EasyCMSSession::ProcessMessage read content-length:%d \n",
                content_length);
    // ���content��fContentBuffer��fContentBufferOffset�Ƿ���ֵ����,������ڣ�˵�������Ѿ���ʼ
    // ����content������,���������,������Ҫ��������ʼ��fContentBuffer��fContentBufferOffset
    if (fContentBuffer == nullptr) {
      fContentBuffer = new char[content_length + 1];
      memset(fContentBuffer, 0, content_length + 1);
      fContentBufferOffset = 0;
    }

    UInt32 theLen = 0;
    // ��ȡHTTP Content��������
    theErr = fInputStream.Read(fContentBuffer + fContentBufferOffset,
                               content_length - fContentBufferOffset,
                               &theLen);
    Assert(theErr != QTSS_BadArgument);

    if (theErr == QTSS_RequestFailed) {
      OSCharArrayDeleter charArrayPathDeleter(fContentBuffer);
      fContentBufferOffset = 0;
      fContentBuffer = nullptr;

      return QTSS_RequestFailed;
    }

    qtss_printf("EasyCMSSession::ProcessMessage() Add Len:%d \n", theLen);
    if ((theErr == QTSS_WouldBlock)
        || (theLen < (content_length - fContentBufferOffset))) {
      //
      // Update our offset in the buffer
      fContentBufferOffset += theLen;

      Assert(theErr == QTSS_NoErr);
      return QTSS_WouldBlock;
    }

    Assert(theErr == QTSS_NoErr);

    // ������ɱ��ĺ���Զ�����Delete����
    OSCharArrayDeleter charArrayPathDeleter(fContentBuffer);

    qtss_printf("EasyCMSSession::ProcessMessage() Get Complete Msg:\n%s",
                fContentBuffer);

    EasyProtocol protocol(fContentBuffer);
    int nNetMsg = protocol.GetMessageType();
    switch (nNetMsg) {
      case MSG_SC_FREE_STREAM_ACK: {
        string strErrorNum = protocol.GetHeaderValue(EASY_TAG_ERROR_NUM);
        string strSerial = protocol.GetBodyValue(EASY_TAG_SERIAL);
        string strChannle = protocol.GetBodyValue(EASY_TAG_CHANNEL);

        qtss_printf("EasyCMSֹͣ������Ӧ:%s,Serial=%s,Channel=%s",
                    strErrorNum.c_str(),
                    strSerial.c_str(),
                    strChannle.c_str());
        fLiveSession = false;//��������
      }
        break;
      default: break;
    }
  }

  // ����fContentBuffer��fContentBufferOffsetֵ
  fContentBufferOffset = 0;
  fContentBuffer = nullptr;

  return QTSS_NoErr;
}

QTSS_Error EasyCMSSession::CSFreeStream() {
  EasyDarwin::Protocol::EasyProtocolACK req(MSG_CS_FREE_STREAM_REQ);
  EasyJsonValue header, body;

  header[EASY_TAG_CSEQ] = 1;
  header[EASY_TAG_VERSION] = EASY_PROTOCOL_VERSION;
  header[EASY_TAG_APP_TYPE] =
      EasyProtocol::GetAppTypeString(EASY_APP_TYPE_EASYDARWIN);

#ifdef __Win32__
  header[EASY_TAG_TERMINAL_TYPE] = EasyProtocol::GetTerminalTypeString(EASY_TERMINAL_TYPE_WINDOWS);
#elif defined __linux__
  header[EASY_TAG_TERMINAL_TYPE] =
      EasyProtocol::GetTerminalTypeString(EASY_TERMINAL_TYPE_LINUX);
#elif defined __MacOSX__
  header[EASY_TAG_TERMINAL_TYPE] = EasyProtocol::GetTerminalTypeString(EASY_TERMINAL_TYPE_IOS);
#endif

  body[EASY_TAG_SERIAL] = fStreamName;
  body[EASY_TAG_CHANNEL] = to_string(fChannelNum);
  //body[EASY_TAG_PROTOCOL] = EasyProtocol::GetProtocolString(EASY_PROTOCOL_TYPE_RTSP);
  //body[EASY_TAG_RESERVE] = "1";

  req.SetHead(header);
  req.SetBody(body);

  auto msg = req.GetMsg();

  StrPtrLen jsonContent(const_cast<char *>(msg.data()));

  HTTPRequest httpReq(&QTSServerInterface::GetServerHeader(), httpRequestType);

  if (!httpReq.CreateRequestHeader()) return QTSS_Unimplemented;

  if (jsonContent.Len)
    httpReq.AppendContentLengthHeader(jsonContent.Len);

  char respHeader[2048] = {0};
  StrPtrLen *ackPtr = httpReq.GetCompleteHTTPHeader();
  strncpy(respHeader, ackPtr->Ptr, ackPtr->Len);

  fOutputStream.Put(respHeader);
  if (jsonContent.Len > 0)
    fOutputStream.Put(jsonContent.Ptr, jsonContent.Len);

  return QTSS_NoErr;
}

QTSS_Error EasyCMSSession::FreeStream(const char *streamName,
                                      UInt32 streamChannel) {
  QTSS_Error theErr = QTSS_NoErr;

  do {
    //1.��������
    fEasyMsgType = MSG_CS_FREE_STREAM_REQ;

    if (strlen(streamName) == 0) {
      theErr = QTSS_BadArgument;
      break;
    }
    fStreamName = new char[strlen(streamName) + 1];
    strcpy(fStreamName, streamName);

    fChannelNum = streamChannel;

    //2.����Serial��ѯ���豸���ڵ�EasyCMS��Ϣ
    char chCMSIP[20] = {0};
    char chCMSPort[6] = {0};
    UInt16 uCMSPort;

    //if(!QTSServerInterface::GetServer()->RedisGetAssociatedCMS(chSerial,chCMSIP,uCMSPort))

    QTSS_RoleParams theParams;
    theParams.GetAssociatedCMSParams.outCMSIP = chCMSIP;
    theParams.GetAssociatedCMSParams.outCMSPort = chCMSPort;
    theParams.GetAssociatedCMSParams.inSerial = fStreamName;
    auto numModules =
        QTSServerInterface::GetNumModulesInRole(QTSSModule::kRedisGetAssociatedCMSRole);
    for (UInt32 currentModule = 0; currentModule < numModules;
         currentModule++) {
      auto theModule =
          QTSServerInterface::GetModule(QTSSModule::kRedisGetAssociatedCMSRole,
                                        currentModule);
      (void) theModule->CallDispatch(Easy_RedisGetAssociatedCMS_Role,
                                     &theParams);
    }

    if (chCMSIP[0] == 0) {
      qtss_printf("��ȡ����CMSʧ��\n");
      return QTSS_ValueNotFound;
    } else {
      uCMSPort = atoi(chCMSPort);
      qtss_printf("��ȡ����CMS�ɹ�\n");
    }
    //TODO::
    //�����ѯʧ�ܣ�����QTSS_ValueNotFound


    //3.��ȡ��EasyCMS��������Socket��ʼ��
    auto inAddr = SocketUtils::ConvertStringToAddr(chCMSIP);
    if (inAddr) {
      fSocket->Set(inAddr, uCMSPort);
    } else {
      theErr = QTSS_IllegalService;
      break;
    }

  } while (false);

  return theErr;
}