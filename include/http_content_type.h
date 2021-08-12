/**
 * @ Author: WangYusong
 * @ E-Mail: admin@wangyusong.cn
 * @ Create Time  : 2021-08-12 21:03:14
 * @ Modified Time: 2021-08-12 21:43:41
 * @ Description  : 记录http content-type文件类型
 */

#include <unordered_map> 

/* 定义文件类型 */

std::unordered_map<std::string, std::string> file_type_map = {
    {".js", "application/javascript"},
    {".xhtml", "application/xhtml+xml"},
    {".rtf", "application/rtf"},
    {".pdf", "application/pdf"},
    {".exe", "application/x-msdownload"},
    {".word", "application/msword"},
    {".zip", "application/zip"},
    {".gzip","application/gzip"},
    {".gz", "application/x-gzip"},
    {".tar", "application/x-tar"},

    {".png", "image/png"},
    {".gif", "image/gif"},
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".ico", "image/x-icon"},
    {".tif", "	image/tiff"},
    
    {".mp3", "audio/mp3"}, 
    {".wav", "audio/wav"},
    {".m3u", "audio/mpegurl"},

    {".mpeg", "video/mpeg"},
    {".mpg", "video/mpeg"},
    {".avi", "video/x-msvideo"},
    {".mp4", "video/mp4"},
    {".movie", "video/x-sgi-movie"},
    
    {".css", "text/css"},
    {".csv", "text/csv"},
    {".txt", "text/plain"},
    {".html", "text/html"},
    {".xml", "text/xml"},
    {"", "text/plain"},
    {".c", "text/plain"},
    {".cpp", "text/plain"},
    {".h", "text/plain"},
    {".hpp", "text/plain"},
    {".md", "text/plain"},
    {"default","text/plain"},

    {".class", "java/*"},
    {".java", "java/*"}
};