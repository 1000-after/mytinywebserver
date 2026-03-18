#include "mime_types.h"
#include <string>
#include <map>
#include <algorithm>

using namespace std;

namespace mime_types{
    //MIME类型映射表
    static const map<string, string> mime_map = {
        //文本文件
        {"html", "text/html"},
        {"htm", "text/html"},
        {"css", "text/css"},
        {"js", "application/javascript"},
        {"json", "application/json"},
        {"xml", "application/xml"},
        {"txt", "text/plain"},
        {"csv", "text/csv"},

        //图片文件
        {"jpg", "image/jpeg"},
        {"jpeg", "image/jpeg"},
        {"png", "image/png"},
        {"gif", "image/gif"},
        {"bmp", "image/bmp"},
        {"ico", "image/x-icon"},
        {"svg", "image/svg+xml"},
        {"webp", "image/webp"},

        //字体文件
        {"woff", "font/woff"},
        {"woff2", "font/woff2"},
        {"ttf", "font/ttf"},
        {"eot", "application/vnd.ms-fontobject"},

        //应用程序文件
        {"pdf", "application/pdf"},
        {"zip", "application/zip"},
        {"rar", "application/x-rar-compressed"},
        {"7z", "application/x-7z-compressed"},
        {"exe", "application/x-msdownload"},

        //音视频文件
        {"mp3", "audio/mpeg"},
        {"mp4", "video/mp4"},
        {"avi", "video/x-msvideo"},
        {"mov", "video/quicktime"},
        {"wav", "audio/wav"},
        {"ogg", "audio/ogg"},
        {"webm", "video/webm"}
    };

         // 根据文件扩展名获取MIME类型
    string get_mime_type(const string& extension) {
        // 转换为小写（避免大小写问题，如.CSS → css）
        string ext = extension;
        transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        // 查找扩展名对应的MIME类型
        auto it = mime_map.find(ext);
        if (it != mime_map.end()) {
            return it->second;
        }
        // 默认返回text/plain（纯文本）
        return "text/plain";
    }
}