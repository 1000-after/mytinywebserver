#ifndef MIME_TYPES_H
#define MIME_TYPES_H

#include <string>

namespace mime_types{
    //根据文件名获取MIME类型
    std::string get_mime_type(const std::string& filename);
}

#endif  // MIME_TYPES_H