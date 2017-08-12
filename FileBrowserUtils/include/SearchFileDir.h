#ifndef  _SearchFileDir_H_
#define  _SearchFileDir_H_

#include <string>
#include <vector>
#ifdef _WIN32
#include <io.h>
#else
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>

#endif // CC_PLATFORM_ANDROID == CC_TARGET_PLATFORM
using namespace std;
enum FileType {
  File_None,//δ֪����
  File_Text,//�ı��ļ�
  File_Dir,//�ļ���
};

struct FileAttributeInfo {
  string fileName;//�ļ�����
  FileType type;//�ļ�����
  struct stat file_info;
  FileAttributeInfo() {
    fileName = "";
    type = File_None;

    file_info.st_size = 0;
  }
  FileAttributeInfo(const FileAttributeInfo &_in) {
    this->fileName = _in.fileName;
    this->type = _in.type;
    this->file_info = _in.file_info;
  }
  const FileAttributeInfo &operator=(const FileAttributeInfo &_in) {
    this->fileName = _in.fileName;
    this->type = _in.type;
    this->file_info = _in.file_info;
    return *this;
  }
};

class SearchFileDir {
 public:
  SearchFileDir();
  ~SearchFileDir();
  static SearchFileDir *getInstance();

  void initRootPath();
  static SearchFileDir *m_sSearchFileDir;

  bool searchFileList(const string &dirName,
                      vector<FileAttributeInfo> &list,
                      vector<string> machList,
                      bool onlyDir = false);

  bool isRootPath(string path);

  string getRealPath(string path);//ȥ��������һ������ǰĿ¼���ַ�

  const vector<FileAttributeInfo> *getRootPath();

  bool isDir(string path);

  bool find_Last_mach(string str, string mach);

  void getRoot();

  bool isHaveThisFile(const string &file);

  void buildPath(const string &path);

  void clearPath(const string &path);

  long getPathSize(const string &path);
 private:
  int getSeparatorPos(string path);
  int getLastSeparatorPos(string path);

  string cmd_system(string cmd);
 private:
  vector<FileAttributeInfo> _rootPathList;
};
#endif // _SearchFileDir_H_
