/*
File:   C3PFile.cpp
Author: J. Ian Lindsay
Date:   2023.12.24

Copyright 2016 Manuvr, Inc

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.


Simple file handler for Linux.
*/

#include "../LinuxStorage.h"
#include "AbstractPlatform.h"

#if true
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#define FILE_BUFFER_SIZE 2048


C3PFile::C3PFile(char* p) {
  memset(_mode, 0, sizeof(_mode));
  char* trimmed = StringBuilder::trim(p);
  size_t path_len = strlen(trimmed);
  if (path_len) {
    _path = (char*) malloc(path_len+1);
    if (_path) {
      memcpy(_path, trimmed, path_len);
      *(_path + path_len) = '\0';
      _fill_from_stat();
    }
  }
}


C3PFile::~C3PFile() {
  if (_path) {
    free(_path);
    _path = nullptr;
  }
}



/*
* Function takes a path and a buffer as arguments. The number of bytes read is
*   returned on success. <0 is returned on failure.
*/
int32_t C3PFile::read(StringBuilder* buf) {
  int32_t return_value = -1;
  int fd = open(_path, O_RDONLY);
  if (fd >= 0) {
    ulong total_read = 0;
    uint8_t self_mass[FILE_BUFFER_SIZE];
    do {
      int r_len = ::read(fd, self_mass, FILE_BUFFER_SIZE);
      if ((r_len > 0) || (0 == _fsize)) {
        buf->concat(self_mass, r_len);
        total_read += r_len;
      }
      else {
        printf("Aborting read due to zero byte return. %s\n", _path);
        c3p_log(LOG_LEV_ERROR, __PRETTY_FUNCTION__, "Aborting read due to zero byte return. %s", _path);
        total_read = _fsize;
      }
    } while (total_read < _fsize);
    if (_fsize == total_read) {
      return_value = (int32_t) total_read;
    }
    else {
      c3p_log(LOG_LEV_ERROR, __PRETTY_FUNCTION__, "Failed to read the entire file %s. Read %u of %u bytes.", _path, total_read, _fsize);
    }
    close(fd);
  }
  else {
    c3p_log(LOG_LEV_ERROR, __PRETTY_FUNCTION__, "Failed to open path for reading: %s", _path);
  }
  return return_value;
}



int32_t C3PFile::write(StringBuilder* buf) {
  return -1;
}


/*
*
*/
int C3PFile::_fill_from_stat() {
  struct stat64 statbuf;
  memset((void*) &statbuf, 0, sizeof(struct stat64));
  int return_value = lstat64((const char*) _path, &statbuf);
  if (0 == return_value) {
    _is_dir  = S_ISDIR(statbuf.st_mode);
    _is_file = S_ISREG(statbuf.st_mode);
    _is_link = S_ISLNK(statbuf.st_mode);
    // S_ISCHR(m)
    // S_ISBLK(m)
    // S_ISFIFO(m)

    if (_is_link || _is_file || _is_dir) {
      _uid = statbuf.st_uid;
      _gid = statbuf.st_gid;
      _mtime = statbuf.st_mtime;
      _ctime = statbuf.st_ctime;
      _exists = true;

      _mode[0] = (statbuf.st_mode & S_IRUSR) ? 'r' : '-';
      _mode[1] = (statbuf.st_mode & S_IWUSR) ? 'w' : '-';
      _mode[2] = (statbuf.st_mode & S_IXUSR) ? 'x' : '-';
      _mode[3] = (statbuf.st_mode & S_IRGRP) ? 'r' : '-';
      _mode[4] = (statbuf.st_mode & S_IWGRP) ? 'w' : '-';
      _mode[5] = (statbuf.st_mode & S_IXGRP) ? 'x' : '-';
      _mode[6] = (statbuf.st_mode & S_IROTH) ? 'r' : '-';
      _mode[7] = (statbuf.st_mode & S_IWOTH) ? 'w' : '-';
      _mode[8] = (statbuf.st_mode & S_IXOTH) ? 'x' : '-';

      if (_is_file) {
        _fsize = statbuf.st_size;
        //c3p_log(LOG_LEV_INFO, "Path is a file with size %lu: %s", _fsize, _path);
      }
      else if (_is_dir) {
        //c3p_log(LOG_LEV_INFO, "Path is a directory: %s", _path);
      }
      else {
        //c3p_log(LOG_LEV_INFO, "Path is a link: %s", _path);
      }
    }
    else {
      // Some unhandled filesystem object.
      c3p_log(LOG_LEV_WARN, __PRETTY_FUNCTION__, "Unhandled filesystem object at path: %s", _path);
    }
  }
  else {
    perror("stat");
    c3p_log(LOG_LEV_ERROR, __PRETTY_FUNCTION__, "Failed to lstat path: %s", _path);
  }

  return return_value;
}


#endif   // CONFIG_C3P_STORAGE
