#include "FileCache.h"
#include <unordered_map>
#include <time.h>

using namespace std;

FileCache *FileCache::cache = nullptr;

typedef struct FileCache_Tag {
  time_t date;
  string context;
} FileCacheEntity;

static unordered_map<string, FileCacheEntity> sFileCacheStore;

FileCache *FileCache::GetInstance() {
  if (cache == nullptr) {
    cache = new FileCache();
  }
  return cache;
}

void FileCache::put(char *path, char *context) {
  if (path == nullptr || context == nullptr) {
    return;
  }
  FileCacheEntity cache = {
      .date = time(nullptr),
      .context = string(context)
  };
  sFileCacheStore[string(path)] = cache;
}

char *FileCache::get(char *path) {
  auto it = sFileCacheStore.find(string(path));
  if (it == sFileCacheStore.end()) {
    return nullptr;
  }
  return (char *) it->second.context.c_str();
}

bool FileCache::remove(char *path) {
  auto it = sFileCacheStore.find(string(path));
  if (it != sFileCacheStore.end()) {
    sFileCacheStore.erase(it);
  }
  return true;
}

long FileCache::getModify(char *path) {
  auto it = sFileCacheStore.find(string(path));
  if (it == sFileCacheStore.end()) {
    return -1;
  }
  return it->second.date;
}

size_t FileCache::getLen(char *path) {
  auto it = sFileCacheStore.find(string(path));
  if (it == sFileCacheStore.end()) {
    return 0;
  }
  return it->second.context.size();
}
