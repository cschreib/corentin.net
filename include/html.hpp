#ifndef HTML_HPP
#define HTML_HPP

#include <crow/http_response.h>
#include <crow/json.h>
#include <string>

void           set_html_header(crow::response& response);
crow::response serve_file(const std::string& filename);
crow::response serve_css(const std::string& filename);
crow::response serve_markdown(const std::string& filename, const crow::json::wvalue& ctx = {});

#endif
