#include "markdown.hpp"

#include <iostream>
#include <md4c-html.h>
#include <sstream>

std::string markdown2html(const std::string& md) {
    std::stringstream str;

    auto process = [](const char* html, unsigned int size, void* userdata) {
        *static_cast<std::stringstream*>(userdata) << std::string_view(html, html + size);
    };

    const unsigned parse_flags = MD_FLAG_COLLAPSEWHITESPACE | MD_FLAG_TABLES | MD_FLAG_TASKLISTS |
                                 MD_FLAG_STRIKETHROUGH | MD_FLAG_PERMISSIVEURLAUTOLINKS |
                                 MD_FLAG_UNDERLINE | MD_FLAG_NOINDENTEDCODEBLOCKS;

    const unsigned render_flags = MD_HTML_FLAG_DEBUG | MD_HTML_FLAG_SKIP_UTF8_BOM;

    const int ret = md_html(md.data(), md.size(), process, &str, parse_flags, render_flags);

    if (ret != 0) {
        std::cerr << "parse error reading " << md << std::endl;
        return "";
    }

    return str.str();
}
